#!/usr/bin/env python3
"""Test selective direct-call synchronization for the five audited leaf routines."""
import json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
subprocess.run(['python3',str(root/'scripts/probe_leaf_registers_20261102.py')],check=True)
base=root/'build/leaf-registers-20261102';out=root/'build/leaf-calls-20261102';out.mkdir(exist_ok=True)
patches=json.loads((base/'patches.json').read_text());header=['#ifndef THING_AUDITED_LEAF_CALLS_H','#define THING_AUDITED_LEAF_CALLS_H']
wrappers=[]
for i,p in enumerate(patches):
 a=p['address'];macro='THING_LEAF_CALL_'+a
 lines=['do {','uint32_t _thing_leaf_caller=g_cur_func;','PUSH32(esp,RECOMP_RETADDR);','RECOMP_FLAGS_OUT();']
 lines += ['g_'+r+'='+r+';'for r in p['used']]
 lines += ['fn();']
 lines += [r+'=g_'+r+';'for r in p['written']]
 lines += ['g_cur_func=_thing_leaf_caller;','} while(0)']
 header += ['#if defined(RECOMP_GENERATED_CODE) && defined(RECOMP_LOCAL_REGS)','#define '+macro+'(fn) '+' '.join(lines),'#else','#define '+macro+'(fn) RECOMP_CALL(fn)','#endif']
 for kind,call in [('reference','RECOMP_CALL(reference'+str(i)+')'),('candidate',macro+'(candidate'+str(i)+')')]:
  wrappers.append('''__attribute__((noinline)) static void call_'''+kind+str(i)+'''(void){
 RECOMP_REGS_LOCALS
 uint32_t _flag_k=0,_flag_a=0,_flag_b=0,_cf=0;
 RECOMP_ENTER(0x123456u);
 /* Make locals deliberately differ from the stale global copies. */
 eax^=0x10000u;ebx^=0x23456u;esi^=0x34567u;edi^=0x45678u;ebp^=0x56789u;
 ecx+=16;edx+=16;MEM32(esp)=1024;
 _flag_a^=0x654321u;
 '''+call+''';
 RECOMP_FLAGS_IN();RECOMP_FLAGS_OUT();RECOMP_REGS_OUT();
}
''')
header+=['#endif'];(out/'leaf_calls.h').write_text('\n'.join(header)+'\n')
s=(base/'probe.c').read_text();mark='static unsigned char mem[4096]';assert mark in s
s=s.replace(mark,'#include "leaf_calls.h"\n'+''.join(wrappers)+'\n'+mark)
s=s.replace('{reference0,reference1,reference2,reference3,reference4}', '{call_reference0,call_reference1,call_reference2,call_reference3,call_reference4}')
s=s.replace('{candidate0,candidate1,candidate2,candidate3,candidate4}', '{call_candidate0,call_candidate1,call_candidate2,call_candidate3,call_candidate4}')
s=s.replace('exact leaf register/memory/x87/flags comparisons','exact selective-call register/memory/x87/flags comparisons')
(out/'probe.c').write_text(s)
print('Generated selective-call ABI probe; game unchanged')
