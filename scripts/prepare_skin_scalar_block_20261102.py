#!/usr/bin/env python3
"""Scalarize a proven balanced, branch-free skin block; isolated experiment."""
import json,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'build/skin-scalar-block-20261102';out.mkdir(exist_ok=True)
src=root/'build/skin-fused-20261102'
block=(src/'block.txt').read_text()
leaves=json.loads((root/'build/leaf-registers-20261102/patches.json').read_text())
for i in (0,1):
 p=leaves[i];s=p['original'];s=s[s.index('L_'+p['address']+':')+len('L_'+p['address']+':'):]
 s=s[:s.index('RECOMP_FLAGS_OUT(); esp += 8;')]
 assert not any(x in s for x in ('goto ','RECOMP_CALL','_flag_','_cf ='))
 block=block.replace('RECOMP_CALL(recomp_thething_'+p['address']+');','PUSH32(esp, RECOMP_RETADDR);\n'+s+'\n esp += 8;')
block=block.replace('RECOMP_FLAGS_IN();','')
assert 'RECOMP_CALL' not in block and 'goto ' not in block
# No callee or runtime yield remains. All x87 reads in this block refer to
# values pushed within it. Keep arithmetic order and every guest store.
depth=0;maximum=0;result=[]
for line in block.splitlines():
 code=line.split('/*',1)[0]
 if 'fp_push(' in code:
  m=re.fullmatch(r'\s*fp_push\((.*)\);\s*',code);assert m,line
  result.append(' skin_fp_'+str(depth)+' = (double)('+m[1]+');')
  depth+=1;maximum=max(maximum,depth)
  continue
 if 'fp_pop()' in code:
  assert depth>0,line
  depth-=1;line=line.replace('fp_pop()','skin_fp_'+str(depth))
 def slot(m):
  n=depth-1-int(m[1]);assert n>=0,line
  return 'skin_fp_'+str(n)
 line=re.sub(r'FP_ST\((\d+)\)',slot,line)
 result.append(line)
assert depth==0 and maximum<=8,(depth,maximum)
# Restore all touched physical slots exactly, including inactive slots. The
# entry top and all untouched slots/shadows are preserved, even for odd tops.
end=[]
for n in range(1,maximum+1):
 end.append(f'g_st[(8u-(unsigned)g_fp_top-{n}u)&7u]=0.0;')
 end.append(f'g_st_i64[(8u-(unsigned)g_fp_top-{n}u)&7u]=0;')
body='\n'.join('double skin_fp_'+str(i)+';'for i in range(maximum))+'\n'+'\n'.join(result+end)+'\n'
prefix='#define RECOMP_GENERATED_CODE 1\n#define RECOMP_LOCAL_REGS 1\n#include "recomp_types.h"\n'
(out/'candidate.c').write_text(prefix+'void candidate0(void){\nuint32_t _cf=0,_flag_a=0,_flag_b=0,_flag_k=0; RECOMP_REGS_LOCALS\nRECOMP_ENTER(0x007AE710u);\n'+body+'RECOMP_FLAGS_OUT();RECOMP_REGS_OUT();\n}\n')
(out/'body.txt').write_text(body)
for name in ('reference.c','driver.c'):
 text=(src/name).read_text()
 if name=='driver.c':
  text=text.replace('if(t%2)v=', 'if(t%2 || getenv("THING_TEST_FINITE_ONLY"))v=')
  text=text.replace('fprintf(stderr,"FAIL mode=', 'for(unsigned k=0;k<sizeof memory;k+=4){uint32_t a,b;memcpy(&a,expected+k,4);memcpy(&b,memory+k,4);if(a!=b)fprintf(stderr,"diff offset=%u expected=%08x actual=%08x\\n",k,a,b);}fprintf(stderr,"FAIL mode=')
 (out/name).write_text(text)
print('Staged scalar skin block; maximum temporary stack depth',maximum)
