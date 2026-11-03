#!/usr/bin/env python3
import re,subprocess,hashlib,json
from pathlib import Path
R=Path(__file__).resolve().parents[1];O=R/'probe/game_color';E=R/'docs/evidence/original-queue-audit-20261103'
def fn(path,name):
 s=path.read_text();m=re.search(r'void '+name+r'\(void\) \{.*?(?=\nvoid recomp_|\Z)',s,re.S);assert m;return m.group()
s=(R/'build/translated-local-regs/thething/functions_0215.c').read_text();block=s[s.index('L_0075D04D:'):s.index('L_0075D13C:')]
preamble='''#define _GNU_SOURCE
#include <fenv.h>
#define RECOMP_GENERATED_CODE
#define RECOMP_LOCAL_REGS
#include "../../third_party/pcrecomp/runtime/recomp32/recomp_types.h"
uint32_t g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;ptrdiff_t g_mem_base;
static uint32_t end_flags[4];
'''
helpers=fn(R/'build/translated-local-regs/thething/functions_0237.c','recomp_thething_007B3480')+fn(R/'build/translated-local-regs/thething/functions_0120.c','recomp_thething_005B78B0')
ref='''static void original(void){
 uint32_t _cf=0,_flag_a=0,_flag_b=0,_flag_k=FK_NONE;
 RECOMP_REGS_LOCALS
 RECOMP_FLAGS_IN();
'''+block+'''
 end_flags[0]=_flag_k;end_flags[1]=_flag_a;end_flags[2]=_flag_b;end_flags[3]=_cf;
 RECOMP_REGS_OUT();
}
'''
(O/'block_reference.inc').write_text(preamble+helpers+ref)
subprocess.run(['cc','-O2','-frounding-math','-fno-strict-aliasing','-Wno-unused-label','-o','/tmp/game_color_block',str(O/'block_test.c'),'-lm'],check=True)
p=subprocess.run(['/tmp/game_color_block'],text=True,capture_output=True,check=True);(E/'color-full-block-test.txt').write_text(p.stdout);print(p.stdout)

native=list(map(int,(E/'color-mapping.txt').read_text().split('table=')[1].strip().split(',')))
table=list(map(int,re.search(r'color_byte\[256\]=\{(.*?)\}',(O/'color_lut.h').read_text()).group(1).split(',')))
assert native==table
(E/'color-block-extraction.json').write_text(json.dumps({'block_sha256':hashlib.sha256(block.encode()).hexdigest(),'helpers_sha256':hashlib.sha256(helpers.encode()).hexdigest(),'runtime_header_sha256':hashlib.sha256((R/'third_party/pcrecomp/runtime/recomp32/recomp_types.h').read_bytes()).hexdigest(),'native_output_matches_all256':True,'reference':'unmodified extracted generated block and ftol/clamp functions; canonical current runtime header','candidate':str((O/'color_block.h').relative_to(R))},indent=2)+'\n')
