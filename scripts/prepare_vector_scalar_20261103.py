#!/usr/bin/env python3
"""Scalarize original vector leaves; preserve ordered loads/stores and fallback for nonfinite inputs."""
import json,re
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/vector-scalar-20261103';out.mkdir(exist_ok=True)
patches=json.loads((root/'build/leaf-registers-20261102/patches.json').read_text())
prefix='#define RECOMP_GENERATED_CODE 1\n#define RECOMP_LOCAL_REGS 1\n#include "recomp_types.h"\n'
for index,p in enumerate(patches[:2]):
 original=p['original'];s=original;start=s.index('L_'+p['address']+':')+len('L_'+p['address']+':');end=s.index('    RECOMP_FLAGS_OUT(); esp += 8;')
 depth=0;lines=['double vector_fp;']
 for line in s[start:end].splitlines():
  code=line.split('/*',1)[0]
  if 'fp_push(' in code:
   m=re.fullmatch(r'\s*fp_push\((.*)\);\s*',code);assert m and depth==0
   lines.append('vector_fp=(double)('+m[1]+');');depth=1
  else:
   if 'fp_pop()' in line:assert depth==1;depth=0;line=line.replace('fp_pop()','vector_fp')
   lines.append(line.replace('FP_ST(0)','vector_fp'))
 assert depth==0
 lines+=['g_st[(7u-(unsigned)g_fp_top)&7u]=0.0;','g_st_i64[(7u-(unsigned)g_fp_top)&7u]=0;']
 scalar=(s[:start]+'\n'.join(lines)+'\n'+s[end:]).replace('void recomp_thething_'+p['address'],'static void scalar'+str(index),1)
 ref=original.replace('void recomp_thething_'+p['address'],'void reference'+str(index),1)
 # Integer classification does not raise FP exceptions. Preserve exceptional path exactly.
 guard='uint32_t a=g_edx,b=MEM32(g_esp+4);' if index==0 else 'uint32_t a=g_edx,b=g_esp+4;'
 guard+='for(unsigned i=0;i<3;i++)if((MEM32(a+4*i)&0x7f800000u)==0x7f800000u)return 0;'
 guard+=('for(unsigned i=0;i<3;i++)if((MEM32(b+4*i)&0x7f800000u)==0x7f800000u)return 0;' if index==0 else 'if((MEM32(b)&0x7f800000u)==0x7f800000u)return 0;')
 # Exclude stack-output alias: input pointer/scale and guest return state may be overwritten.
 guard+='if((uint64_t)g_ecx< (uint64_t)g_esp+8 && (uint64_t)g_esp<(uint64_t)g_ecx+12)return 0;return 1;'
 (out/f'reference{index}.c').write_text(prefix+ref)
 (out/f'candidate{index}.c').write_text(prefix+scalar+f'\nextern void reference{index}(void);\nstatic int eligible{index}(void){{{guard}}}\nvoid candidate{index}(void){{if(eligible{index}())scalar{index}();else reference{index}();}}\n')
