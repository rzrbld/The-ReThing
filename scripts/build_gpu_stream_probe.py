#!/usr/bin/env python3
"""Build isolated GPU streaming renderer with matching ABI; restore baseline."""
import hashlib,json,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/gpu-stream-probe';backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
staged=root/'build/render-thread-src/runtime'
paths=[staged/'d3d8_renderer.c',staged/'d3d8_renderer.h']
paths+=sorted(p for p in (root/'build/game/host').iterdir() if p.is_file())
paths += [root/'build/game/thething.host',root/'build/game/thething.host.sha256']
manifest={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));manifest[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 s=(root/'build/gpu-stream-20261102/d3d8_renderer.c').read_text()
 s=s.replace('#include "d3d8_renderer.h"','#include "d3d8_renderer.h"\n#include "render_thread_proxy.h"',1)
 assert s.count('e=glGetError();')==1
 paths[0].write_text(s.replace('e=glGetError();','e=ds_rt_defer_draw_error();',1))
 shutil.copy2(root/'build/gpu-stream-20261102/d3d8_renderer.h',paths[1])
 with (out/'build.log').open('w') as log:subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.host',out/'diagnostic.host')
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==manifest[str(p.relative_to(root))]
evidence=root/'docs/evidence/gpu-stream-20261102';evidence.mkdir(parents=True,exist_ok=True)
(evidence/'build.json').write_text(json.dumps(dict(restored=manifest,candidate_sha256=hashlib.sha256((out/'diagnostic.host').read_bytes()).hexdigest()),indent=2)+'\n')
print('GPU streaming candidate built; baseline source/native artifacts restored')
