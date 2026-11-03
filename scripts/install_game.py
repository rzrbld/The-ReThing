#!/usr/bin/env python3
"""Offline, transactional first-run installer. All writes stay under --root."""
import argparse, fcntl, hashlib, json, os, shutil, subprocess, sys, traceback
from pathlib import Path
from stage_mainui import stage_mainui

ISO_SHA = '1f3ac2916d8185f0e76b811fe24bc15af9412bf4da18f4bf4579ee483683fe61'
EXE_SHA = '085d1a90b2bd57967e8e911eb1f7c2232fd4b045b7f363c5c4a3056f720abb49'

def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024*1024), b''): h.update(block)
    return h.hexdigest()

def status(root, percent, message):
    p = root/'tmp/install-status.new'
    p.write_text('%d\n%s\n' % (percent, message))
    p.replace(root/'tmp/install-status.txt')

def verify(stage, manifest):
    for item in manifest['files']:
        p = stage/item['path']
        if not p.is_file() or digest(p) != item['sha256']:
            raise RuntimeError('Payload checksum mismatch: '+item['path'])

def existing_path(root, relative):
    """Windows payload names are case insensitive, including older port installs."""
    path = root
    for part in Path(relative).parts:
        matches = [p for p in path.iterdir() if p.name.casefold() == part.casefold()] if path.is_dir() else []
        if len(matches) != 1:
            raise RuntimeError('Missing or ambiguous installed file: ' + relative)
        path = matches[0]
    return path

def adopt_existing(root, manifest, marker):
    status(root, -1, 'VERIFYING EXISTING GAME FILES')
    verified = []
    for item in manifest['files']:
        path = existing_path(root, item['path'])
        if not path.is_file() or digest(path) != item['sha256']:
            raise RuntimeError('Existing game checksum mismatch: ' + item['path'])
        verified.append(path)
    # Older packages lack the installer receipt. Recover loose menu resources
    # from the verified archive; never replace differing user files.
    ui = root/'tmp/adopt-mainui'
    if ui.exists(): shutil.rmtree(ui)
    archive = existing_path(root, 'data/mainui.pak')
    data = existing_path(root, 'data')
    stage_mainui(archive, data, ui)
    for staged in (ui/'data').rglob('*'):
        if not staged.is_file(): continue
        relative = str(staged.relative_to(ui/'data'))
        try: destination = existing_path(data, relative)
        except RuntimeError: destination = data/relative
        if not destination.exists():
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(staged, destination)
        verified.append(destination)
    shutil.copy2(ui/'original-mainui-assets.json', root/'config/original-mainui-assets.json')
    receipt = {'iso_sha256': ISO_SHA, 'adopted_existing': True,
               'files': [{'path':str(p.relative_to(root)), 'size':p.stat().st_size} for p in verified]}
    temp = marker.with_suffix('.new'); temp.write_text(json.dumps(receipt)); temp.replace(marker)
    shutil.rmtree(ui)
    status(root, 100, 'READY')

