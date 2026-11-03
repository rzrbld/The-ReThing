#!/usr/bin/env python3
from pathlib import Path
import zipfile,hashlib
root=Path(__file__).resolve().parents[1]
files={}
for n in ['The ReThing.sh','The Thing.sh','The Thing Measure.sh','The Thing Measure Cache.sh']:
 files['Roms/PORTS/'+n]=(root/'packaging'/n).read_bytes()
files['ports/the-thing/run-game.sh']=(root/'packaging/The Thing.sh').read_bytes()
files['ports/the-thing/the-thing-env.sh']=(root/'packaging/the-thing-env.sh').read_bytes()
files['ports/the-thing/LAUNCHER-PATH-FIX.txt']=b'''Extract to /mnt/sdcard (SD card root), allowing replacements.
Shortcuts: /mnt/sdcard/Roms/PORTS/*.sh
Payload: /mnt/sdcard/ports/the-thing/
Do not move the game folder beside the shortcuts. No game binary, assets, saves
or config are replaced by this patch. Apply the previous Load Game update too
if your executable SHA256 is not:
15485549bb7b78aa10743b51fc5c060267f328b73a18b7477c13de36611bd400
This corrects manual muOS launching; PortMaster catalogue work remains open.
'''
p=root/'dist/the-thing-shortcut-paths-20261102.zip'
with zipfile.ZipFile(p,'w',zipfile.ZIP_DEFLATED) as z:
 for name,data in files.items():
  info=zipfile.ZipInfo(name);info.create_system=3;info.external_attr=(0o100755 if name.endswith('.sh') else 0o100644)<<16
  info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,data)
with zipfile.ZipFile(p) as z:assert z.testzip() is None
p.with_suffix('.zip.sha256').write_text(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n')
print(p,p.stat().st_size)
