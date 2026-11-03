#!/usr/bin/env python3
"""Generate an isolated local-x87-top test for five audited call-free leaves."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];base=root/'build/leaf-registers-20261102';out=root/'build/leaf-fp-top-20261102';out.mkdir(exist_ok=True)
patches=json.loads((base/'patches.json').read_text());source=(base/'probe.c').read_text()
header='''#ifndef THING_LEAF_FP_TOP_H
#define THING_LEAF_FP_TOP_H
/* Only for audited call-free bodies: publish top before every return. */
#define THING_LEAF_ST(i) (g_st[((8u-(unsigned)_thing_fp_top)+(unsigned)(i))&7u])
static inline void thing_leaf_push(int *top,double value){
 unsigned next=(unsigned)*top+1u;*top=(int)next;unsigned slot=(8u-next)&7u;
 g_st[slot]=value;g_st_i64[slot]=0;
}
static inline double thing_leaf_pop(int *top){
 unsigned old=(unsigned)*top,slot=(8u-old)&7u;double value=g_st[slot];
 g_st[slot]=0.0;g_st_i64[slot]=0;*top=(int)(old-1u);return value;
}
#endif
'''
(out/'leaf_fp_top.h').write_text(header);source=source.replace('#include "recomp_types.h"','#include "recomp_types.h"\n#include "leaf_fp_top.h"',1)
result=[]
for i,p in enumerate(patches):
 old=p['candidate'].replace('void recomp_thething_'+p['address'],'__attribute__((noinline)) static void candidate'+str(i))
 body=p['original'].replace('RECOMP_REGS_LOCALS','RECOMP_REGS_LOCALS\n    int _thing_fp_top=g_fp_top;')
 body=body.replace('fp_push(','thing_leaf_push(&_thing_fp_top,').replace('fp_pop()','thing_leaf_pop(&_thing_fp_top)').replace('FP_ST(','THING_LEAF_ST(')
 body=body.replace('RECOMP_REGS_OUT()','(g_fp_top=_thing_fp_top,RECOMP_REGS_OUT())')
 new=body.replace('void recomp_thething_'+p['address'],'__attribute__((noinline)) static void candidate'+str(i))
 assert source.count(old)==1;source=source.replace(old,new,1)
 result.append(dict(address=p['address'],original=p['original'],candidate=body))
source=source.replace('exact leaf register/memory/x87/flags comparisons','exact local-x87-top register/memory/x87/flags comparisons')
(out/'probe.c').write_text(source);(out/'patches.json').write_text(json.dumps(result,indent=2)+'\n')
print('Generated local-top test; no game files changed')
