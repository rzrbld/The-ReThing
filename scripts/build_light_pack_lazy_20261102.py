#!/usr/bin/env python3
"""Build isolated HLE light-pack candidate; restore original files by hash."""
import hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
target='aarch64' if '--target=aarch64' in sys.argv else 'host'
variant='light-pack-lazy-game-'+target+'-20261102'
out=root/'build'/variant;backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
source=root/'build/render-thread-src/runtime/d3d8.c'
subprocess.run(['python3','scripts/prepare_light_pack_lazy_20261102.py','--input',str(source),'--output-dir',str(out)],cwd=root,check=True)
paths=[source,root/'build/game'/('thething.'+target),root/'build/game'/('thething.'+target+'.sha256')]
paths+=sorted(p for p in (root/'build/game'/target).iterdir()if p.is_file())
hashes={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));hashes[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 source.write_text((out/'d3d8.candidate.c').read_text())
 command=['python3','-u','scripts/build_game.py','--target',target,'--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4']
 if target=='aarch64':command=['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{root}:/src:ro','-v',f'{root}/build/game:/src/build/game:rw','--workdir','/src','sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49',*command]
 with (out/'build.log').open('w')as log:subprocess.run(command,cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game'/('thething.'+target),out/('diagnostic.'+target))
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==hashes[str(p.relative_to(root))]
e=root/'docs/evidence'/variant;e.mkdir(exist_ok=True)
(e/'build.json').write_text(json.dumps(dict(restored=hashes,candidate_sha256=hashlib.sha256((out/('diagnostic.'+target)).read_bytes()).hexdigest()),indent=2)+'\n')
print('Lazy light pack candidate built; baseline sources and artifacts restored')
