#!/usr/bin/env python3
"""Installable PortMaster layout. No catalogue submission or user-folder migration."""
import argparse,hashlib,json,subprocess,zipfile
from pathlib import Path
R=Path(__file__).resolve().parents[1]
sha=lambda b:hashlib.sha256(b).hexdigest()
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--game',type=Path,default=R/'build/game/thething.aarch64')
parser.add_argument('--audio',type=Path,default=R/'build/native-vorbis-arm/probe.aarch64.so')
parser.add_argument('--output',type=Path,default=R/'dist/the_rething.zip')
args=parser.parse_args()
files={}
def add(name,path): files[name]=Path(path).read_bytes()
add('The ReThing.sh',R/'packaging/The ReThing.sh')
for name in ('the-thing-env.sh','cpu-session.sh','muos-session.sh'):
 add('the-thing/'+name,R/'packaging'/name)
add('the-thing/run-game.sh',R/'packaging/run-game.sh')
add('the-thing/thething.aarch64',args.game)
add('the-thing/libthing-audio.aarch64.so',args.audio)
add('the-thing/controller-buttons.cfg',R/'config/controller-buttons.cfg')
add('the-thing/CONTROLS.md',R/'docs/CONTROLS.md')
add('the-thing/RENDER_OPTIONS.md',R/'docs/RENDER_OPTIONS.md')
add('the-thing/BUILDING.md',R/'BUILDING.md')
files['the-thing/PREBUILT-FOR-REVIEW.txt']=b'The prebuilt executable is provided for review. Users must supply their own game files. See README.md and THIRD_PARTY.md for rights and license notices.\n'
files['the-thing/source/PLACE-YOUR-ISO-HERE.txt']=b'Put your supported original ISO here as the_thing.iso. Existing supported bin/ and data/ are also accepted. See README.md.\n'
for name,path in (
 ('LICENSE.TheReThing.txt','LICENSE'),('THIRD_PARTY.md','THIRD_PARTY.md'),
 ('LICENSE.pcrecomp.txt','third_party/pcrecomp/LICENSE'),
 ('NOTICE.pcrecomp.txt','third_party/pcrecomp/NOTICE'),
 ('LICENSE.controller-source.txt','probe/LICENSE.desert-pad-source'),
 ('LICENSE.ReConflict.txt','third_party/reconflict/LICENSE'),
 ('pl_mpeg.h','third_party/pl_mpeg/pl_mpeg.h'),
 ('LICENSE.ogg','build/native-vorbis-arm/LICENSE.ogg'),
 ('LICENSE.vorbis','build/native-vorbis-arm/LICENSE.vorbis')):
 add('the-thing/licenses/'+name,R/path)
add('the-thing/THIRD_PARTY.md',R/'THIRD_PARTY.md')
for path in sorted((R/'third_party/pcrecomp/LICENSES').glob('*')):
 if path.is_file():add('the-thing/licenses/'+path.name,path)
for path in sorted((R/'build/installer-tools/bundle').rglob('*')):
 if path.is_file():
  rel=str(path.relative_to(R/'build/installer-tools/bundle'));add('the-thing/tools/'+rel,path)
  # PortMaster keeps every license under the port's licenses/ folder.
  if rel.startswith('licenses/'):add('the-thing/licenses/installer-tools/'+rel[len('licenses/'):],path)
for name in ('thething.aarch64','libthing-audio.aarch64.so'):
 blob=files['the-thing/'+name]
 assert blob[:6]==b'\x7fELF\x02\x01' and int.from_bytes(blob[18:20],'little')==183,name
