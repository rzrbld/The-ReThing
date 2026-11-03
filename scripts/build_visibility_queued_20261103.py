#!/usr/bin/env python3
"""Instrument selected original functions; restore normal build by hash."""
import re,hashlib,json,shutil,subprocess,sys
from pathlib import Path
variant='visibility-queued-probe-20261103'
root=Path(__file__).resolve().parents[1];out=root/'build'/variant;backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
header_text=(root/'probe/effects/attribution.h').read_text()
header_text=header_text.replace('extern unsigned ea_context;', 'extern unsigned ea_context,ea_render_context;\nunsigned thing_effect_draw_category(void);')
header_text=header_text.replace('unsigned ea_context;\nstatic', 'unsigned ea_context,ea_render_context;\nunsigned thing_effect_draw_category(void){return ea_render_context;}\nstatic')
header_text=header_text.replace('EA_AMBIGUOUS,EA_COUNT','EA_AMBIGUOUS,EA_STATIC,EA_SKIN,EA_COUNT').replace('\"other\",\"weather\",\"particle\",\"decal\",\"shadow\",\"ambiguous\"','\"other\",\"weather\",\"particle\",\"decal\",\"shadow\",\"ambiguous\",\"static_mesh\",\"skin_mesh\"')
(out/'attribution.h').write_text(header_text)
roots={'007A3CE0':'EA_STATIC','007AE710':'EA_SKIN','005B7190':'EA_WEATHER','00778590':'EA_PARTICLE','0077E310':'EA_DECAL','006D8A90':'EA_SHADOW','006D8CA0':'EA_SHADOW','00754700':'tag','0075DC80':'render','00758AA0':'present'}
# RECOMP_ITAIL in current header invokes the callee and returns to the generated
# explicit return below; section_end covers it and includes descendant work.
hdr=(root/'third_party/pcrecomp/runtime/recomp32/recomp_types.h').read_text()
itail=hdr.split('#define RECOMP_ITAIL(target_va)',1)[1].split('} while(0)',1)[0]
assert '_fn(); RECOMP_REGS_IN();' in itail and 'return;' not in itail
units={}
for p in (root/'build/translated-local-regs/thething').glob('functions_*.c'):
 text=p.read_text();found=[a for a in roots if 'void recomp_thething_'+a+'(void) {' in text]
 if found:units[p]=found
assert sum(map(len,units.values()))==len(roots)
paths=[]
for p in units:paths += [p,root/'build/game/host/thething'/(p.stem+'.o'),root/'build/game/host/thething'/(p.stem+'.flags')]
paths += [root/'build/game/host/thething/lifted.a',root/'build/game/thething.host',root/'build/game/thething.host.sha256']
stage=root/'build/render-thread-src/runtime/d3d8.c'
paths += [stage]+sorted(p for p in (root/'build/game/host').iterdir() if p.is_file())
paths=list(dict.fromkeys(paths))
manifest={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));manifest[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 staged=stage.read_text()
 needle='static void diagnostic_draw(unsigned primitive,unsigned count){'
 assert staged.count(needle)==1
 staged=staged.replace(needle,'extern unsigned thing_effect_draw_category(void) __attribute__((weak));\n'+needle)
 needle='    fputs("rs",diagnostic_frame);'
 assert staged.count(needle)==1
 staged=staged.replace(needle,'    if(thing_effect_draw_category)fprintf(diagnostic_frame,"effect category=%u\\n",thing_effect_draw_category());\n'+needle)
 stage.write_text(staged)
 for unit_index,(p,addresses) in enumerate(units.items()):
  header=('#define THING_EFFECT_ATTRIBUTION_DEFINE\n' if unit_index==0 else '')+'#include "'+str(out/'attribution.h')+'"'
  text=p.read_text().replace('#include "functions.h"','#include "functions.h"\n'+header,1)
  for a in addresses:
   match=re.search(r'void recomp_thething_'+a+r'\(void\) \{.*?(?=\nvoid recomp_|\Z)',text,re.S);assert match
   body=match.group();kind=roots[a];endcode=''
   if kind=='tag':begin='ea_tag(MEM32(MEM32(g_esp+4)));'
   elif kind=='present':begin='ea_present();'
   elif kind=='render':
    begin='unsigned _ea_cat=ea_lookup(MEM32(MEM32(g_esp+4)+0x90)); unsigned _ea_prev=ea_render_context; ea_render_context=_ea_cat; uint64_t _ea_start=ea_clock();'
    endcode='ea_record(_ea_cat,ea_clock()-_ea_start); ea_render_context=_ea_prev; '
   else:
    begin='unsigned _ea_old=ea_context; '+('if(ea_context==EA_OTHER)' if kind in ('EA_PARTICLE','EA_STATIC','EA_SKIN') else '')+'ea_context='+kind+';'
    endcode='ea_context=_ea_old; '
   body=body.replace('{','{\n    '+begin,1)
   if endcode:
    marker='RECOMP_REGS_OUT(); return;';assert marker in body
    body=body.replace(marker,endcode+marker)
    end='    RECOMP_REGS_OUT(); /* checked fallthrough flush */';assert body.count(end)==1
    body=body.replace(end,'    '+endcode+'\n'+end)
   text=text[:match.start()]+body+text[match.end():]
  p.write_text(text)
 with (out/'build.log').open('w') as log:subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.host',out/'diagnostic.host')
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==manifest[str(p.relative_to(root))]
evidence=root/'docs/evidence'/variant;evidence.mkdir(parents=True,exist_ok=True)
(evidence/'build.json').write_text(json.dumps(dict(roots=roots,restored=manifest,candidate_sha256=hashlib.sha256((out/'diagnostic.host').read_bytes()).hexdigest()),indent=2)+'\n')
print('CPU section diagnostic built; original artifacts restored by hash')
