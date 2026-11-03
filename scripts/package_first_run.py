#!/usr/bin/env python3
"""Stage/zip the bring-your-own-game distribution; prebuilt executable for review."""
import argparse,hashlib,json,shutil,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/first-run-distro';port=out/'ports/the-thing'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
 return h.hexdigest()
def copy(src,dst):
 dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/src,dst)
def stage():
 if out.exists():shutil.rmtree(out)
 port.mkdir(parents=True)
 assert sha(ROOT/'build/game/thething.aarch64')=='da4157377000f9b34b6bdc2260b07e48e6c61d6658b063a9f43d8e0c429a8dbe','Unexpected game revision: review before packaging'
 copy('build/game/thething.aarch64',port/'thething.aarch64')
 copy('build/launcher/thething-launcher.aarch64',port/'thething-launcher')
 for name in ['the-thing-env.sh','cpu-session.sh','muos-session.sh']:copy('packaging/'+name,port/name)
 copy('packaging/The Thing.sh',port/'run-game.sh')
 for name in ['The ReThing.sh','The Thing Measure.sh','The Thing Measure Cache.sh']:copy('packaging/'+name,out/'Roms/PORTS'/name)
 for name in ['install_game.py','stage_mainui.py']:copy('scripts/'+name,port/'installer'/name)
 copy('docs/evidence/patched-v12.json',port/'installer/patched-v12.json')
 (port/'source').mkdir(parents=True)
 (port/'source/PLACE-YOUR-ISO-HERE.txt').write_text('Supply your own supported game disc image as the_thing.iso here. No original game files are included. See FIRST_RUN_INSTALL.txt for the currently supported revision.\n')
 (port/'PREBUILT-FOR-REVIEW.txt').write_text('The prebuilt ARM executable is provided for review. It is unchanged from the previous build. This label is not a grant of rights to the original game. The intended user workflow is to supply their own supported copy and generate the executable locally; see OWN_COPY_BUILD.txt.\n')
 copy('docs/OWN_COPY_BUILD.txt',port/'OWN_COPY_BUILD.txt')
 shutil.copytree(ROOT/'build/installer-tools/bundle',port/'tools')
 copy('config/controller-buttons.cfg',port/'controller-buttons.cfg')
 for name in ['FIRST_RUN_INSTALL.txt','PERFORMANCE_PAUSED.txt','CONTROLS.md']:copy('docs/'+name,port/name)
 copy('docs/FIRST_RUN_INSTALL.txt',out/'READ-ME.txt')
 copy('docs/evidence/first-run-installer/README.txt',port/'INSTALLER-VALIDATION.txt')
 copy('docs/evidence/first-run-installer/tests.txt',port/'INSTALLER-TESTS.txt')
 for src,name in [('third_party/pcrecomp/LICENSE','pcrecomp-LICENSE'),('third_party/pcrecomp/NOTICE','pcrecomp-NOTICE'),('third_party/pl_mpeg/pl_mpeg.h','pl_mpeg.h'),('probe/LICENSE.desert-pad-source','controller-source.txt')]:copy(src,port/'licenses'/name)
 for p in out.rglob('*.sh'):p.chmod(0o755)
 for p in [port/'thething.aarch64',port/'thething-launcher',*list((port/'tools/bin').iterdir()),port/'tools/p7zip/7z',port/'tools/lib/ld-linux-aarch64.so.1']:p.chmod(0o755)
 print('Staged',out)
def pack():
 for p in [port/'thething.aarch64',port/'thething-launcher']:
  b=p.read_bytes();assert b[:6]==b'\x7fELF\x02\x01' and int.from_bytes(b[18:20],'little')==183
 for forbidden in ('source/the_thing.iso','extracted-iso','bin','data'):
  assert not (port/forbidden).exists(), 'Original game input/payload must not be distributed: '+forbidden
 manifest={'distribution':'User supplies original files; prebuilt executable for review', 'status':'First-run installer locally validated; device installation/gameplay/performance unverified', 'game_revision':'Save/light/scale da415737; no SIMD/batching experiment promoted','files':[{'path':str(p.relative_to(out)), 'size':p.stat().st_size,'sha256':sha(p)} for p in sorted(out.rglob('*')) if p.is_file() and p.name!='MANIFEST.json']}
 (port/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
 target=ROOT/'dist/the-thing-own-copy-review-20261102.zip';target.parent.mkdir(exist_ok=True)
 tmp=target.with_suffix('.zip.tmp')
 with zipfile.ZipFile(tmp,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for p in sorted(out.rglob('*')):
   if p.is_file():z.write(p,p.relative_to(out))
 with zipfile.ZipFile(tmp) as z:assert z.testzip() is None
 tmp.replace(target);target.with_suffix('.zip.sha256').write_text(sha(target)+'  '+target.name+'\n')
 print(target, target.stat().st_size,sha(target))
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('action',choices=['stage','pack']);a=parser.parse_args()
 (stage if a.action=='stage' else pack)()