files['the-thing/the-thing-env.sh']=files['the-thing/the-thing-env.sh'].replace(b'THING_PERF_PROBE:-1',b'THING_PERF_PROBE:-0')
files['the-thing/thething-launcher']=(R/'build/launcher/thething-launcher.aarch64').read_bytes()
files['the-thing/libs.aarch64/libthing-audio.aarch64.so']=files.pop('the-thing/libthing-audio.aarch64.so')
run=files['the-thing/run-game.sh'].decode().replace('$gamedir/libthing-audio.aarch64.so','$gamedir/libs.aarch64/libthing-audio.aarch64.so')
files['the-thing/run-game.sh']=run.encode()
# ZIP extraction can drop executable bits. Restore only the port executables.
entry=files['The ReThing.sh'].decode()
entry=entry.replace('source "$gamedir/the-thing-env.sh" || exit 1', '''source "$gamedir/the-thing-env.sh" || exit 1
if [[ ${DEVICE_ARCH:-$(uname -m)} != aarch64 ]]; then
    echo "The ReThing requires AArch64 firmware." >&2
    exit 1
fi
export CONFDIR="$gamedir/config"
${ESUDO:-} chmod +x "$gamedir/thething-launcher" "$gamedir/thething.aarch64" || exit 1
if [[ -f "$gamedir/tools/lib/ld-linux-aarch64.so.1" ]]; then
    ${ESUDO:-} chmod +x "$gamedir/tools/lib/ld-linux-aarch64.so.1" || exit 1
fi''')
files['The ReThing.sh']=entry.encode()
for n in ('port.json','README.md','gameinfo.xml'):files['the-thing/'+n]=(R/'packaging/portmaster'/n).read_bytes()
# Match PortMaster's release-builder documentation placement too.
files['the-thing/the_rething.md']=files['the-thing/README.md']
files['the-thing/screenshot.png']=(R/'packaging/portmaster/screenshot.png').read_bytes()
# Use one production installation guide; omit historical measurement instructions.
files['the-thing/FIRST_RUN_INSTALL.txt']=files['the-thing/README.md']
# Canonical PortMaster identifier: lowercase letters and underscore, no hyphen.
# Internal helper filenames are retained; only the installed folder is renamed.
renamed={}
for n,b in files.items():
 target=n.replace('the-thing/','the_rething/',1).replace('The Thing Measure.sh','The ReThing Measure.sh')
 if '/licenses/' not in n and '/tools/' not in n and n.endswith(('.sh','.txt','.md','.xml')) and n!='the-thing/README.md' and n!='the-thing/the_rething.md' and n!='the-thing/FIRST_RUN_INSTALL.txt':
  text=b.decode()
  text=text.replace('the-thing-env.sh','__ENV_HELPER__')
  text=text.replace('the-thing','the_rething').replace('The Thing Measure','The ReThing Measure')
  text=text.replace('__ENV_HELPER__','the-thing-env.sh')
  b=text.encode()
 renamed[target]=b
files=renamed
# Include the documented template files beside the launch script. Keep installed
# metadata inside the payload as well for PortMaster's runtime discovery.
for name in ('port.json','README.md','gameinfo.xml','screenshot.png'):
 files[name]=files['the_rething/'+name]
for n,b in files.items():
 if n.endswith('.sh'):subprocess.run(['bash','-n'],input=b,check=True)
 assert not n.startswith(('ports/','Roms/')) and not n.endswith(('.iso','.sav','.py'))
files['the_rething/MANIFEST.json']=json.dumps({'format':'manual PortMaster-compatible install archive','extract_into':'ports','files':{n:{'size':len(b),'sha256':sha(b)} for n,b in sorted(files.items())}},indent=2).encode()
out=args.output
out.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for n,b in sorted(files.items()):
  i=zipfile.ZipInfo(n,(2026,11,3,0,0,0));i.create_system=3
  executable=n.endswith(('.sh','.aarch64','.so','thething-launcher','/7z','/unshield','/xdelta','/ld-linux-aarch64.so.1'))
  i.external_attr=(0o100755 if executable else 0o100644)<<16;i.compress_type=zipfile.ZIP_DEFLATED;z.writestr(i,b)
with zipfile.ZipFile(out) as z:assert z.testzip() is None
out.with_suffix('.zip.sha256').write_text(sha(out.read_bytes())+'  '+out.name+'\n')
print(out,out.stat().st_size)
