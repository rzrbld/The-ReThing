#!/usr/bin/env python3
import re,hashlib,json,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/queue-bounds-capture-20261103';backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
roots={'00789200':'cell','00754700':'tag','0075DC80':'render','00758AA0':'present','0075C850':'queue'};units={}
for p in (root/'build/translated-local-regs/thething').glob('functions_*.c'):
 s=p.read_text();found=[a for a in roots if 'void recomp_thething_'+a+'(void) {' in s]
 if found:units[p]=found
assert sum(map(len,units.values()))==5
paths=[]
for p in units:paths +=[p,root/'build/game/host/thething'/(p.stem+'.o'),root/'build/game/host/thething'/(p.stem+'.flags')]
paths +=[root/'build/game/host/thething/lifted.a',root/'build/game/thething.host',root/'build/game/thething.host.sha256']
stage=root/'build/render-thread-src/runtime/d3d8.c';paths +=[stage]+sorted(p for p in (root/'build/game/host').iterdir() if p.is_file());paths=list(dict.fromkeys(paths));manifest={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));manifest[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 s=stage.read_text();needle='static D8State state;';assert s.count(needle)==1
 s=s.replace(needle,needle+'\n#include "../../queue-bounds-capture-20261103/capture.h"')
 s=s.replace('static void diagnostic_poll(void){','static void diagnostic_poll(void){gc_end();',1)
 s=s.replace('else if(!strcmp(command,"glcost"))gl_cost_requested=120;','else if(!strcmp(command,"geometry")){gc_begin(root,frames+1);gl_cost_requested=120;}\n    else if(!strcmp(command,"glcost"))gl_cost_requested=120;',1)
 needle='    diagnostic_draw(primitive,count);';assert s.count(needle)==1;s=s.replace(needle,'    gc_capture(primitive,count,vertices,stride,vcount,indices,isize,base,render_target,depth_target);\n'+needle)
 stage.write_text(s)
 for i,(p,addresses) in enumerate(units.items()):
  s=p.read_text().replace('#include "functions.h"','#include "functions.h"\n'+('#define THING_TERRAIN_IDENTITY_DEFINE\n' if i==0 else '')+'#include "../../queue-bounds-capture-20261103/identity.h"',1)
  for a in addresses:
   m=re.search(r'void recomp_thething_'+a+r'\(void\) \{.*?(?=\nvoid recomp_|\Z)',s,re.S);assert m;b=m.group();kind=roots[a];end=''
   if kind=='queue':begin='ti_capture_bounds(MEM32(MEM32(g_esp+4)),MEM32(g_esp+8));'
   elif kind=='tag':begin='ti_tag(MEM32(MEM32(g_esp+4)));'
   elif kind=='present':begin='ti_present();'
   elif kind=='render':begin='uint32_t _ti_old_render=ti_render;ti_render=MEM32(MEM32(g_esp+4)+0x90);';end='ti_render=_ti_old_render;'
   else:
    begin='uint32_t _ti_old_cell=ti_cell,_ti_old_node=ti_node,_ti_old_target=ti_target;ti_cell=g_ecx;';end='ti_cell=_ti_old_cell;ti_node=_ti_old_node;ti_target=_ti_old_target;'
    needle='    RECOMP_ICALL(MEM32(edx + 0x10)); /* 0x0078922C:';assert b.count(needle)==1;b=b.replace(needle,'    ti_node=ecx;ti_target=MEM32(edx+0x10);\n'+needle)
   b=b.replace('{','{\n    '+begin,1)
   if end:
    b=b.replace('RECOMP_REGS_OUT(); return;',end+' RECOMP_REGS_OUT(); return;')
    b=b.replace('    RECOMP_REGS_OUT(); /* checked fallthrough flush */','    '+end+'\n    RECOMP_REGS_OUT(); /* checked fallthrough flush */')
   s=s[:m.start()]+b+s[m.end():]
  p.write_text(s)
 with (out/'build.log').open('w') as log:subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.host',out/'diagnostic.host')
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==manifest[str(p.relative_to(root))]
e=root/'docs/evidence/queue-bounds-capture-20261103';e.mkdir(exist_ok=True);(e/'build.json').write_text(json.dumps({'restored':manifest,'diagnostic_sha256':hashlib.sha256((out/'diagnostic.host').read_bytes()).hexdigest()},indent=2)+'\n');shutil.rmtree(backup)
print('Terrain capture built; baseline restored and rollback copies removed')
