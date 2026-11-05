#!/usr/bin/env python3
"""Stage the guarded material-prefix bypass in user-generated translated code."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=ROOT/'build/translated-local-regs/thething/functions_0221.c'
s=p.read_text()
a=s.index('void recomp_thething_0076FC10(void) {')
b=s.index('\nvoid ',a+1)
body=s[a:b]
assert body.count('L_0076FC46:\n')==1
assert body.count('L_0076FE24:\n')==1
assert 'thing_material_state_reusable' not in s, 'already staged'
body=body.replace('L_0076FC46:\n','L_0076FC46:\n    if(thing_material_state_reusable(ebp,esi))goto L_0076FE24;\n',1)
s=s[:a]+body+s[b:]
assert s.count('#include "functions.h"')==1
s=s.replace('#include "functions.h"','#include "functions.h"\nextern int thing_material_state_reusable(uint32_t,uint32_t);',1)
p.write_text(s)
print('Staged guarded original material-state prefix reuse')
