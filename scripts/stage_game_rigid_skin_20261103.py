#!/usr/bin/env python3
"""Prototype whole rigid-loop replacement. Parent owns backup/build/restoration.
--check only lists intended changes. Native rigid.c requires -ffp-contract=off.
"""
import argparse,re,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--check',action='store_true');a=p.parse_args()
r=Path(__file__).resolve().parents[1];plan=[]
for path in (r/'build/translated-local-regs/thething').glob('functions_*.c'):
 s=path.read_text();edits=[]
 for m in re.finditer(r'void recomp_thething_([0-9A-F]{8})\(void\) \{.*?(?=\n(?:__attribute__[^\n]* )?void recomp_|\Z)',s,re.S):
  b=m.group();label='L_007AED58:\n'
  if label not in b:continue
  assert b.count(label)==1 and 'thing_game_rigid_skin(' not in b
  localtop='_thing_fp_top' in b;top='_thing_fp_top' if localtop else 'g_fp_top'
  patch='''    if(thing_game_rigid_skin_enabled) { uint32_t _rs_n=MEM32(esp+0x14);
      if(_rs_n>1 && _rs_n<=1000000u && edx==(_rs_n-1)*72u && edi==(_rs_n-1)*32u) {
        uint32_t _rs_head=MEM32(ebx+0x4C)+MEM32(esp+0x1C);
        uint32_t _rs_done=thing_game_rigid_skin(MEM32(ebp+4),MEM32(ebp+8),MEM32(_rs_head),MEM32(_rs_head+4),MEM32(ebx+0x3C),MEM32(ebx+0x40),_rs_n,g_fpu_cw,TOP);
        edx-=_rs_done*72u;edi-=_rs_done*32u;MEM32(esp+0x14)=_rs_n-_rs_done;
      }
    }
'''.replace('TOP',top)
  b=b.replace(label,label+patch,1);edits.append((m.start(),m.end(),b));plan.append(dict(unit=path.name,entry=m.group(1),local_top=localtop))
 if edits:
  for lo,hi,b in reversed(edits):s=s[:lo]+b+s[hi:]
  s=s.replace('#include "functions.h"','#include "functions.h"\n#include "../../../probe/game_rigid_skin/rigid.h"',1)
  if not a.check:path.write_text(s)
assert any(x['entry']=='007AE710' for x in plan)
print(json.dumps(dict(check_only=a.check,entries=plan),indent=2))
