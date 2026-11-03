#!/usr/bin/env python3
"""Compare inlining against the actual existing local-TOP baseline, not old x87 code."""
import json
from pathlib import Path
from probe_fp_local_top_20261102 import transform
root=Path(__file__).resolve().parents[1];src=root/'build/skin-fused-20261102';out=root/'build/skin-fused-local-top-20261103';out.mkdir(exist_ok=True)
leaves=json.loads((root/'build/leaf-registers-20261102/patches.json').read_text())
patched={p['address']for p in json.loads((root/'build/fp-local-top-20261102/patches.json').read_text())}
assert {'007AE710','00414210','00414230'}<=patched
prefix='#define RECOMP_GENERATED_CODE 1\n#define RECOMP_LOCAL_REGS 1\n#include "recomp_types.h"\n#include "../../probe/fp_local_top/local_top.h"\n'
for kind,attrs in [('reference','static __attribute__((noinline,noclone))'),('candidate','static __attribute__((always_inline)) inline')]:
 helpers=''
 for p in leaves[:2]:
  helpers+=transform(p['original']).replace('void recomp_thething_'+p['address'],attrs+' void '+kind+'_'+p['address'],1)+'\n'
 block=(src/'block.txt').read_text()
 for p in leaves[:2]:block=block.replace('recomp_thething_'+p['address'],kind+'_'+p['address'])
 body='void recomp_thething_007AE710(void){\nuint32_t _cf=0,_flag_a=0,_flag_b=0,_flag_k=0; RECOMP_REGS_LOCALS\nRECOMP_ENTER(0x007AE710u);\n'+block+'RECOMP_FLAGS_OUT();RECOMP_REGS_OUT();\n}\n'
 body=transform(body).replace('recomp_thething_007AE710',kind+'0')
 (out/(kind+'.c')).write_text(prefix+helpers+body)
 if kind=='candidate':(out/'helpers-inline.h').write_text('#include "../../../probe/fp_local_top/local_top.h"\n'+helpers.replace('candidate_','thing_skin_inline_'))
(out/'driver.c').write_bytes((src/'driver.c').read_bytes())
print('Staged local-TOP baseline versus local-TOP + inlining; no game mutation')
