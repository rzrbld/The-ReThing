#!/usr/bin/env python3
"""Stage bounded matrix scalarization; no changes to game or runtime."""
import json,re
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/matrix-scalar-guarded-20261102';out.mkdir(exist_ok=True)
p=json.loads((root/'build/leaf-registers-20261102/patches.json').read_text())[4];assert p['address']=='005B7F80'
original=p['original'];a=original.index('L_005B7F80:')+len('L_005B7F80:');b=original.index('    RECOMP_FLAGS_OUT(); esp += 8;');block=original[a:b]
assert 'goto 'not in block and 'RECOMP_CALL'not in block
depth=maximum=0;lines=[]
for line in block.splitlines():
 code=line.split('/*',1)[0]
 if 'fp_push('in code:
  m=re.fullmatch(r'\s*fp_push\((.*)\);\s*',code);assert m,line
  lines.append(f' matrix_fp_{depth}=(double)({m[1]});');depth+=1;maximum=max(maximum,depth);continue
 if 'fp_pop()'in code:
  assert depth>0;depth-=1;line=line.replace('fp_pop()',f'matrix_fp_{depth}')
 def slot(m):
  n=depth-1-int(m[1]);assert n>=0;return f'matrix_fp_{n}'
 lines.append(re.sub(r'FP_ST\((\d+)\)',slot,line))
assert depth==0 and maximum<=8
end=[]
for n in range(1,maximum+1):
 end += [f'g_st[(8u-(unsigned)g_fp_top-{n}u)&7u]=0.0;',f'g_st_i64[(8u-(unsigned)g_fp_top-{n}u)&7u]=0;']
body='\n'.join([*(f'double matrix_fp_{i};'for i in range(maximum)),*lines,*end])+'\n'
prefix='#define RECOMP_GENERATED_CODE 1\n#define RECOMP_LOCAL_REGS 1\n#include "recomp_types.h"\n'
(out/'reference.c').write_text(prefix+original.replace('recomp_thething_005B7F80','reference0',1))
s=original[:a]+body+original[b:];s=s.replace('void recomp_thething_005B7F80','static void scalar0',1)
s+='''
#include "../../probe/skin_scalar_guard.h"
extern void reference0(void);
unsigned skin_fast_hits,skin_fallback_hits;
static int eligible(void){
 uint32_t a=g_edx,b=MEM32(g_esp+4),o=g_ecx;
 return skin_range_ok(a,64)&&skin_range_ok(b,64)&&skin_range_ok(o,64)
 &&skin_separate(o,64,a,64)&&skin_separate(o,64,b,64)
 &&skin_bounded(a,16)&&skin_bounded(b,16);
}
void candidate0(void){if(eligible()){skin_fast_hits++;scalar0();}else{skin_fallback_hits++;reference0();}}
'''
(out/'candidate.c').write_text(prefix+s);(out/'body.txt').write_text(body)
d=(root/'build/skin-scalar-guarded-20261102/driver.c').read_text()
a=d.index(' g_eax=256;');b=d.index(' g_flag_a=',a)
d=d[:a]+''' g_eax=64;g_ecx=1536;g_edx=512;g_esp=2048;g_esi=0;g_ebx=128;g_edi=trial;g_ebp=1536;g_cur_func=0x777777;
 MEM32(g_esp+4)=256;
 if(trial%5==0)g_ecx=256;if(trial%7==0)g_ecx=516;if(trial%11==0)g_edx=256;
'''+d[b:]
d=d.replace('including four original helper calls','for guarded matrix multiplication')
(out/'driver.c').write_text(d)
print('Staged guarded matrix scalarization, maximum FP depth',maximum)
