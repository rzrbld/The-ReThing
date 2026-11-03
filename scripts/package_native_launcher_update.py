#!/usr/bin/env python3
"""Small native-launcher overlay, derived from the verified full r3 package."""
import hashlib,json,zipfile
from pathlib import Path
R=Path(__file__).resolve().parents[1]
P='ports/the-thing/'
with zipfile.ZipFile(R/'dist/the-thing-production-review-20261103-r3.zip') as z:
 files={P+n:z.read(P+n) for n in ('thething-launcher','run-game.sh','NATIVE_SETUP.txt','FIRST_RUN_INSTALL.txt')}
files['UPDATE-NATIVE-LAUNCHER.txt']=b'Extract at the SD-card root, replacing the packaged files. Start The ReThing normally. Python is no longer needed. Existing bin/ and data/ are verified and reused; saves/settings are preserved. Old installer/*.py files are unused and may remain. Requires the existing full port with bundled tools/. Game executable unchanged. See ports/the-thing/NATIVE_SETUP.txt.\n'
files[P+'native-launcher-update-manifest.json']=json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in files.items()},indent=2).encode()
out=R/'dist/the-thing-native-launcher-update-20261103.zip'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for n,b in sorted(files.items()):
  i=zipfile.ZipInfo(n,(2026,11,3,0,0,0));i.create_system=3;i.external_attr=(0o100755 if n.endswith(('.sh','thething-launcher')) else 0o100644)<<16;i.compress_type=zipfile.ZIP_DEFLATED;z.writestr(i,b)
with zipfile.ZipFile(out) as z:assert z.testzip() is None
out.with_suffix('.zip.sha256').write_text(hashlib.sha256(out.read_bytes()).hexdigest()+'  '+out.name+'\n')
print(out,out.stat().st_size)
