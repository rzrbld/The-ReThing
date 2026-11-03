#!/usr/bin/env python3
"""Stage byte-identical MainUI ZIP members as loose original-game resources.

The original archive remains in the package. Only archive-defined Textures,
Sound and Levels members are staged; configuration and user data are excluded.
"""
import hashlib,json,re,stat,zipfile
from pathlib import Path

DIRECTORIES={'textures':'Textures','sound':'Sound','levels':'Levels'}

def checked_member(info):
    name=info.filename
    parts=name.rstrip('/').split('/')
    mode=info.external_attr>>16
    if stat.S_ISLNK(mode) or stat.S_IFMT(mode) not in (0,stat.S_IFREG,stat.S_IFDIR):
        raise ValueError('Nonregular MainUI ZIP entry: '+name)
    if not parts or parts[0].casefold() not in DIRECTORIES or any(not re.fullmatch(r'[A-Za-z0-9_.-]+',p) or p in ('.','..') for p in parts):
        raise ValueError('Unsafe/unsupported MainUI ZIP path: '+name)
    return Path(DIRECTORIES[parts[0].casefold()],*parts[1:])

def stage_mainui(archive,data_root,staging):
    archive,data_root,staging=map(Path,(archive,data_root,staging))
    if staging.resolve().is_relative_to(data_root.resolve()) or data_root.resolve().is_relative_to(staging.resolve()):
        raise ValueError('MainUI staging must be separate from original data')
    existing={}
    for path in data_root.rglob('*'):
        key=str(path.relative_to(data_root)).casefold()
        if key in existing and path!=existing[key]:
            raise ValueError('Case-insensitive loose data collision: '+key)
        existing[key]=path
    planned=[];seen=set();overlaps=[]
    # Validate every path/collision before writing any staging files.
    with zipfile.ZipFile(archive) as source:
        for info in source.infolist():
            relative=checked_member(info);key=str(relative).casefold()
            if info.is_dir():continue
            if key in seen:raise ValueError('Case-insensitive MainUI ZIP collision: '+str(relative))
            seen.add(key);data=source.read(info)
            for parent in relative.parents:
                if str(parent)=='.':continue
                found=existing.get(str(parent).casefold())
                if found is not None and not found.is_dir():raise ValueError('Loose file blocks MainUI directory: '+str(parent))
            found=existing.get(key)
            if found is not None:
                if not found.is_file() or found.read_bytes()!=data:raise ValueError('Differing MainUI/loose data collision: '+str(relative))
                overlaps.append(str(relative))
            planned.append((info.filename,relative,data))
    archive_overlaps=[]
    mainui={str(relative).casefold():(member,data) for member,relative,data in planned}
    for name in ('resources.pak','english.pak'):
        other=data_root/name
        if not other.exists():continue
        with zipfile.ZipFile(other) as source:
            for info in source.infolist():
                matched=mainui.get(info.filename.casefold())
                if matched is None or info.is_dir():continue
                checked_member(info)
                if source.read(info)!=matched[1]:raise ValueError('Differing MainUI/archive collision: '+name+':'+info.filename)
                archive_overlaps.append(dict(archive=name,archive_member=info.filename,mainui_member=matched[0],precedence='Identical bytes; loose MainUI copy does not change content'))
    inputs=[];assets=[]
    for member,relative,data in planned:
        destination=staging/'data'/relative
        # This is a dedicated generated staging tree, never an active game path.
        if destination.is_symlink():raise ValueError('Symlink in MainUI staging tree: '+str(destination))
        if any(p.is_symlink() for p in destination.parents):
            raise ValueError('Symlink parent in MainUI staging tree: '+str(destination))
        destination.parent.mkdir(parents=True,exist_ok=True)
        destination.write_bytes(data)
        name='the-thing/data/'+relative.as_posix();inputs.append((destination,name))
        assets.append(dict(path=name,archive_member=member,size=len(data),sha256=hashlib.sha256(data).hexdigest()))
    manifest=staging/'original-mainui-assets.json'
    manifest.write_text(json.dumps(dict(source_archive='data/mainui.pak',source_archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),purpose='Byte-identical original resources available through the game loose-file VFS; original archive retained',identical_existing_loose=overlaps,identical_other_archive=archive_overlaps,assets=assets),indent=2)+'\n')
    notice=staging/'original-mainui-assets.txt'
    notice.write_text('Original game resource provenance\n\nThese files are copied byte-for-byte from the supplied game data/mainui.pak.\nThe archive is retained unchanged. The original game assets remain covered\nby their original rights; this port grants no new license to those assets.\nSee original-mainui-assets.json for archive-member identities and hashes.\n')
    inputs.extend([(manifest,'the-thing/original-mainui-assets.json'),(notice,'the-thing/licenses/original-mainui-assets.txt')])
    return inputs

def merge_inputs(inputs,additional):
    result=list(inputs);names={name.casefold():(path,name) for path,name in inputs}
    if len(names)!=len(inputs):raise ValueError('Case-insensitive duplicate package paths')
    for path,name in additional:
        previous=names.get(name.casefold())
        if previous:
            if previous[0].read_bytes()!=path.read_bytes():raise ValueError('Differing package path collision: '+name)
            continue
        names[name.casefold()]=(path,name);result.append((path,name))
    return result
