#!/usr/bin/env python3
"""Own-copy diagnostic update: never package assets or user state."""
import hashlib,json,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
expected='7c0ae7511ab36ffd17d48fd3a211e4080dbb730228c951e71a08de21ea71d683'
game=(root/'build/index-range-candidate/thething.aarch64').read_bytes()
assert hashlib.sha256(game).hexdigest()==expected
files={'ports/the-thing/thething.aarch64':game,'ports/the-thing/CPU_MEASURE_20261102.txt':(root/'docs/CPU_MEASURE_20261102.txt').read_bytes()}
for name in ('The Thing Measure.sh','The Thing Measure Scan.sh'):
 files['Roms/PORTS/'+name]=(root/'packaging'/name).read_bytes()
for name in ('the-thing-env.sh','muos-session.sh','cpu-session.sh'):
 files['ports/the-thing/'+name]=(root/'packaging'/name).read_bytes()
files['ports/the-thing/run-game.sh']=(root/'packaging/The Thing.sh').read_bytes()
files['ports/the-thing/cpu-measure-manifest.json']=json.dumps({'game_sha256':expected,'debug_elf_sha256':'fd314856d177c4ad616d74fee89a6978557c7a122ee99d91d6d486103fa19bba','hardware_performance':'unverified','files':{name:hashlib.sha256(data).hexdigest() for name,data in files.items()}},indent=2).encode()
target=root/'dist/the-thing-cpu-measure-20261102.zip'
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for name,data in sorted(files.items()):
  assert not any(part in name.split('/') for part in ('data','bin','userdata','config','source','extracted-iso'))
  info=zipfile.ZipInfo(name);info.create_system=3;info.external_attr=(0o100755 if name.endswith(('.sh','.aarch64')) else 0o100644)<<16;info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,data)
with zipfile.ZipFile(target) as z:
 assert z.testzip() is None
 assert hashlib.sha256(z.read('ports/the-thing/thething.aarch64')).hexdigest()==expected
target.with_suffix('.zip.sha256').write_text(hashlib.sha256(target.read_bytes()).hexdigest()+'  '+target.name+'\n')
print(target,target.stat().st_size)
