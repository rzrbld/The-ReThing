#!/usr/bin/env python3
"""Cheap counts only. Parent must back up/restore all reported modified units.
--check reports planned staging without changing any generated inputs.
No skinning operation, branch, cached vertex or submission is suppressed.
"""
import argparse,json,re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--check',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[1];planned=[]
for path in (root/'build/translated-local-regs/thething').glob('functions_*.c'):
 s=path.read_text();changes=[]
 for m in re.finditer(r'void recomp_thething_([0-9A-F]{8})\(void\) \{.*?(?=\n(?:__attribute__[^\n]* )?void recomp_|\Z)',s,re.S):
  address=m.group(1);body=m.group();hooks=[]
  if address=='00688730':
   marker='    RECOMP_ENTER(0x00688730u);';assert body.count(marker)==1
   body=body.replace(marker,marker+'\n    thing_game_skin_work_frame();',1);hooks.append('frame')
  elif '/* 0x007AE937:' in body or 'L_007AEB57:' in body or '/* 0x007AED6F:' in body or address=='007AE710':
   assert 'ThingSkinWorkLocal' not in body,'Already staged'
   body=body.replace('{','{\n    ThingSkinWorkLocal _gsw={0};\n    thing_game_skin_work.'+('entry_calls' if address=='007AE710' else 'alternate_entries')+'++;',1)
   label_hooks={
    '007AE89B':'thing_skin_work_begin(&_gsw,MEM32(ebx+0x3C),MEM32(ebx+0x40),MEM32(ebx+0x1C),MEM32(ebx+0x20));',
    '007AE93B':'thing_skin_work_changed(&_gsw,eax);',
    '007AE93F':'thing_skin_work_compared(&_gsw);',
    '007AE960':'thing_skin_work_palette_done(&_gsw,MEM8(esp+0x13));',
    '007AEB57':'thing_skin_work_weight_begin(&_gsw);',
    '007AED00':'thing_skin_work_weight_end(&_gsw);',
   }
   for addr,code in label_hooks.items():
    marker='L_'+addr+':\n'
    if marker in body:assert body.count(marker)==1;body=body.replace(marker,marker+'    '+code+'\n',1);hooks.append(addr)
   # Match instruction comments, retaining original code and localTOP flavor.
   for addr,code in [('007AEA54','if(MEM8(esp+0x13))thing_game_skin_work.sections_reused++;else thing_game_skin_work.sections_recomputed++;'),('007AEBB7','thing_skin_work_influence(&_gsw,(int32_t)(int16_t)MEM16(ebx));'),('007AED6F','thing_skin_work_rigid(&_gsw,(int32_t)(int16_t)MEM16(edx+esi+0x2C));')]:
    marker=re.search(r'^.*?/\* 0x'+addr+r':[^\n]*$',body,re.M)
    if marker:body=body[:marker.start()]+'    '+code+'\n'+body[marker.start():];hooks.append(addr)
  if hooks:changes.append((m.start(),m.end(),body));planned.append(dict(unit=path.name,address=address,hooks=hooks))
 if changes:
  for lo,hi,body in reversed(changes):s=s[:lo]+body+s[hi:]
  assert '#include "../../../probe/game_skin_work/counts.h"' not in s
  s=s.replace('#include "functions.h"','#include "functions.h"\n#include "../../../probe/game_skin_work/counts.h"',1)
  if not a.check:path.write_text(s)
assert any(x['address']=='007AE710' and '007AE89B' in x['hooks'] for x in planned)
assert sum(x['hooks']==['frame'] for x in planned)==1
print(json.dumps(dict(check_only=a.check,entries=planned),indent=2))
