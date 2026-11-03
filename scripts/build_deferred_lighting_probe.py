#!/usr/bin/env python3
"""Isolated deferred lighting-upload experiment; restores normal artifacts by hash."""
import hashlib,json,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/deferred-lighting-probe';backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
paths=[root/p for p in ['build/render-thread-src/runtime/d3d8_renderer.c','build/game/host/native_d3d8_renderer.o','build/game/host/native_d3d8_renderer.flags','build/game/thething.host','build/game/thething.host.sha256']]
manifest={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));manifest[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 source=paths[0];text=(root/'build/deferred-lighting-20261102/d3d8_renderer.candidate.c').read_text()
 text=text.replace('#include "d3d8_renderer.h"','#include "d3d8_renderer.h"\n#include "render_thread_proxy.h"',1)
 assert text.count('e=glGetError();')==1
 source.write_text(text.replace('e=glGetError();','e=ds_rt_defer_draw_error();',1))
 with (out/'build.log').open('w') as log:subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.host',out/'diagnostic.host')
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==manifest[str(p.relative_to(root))]
evidence=root/'docs/evidence/deferred-lighting-20261102';evidence.mkdir(parents=True,exist_ok=True)
(evidence/'build.json').write_text(json.dumps(dict(restored=manifest,candidate_sha256=hashlib.sha256((out/'diagnostic.host').read_bytes()).hexdigest()),indent=2)+'\n')
print('Deferred lighting candidate built; source and normal artifacts restored')
