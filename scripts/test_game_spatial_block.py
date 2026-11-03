#!/usr/bin/env python3
"""Extract original search block; compile/test safe tail splice in /tmp only."""
from pathlib import Path
import hashlib, subprocess, tempfile, sys, os

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'build/active/bin/thething.exe'
assert hashlib.sha256(EXE.read_bytes()).hexdigest() == '085d1a90b2bd57967e8e911eb1f7c2232fd4b045b7f363c5c4a3056f720abb49'
SOURCE = ROOT / 'build/call-flag-inputs/translated/thething/functions_0194.c'
text = SOURCE.read_text().split('void recomp_thething_0070C1A0(void) {',1)[1]
block = text.split('L_0070C20E:',1)[1].split('    if (CMP_GE(_flag_a, _flag_b)) goto L_0070C62B; /* 0x0070C2D3:',1)[0]
splice = (ROOT / 'probe/game_spatial/tail_splice.inc').read_text()
preamble = '''#define _GNU_SOURCE 1
#define RECOMP_GENERATED_CODE 1
#define RECOMP_LOCAL_REGS 1
#include "recomp_types.h"
#include <fenv.h>
#include <assert.h>
#include <stdio.h>
uint32_t g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;ptrdiff_t g_mem_base;
static int game_spatial_generation_valid;
static uint32_t end_state[6];
'''
def function(name, body):
    return 'static void '+name+'(void) {\nint _fpu_cmp=0,_df=1;uint32_t _cf=0,_flag_a=0,_flag_b=0,_flag_k=FK_NONE;\nRECOMP_REGS_LOCALS\n'+body+'''\nend_state[0]=_fpu_cmp;end_state[1]=_cf;end_state[2]=_flag_a;end_state[3]=_flag_b;end_state[4]=_flag_k;end_state[5]=_df;
RECOMP_REGS_OUT();
}\n'''
code = preamble + function('original',block) + function('candidate',block.replace('L_0070C2B1:', 'L_0070C2B1:\n'+splice))
code += (ROOT / 'probe/game_spatial/block_test.inc').read_text()
with tempfile.TemporaryDirectory(prefix='game-spatial-block-') as work:
    path=Path(work); (path/'block.c').write_text(code)
    flags=['-std=c11','-O2','-fno-fast-math','-frounding-math','-fno-strict-aliasing','-I'+str(ROOT/'third_party/pcrecomp/runtime/recomp32'),str(path/'block.c'),'-lm','-o',str(path/'test')]
    arm='--aarch64' in sys.argv
    if arm:
        image='sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49'
        subprocess.run(['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{ROOT}:{ROOT}:ro','-v',f'{path}:{path}:rw',image,'aarch64-linux-gnu-gcc','-static','-mcpu=cortex-a53']+flags,check=True)
        runner=[str(ROOT/'build/tools/qemu-user/usr/bin/qemu-aarch64'),str(path/'test')]
    else:
        subprocess.run(['cc']+flags,check=True)
        runner=[str(path/'test')]
    result=subprocess.run(runner,check=True,text=True,capture_output=True)
    print(result.stdout,end='')
    evidence=ROOT/'docs/evidence/original-spatial-audit-20261103'/('block-aarch64-qemu.txt' if arm else 'block-host.txt')
    evidence.write_text(('Static AArch64 Cortex-A53 under QEMU; no hardware timing.\n' if arm else 'Host isolated block test; no gameplay timing.\n')+result.stdout)