def install(root, host_tools=False):
    # Reject symlinked writable trees, including existing nested links, before any writes.
    for name in ('tmp','config','userdata','cache','saves','source','extracted-iso','bin','data'):
        p=root/name
        if p.is_symlink() or (p.exists() and any(q.is_symlink() for q in p.rglob('*'))):
            raise RuntimeError('Symlink in installation tree: '+name)
    for name in ('tmp','config','userdata','cache','saves','source'): (root/name).mkdir(exist_ok=True)
    with (root/'tmp/install.lock').open('w') as lock:
        try: fcntl.flock(lock, fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError: raise RuntimeError('Another installer is already running')
        manifest=json.loads((root/'installer/patched-v12.json').read_text())
        marker=root/'config/install-v12.json'
        journal=root/'tmp/install-publishing.json'
        work=root/'tmp/install-work'
        # Recover an interrupted publication. Never infer ownership from directory names alone.
        if journal.exists():
            for name in ('bin','data'):
                backup=work/('previous-'+name)
                if backup.exists():
                    if (root/name).exists(): shutil.rmtree(root/name)
                    backup.rename(root/name)
                elif not json.loads(journal.read_text())[name] and (root/name).exists():
                    shutil.rmtree(root/name)
            marker.unlink(missing_ok=True)
            journal.unlink()
        if marker.exists():
            try: installed=json.loads(marker.read_text())
            except (ValueError, OSError): installed={}
            if installed.get('iso_sha256')==ISO_SHA and installed.get('files') and all((root/i['path']).is_file() and (root/i['path']).stat().st_size==i['size'] for i in installed['files']):
                status(root,100,'READY'); return
        if (root/'bin').is_dir() and (root/'data').is_dir():
            try:
                adopt_existing(root, manifest, marker)
                return
            except (RuntimeError, ValueError) as error:
                print('Existing installation could not be adopted:', error, flush=True)
                if not (root/'source/the_thing.iso').is_file():
                    raise RuntimeError(str(error) + '; see tmp/install.log') from error
        status(root,-1,'VERIFYING ORIGINAL ISO')
        iso=root/'source/the_thing.iso'
        if not iso.is_file(): raise RuntimeError('Missing source/the_thing.iso')
        if digest(iso)!=ISO_SHA: raise RuntimeError('Wrong ISO - checksum does not match')
        if shutil.disk_usage(root).free < 1500*1024*1024:
            raise RuntimeError('Need 1500 MB free for installation')
        if work.exists(): shutil.rmtree(work)
        work.mkdir()
        env=os.environ.copy();env['TMPDIR']=str(root/'tmp');env['LC_ALL']='C'
        def run(tool, *args):
            if host_tools:
                tools={'7z':['7z'], 'unshield':[str(Path.cwd()/'build/tools/unshield/usr/bin/unshield')], 'xdelta':[str(Path.cwd()/'build/tools/xdelta/usr/bin/xdelta')]}
                command=tools[tool]
                lib=Path(command[0]).parent.parent/'lib/x86_64-linux-gnu'
                child_env=dict(env, LD_LIBRARY_PATH=str(lib)) if tool!='7z' else env
            else:
                base=root/'tools'; child_env=env.copy(); child_env.pop('LD_LIBRARY_PATH',None)
                command=[str(base/'lib/ld-linux-aarch64.so.1'),'--library-path',str(base/'lib'),str(base/('p7zip/7z' if tool=='7z' else 'bin/'+tool))]
            with (root/'tmp/install-tools.log').open('a') as log:
                log.write('\n'+repr(command+list(map(str,args)))+'\n');log.flush()
                subprocess.run(command+list(map(str,args)),env=child_env,stdout=log,stderr=subprocess.STDOUT,check=True)
        status(root,5,'EXTRACTING ISO')
        iso_tree=work/'iso'
        run('7z','x','-y',iso,'-o'+str(iso_tree))
        status(root,25,'INSTALLING ORIGINAL GAME')
        payload=work/'payload';run('unshield','-d',payload,'x',iso_tree/'data1.cab')
        stage=work/'game'
        shutil.copytree(payload/'US_bin/bin',stage/'bin')
        for component in ('levels','English_data','FMV','Default_language_resources','Global_data'):
            shutil.copytree(payload/component/'data',stage/'data',dirs_exist_ok=True)
        targets=[('thething.exe','bin/thething.exe'),('english.pak','data/english.pak'),('resources.pak','data/resources.pak'),('mainui.pak','data/mainui.pak'),('level5a.dll','data/Levels/level5a.dll'),('level6.dll','data/Levels/level6.dll')]
        for percent,version,filename,updates in [(50,'1.1','thething_patch_10_11.exe',targets),(65,'1.2','thething_patch_11_12.exe',[('TheThing.exe','bin/thething.exe')])]:
            status(root,percent,'APPLYING PATCH '+version)
            patch=work/('patch'+version)
            run('7z','x','-y',iso_tree/'UPDATE'/('Version '+version+' Eng')/filename,'-o'+str(patch))
            for delta,target in updates:
                old=stage/target;new=old.with_suffix('.patched')
                run('xdelta','patch',patch/'Disk1'/delta,old,new);new.replace(old)
        replacement=iso_tree/'UPDATE/Version 1.2 Eng/CRACK/TheThing.exe'
        if digest(replacement)!=EXE_SHA: raise RuntimeError('Executable checksum mismatch')
        shutil.copy2(replacement,stage/'bin/thething.exe')
        status(root,80,'VERIFYING PATCHED GAME');verify(stage,manifest)
        status(root,90,'PREPARING MENU RESOURCES')
        ui=work/'ui';stage_mainui(stage/'data/mainui.pak',stage/'data',ui)
        shutil.copytree(ui/'data',stage/'data',dirs_exist_ok=True)
        installed={'iso_sha256':ISO_SHA,'files':[{'path':str(p.relative_to(stage)),'size':p.stat().st_size} for p in sorted(stage.rglob('*')) if p.is_file()]}
        status(root,95,'FINISHING INSTALLATION')
        # Journal is written before the first rename; rollback on the next invocation.
        journal.write_text(json.dumps({name:(root/name).exists() for name in ('bin','data')}))
        for name in ('bin','data'):
            if (root/name).exists(): (root/name).rename(work/('previous-'+name))
            (stage/name).rename(root/name)
        if (root/'extracted-iso').exists(): shutil.rmtree(root/'extracted-iso')
        iso_tree.rename(root/'extracted-iso')
        shutil.copy2(ui/'original-mainui-assets.json',root/'config/original-mainui-assets.json')
        temp=marker.with_suffix('.new');temp.write_text(json.dumps(installed));temp.replace(marker)
        journal.unlink() # transaction committed; old payload can now be discarded
        shutil.rmtree(work)
        status(root,100,'READY')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--host-tools',action='store_true',help='Developer test only: use local extraction tools')
    args=parser.parse_args();root=args.root.resolve()
    try: install(root,args.host_tools)
    except Exception as error:
        traceback.print_exc()
        if (root/'tmp').is_dir() and not (root/'tmp').is_symlink(): status(root,-1,str(error).upper()[:70])
        sys.exit(1)
