#!/usr/bin/env python3
"""Package the already verified fixed binary, preserving its exact identity."""
from pathlib import Path
import hashlib,zipfile,json
root=Path(__file__).resolve().parents[1]
source=root/'dist/the-thing-own-copy-review-20261102.zip'
expected='15485549bb7b78aa10743b51fc5c060267f328b73a18b7477c13de36611bd400'
with zipfile.ZipFile(source) as old:
 game=old.read('ports/the-thing/thething.aarch64');ui=old.read('ports/the-thing/thething-launcher')
assert hashlib.sha256(game).hexdigest()==expected
files={'ports/the-thing/thething.aarch64':game,'ports/the-thing/thething-launcher':ui}
for name in ('The ReThing.sh','The Thing.sh'):
 files['ports/'+name]=(root/'packaging'/name).read_bytes()
for name in ('the-thing-env.sh','muos-session.sh','cpu-session.sh'):
 files['ports/the-thing/'+name]=(root/'packaging'/name).read_bytes()
files['ports/the-thing/run-game.sh']=(root/'packaging/The Thing.sh').read_bytes()
for name in ('LOAD_MENU_UPDATE_20261102.txt','PORTMASTER_AUDIT_2026-11-02.md'):
 files['ports/the-thing/'+name]=(root/'docs'/name).read_bytes()
files['ports/the-thing/load-menu-update-manifest.json']=json.dumps({'game_sha256':expected,'files':{n:hashlib.sha256(b).hexdigest() for n,b in files.items()},'validation':'ARM QEMU menus pass; H700 retest pending; manual installation only'},indent=2).encode()
target=root/'dist/the-thing-load-menu-20261102-update.zip'
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for name,data in sorted(files.items()):
  info=zipfile.ZipInfo(name);info.create_system=3
  executable=name.endswith(('.sh','.aarch64','/thething-launcher'))
  info.external_attr=(0o100755 if executable else 0o100644)<<16
  info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,data)
with zipfile.ZipFile(target) as z:
 assert z.testzip() is None
 assert hashlib.sha256(z.read('ports/the-thing/thething.aarch64')).hexdigest()==expected
 assert not any('/userdata/' in n or '/config/' in n or n.endswith(('.iso','.pak','.sav','.exe')) for n in z.namelist())
sha=hashlib.sha256(target.read_bytes()).hexdigest();target.with_suffix('.zip.sha256').write_text(sha+'  '+target.name+'\n')
print(target,target.stat().st_size,sha)
