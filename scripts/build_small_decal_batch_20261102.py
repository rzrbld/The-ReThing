#!/usr/bin/env python3
"""Build isolated ordered-alpha batching host; restore normal native artifacts."""
import hashlib,json,shutil,subprocess,sys,os
from pathlib import Path
root=Path(__file__).resolve().parents[1]
target='aarch64' if '--target=aarch64' in sys.argv else 'host'
variant='small-decal-batch-20261102' if target=='host' else 'small-decal-batch-aarch64-20261102'
out=root/'build'/variant
prepared=root/'build/small-decal-batch-20261102/adjacent_batch.c'
backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
paths=[root/'build/render-thread-src/runtime/adjacent_batch.c']
paths+=sorted(p for p in (root/'build/game'/target).iterdir()if p.is_file())
paths +=[root/'build/game'/('thething.'+target),root/'build/game'/('thething.'+target+'.sha256')]
hashes={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));hashes[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 shutil.copy2(prepared,paths[0])
 command=['python3','-u','scripts/build_game.py','--target',target,'--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4']
 if target=='aarch64':command=['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{root}:/src:ro','-v',f'{root}/build/game:/src/build/game:rw','--workdir','/src','sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49',*command]
 with (out/'build.log').open('w')as log:
  subprocess.run(command,cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game'/('thething.'+target),out/('diagnostic.'+target))
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==hashes[str(p.relative_to(root))]
e=root/'docs/evidence'/variant;e.mkdir(exist_ok=True)
(e/'build.json').write_text(json.dumps(dict(restored=hashes,candidate_sha256=hashlib.sha256((out/('diagnostic.'+target)).read_bytes()).hexdigest()),indent=2)+'\n')
print('Isolated alpha batch host built; all normal artifacts restored')
