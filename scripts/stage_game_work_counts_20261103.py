#!/usr/bin/env python3
"""Cheap work counts only; caller must back up/restore these generated inputs."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
entries={'00754460':0,'0075CEF0':1,'0075CFC3':2}
roots={'00688730':'thing_game_work_frame();','0072EDB0':'thing_game_work.recursive_effect++;','006A0460':'thing_game_work.server_ticks++;','006A0850':'thing_game_work.entity_pass1++;','006A0BA0':'thing_game_work.entity_pass2++;'}
found=[]
for p in (root/'build/translated-local-regs/thething').glob('functions_*.c'):
 s=p.read_text();changed=False
 for a in list(entries)+list(roots)+['0070C1A0']:
  marker='void recomp_thething_'+a+'(void) {'
  if marker not in s:continue
  m=re.search(re.escape(marker)+r'.*?(?=\n(?:__attribute__[^\n]* )?void recomp_|\Z)',s,re.S);assert m
  b=m.group()
  if a in roots:
   enter='    RECOMP_ENTER(0x'+a+'u);';assert b.count(enter)==1;b=b.replace(enter,enter+'\n    '+roots[a],1)
  elif a in entries:
   tag='L_0075D04D:\n';assert b.count(tag)==1;b=b.replace(tag,tag+f'    thing_game_work_color({entries[a]},MEM32(edi+0x44));\n',1)
  else:
   tag='    MEM32(esp + 0x14) = ebp; /* 0x0070C204: mov dword ptr [esp + 0x14], ebp */';assert b.count(tag)==1;b=b.replace(tag,tag+'\n    thing_game_work_spatial(ebp);')
   for tag,code in [('L_0070C2B5:\n','thing_game_work.spatial_tail++;'),('L_0070C2CB:\n','thing_game_work.spatial_index_sum+=ecx;'),('L_0070C2E0:\n','thing_game_work.spatial_filter++;')]:
    assert b.count(tag)==1,(a,tag);b=b.replace(tag,tag+'    '+code+'\n')
  s=s[:m.start()]+b+s[m.end():];changed=True;found.append(a)
 if changed:
  s=s.replace('#include "functions.h"','#include "functions.h"\n#include "../../../probe/game_work/counts.h"',1);p.write_text(s)
assert len(found)==9,found
