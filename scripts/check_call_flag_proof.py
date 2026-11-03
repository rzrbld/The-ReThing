#!/usr/bin/env python3
"""Reject unsafe call-entry assumptions: carry use, branch, tail and native hooks."""
import importlib.util
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('audit',root/'scripts/audit_call_flag_inputs.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Image:
 def __init__(self,data):self.d=data
 def raw(self,va):return 0
va=0x401000
body='''
 uint32_t _cf=0,_flag_a=0,_flag_b=0,_flag_k=FK_NONE;
 RECOMP_REGS_LOCALS
 RECOMP_ENTER(0x00401000u);
 L_00401000:
 _flag_a=esp;_flag_b=16;_cf=CMP_B(_flag_a,_flag_b);
 esp-=16; /* 0x00401000: sub esp, 0x10 */
 _flag_k=FK_CMP;
 /* 0x00401003: nop */
 RECOMP_FLAGS_OUT();return;
'''
m.image=Image(bytes.fromhex('83ec1090c3'));assert m.prove(va,body)
for prefix in ('if (hook()) return;','native_hook();'):
 bad=body.replace('L_00401000:', 'L_00401000:\n'+prefix)
 assert not m.prove(va,bad),prefix
assert not m.prove(va,body.replace('RECOMP_ENTER(0x00401000u);','if (hook()) return;\nRECOMP_ENTER(0x00401000u);'))
assert not m.prove(va,body.replace('_cf=CMP_B(_flag_a,_flag_b);',''))
for data in ('740183ec1090c3','11c083ec1090c3','40'+'83ec1090c3','e90000000083ec1090c3','e80000000083ec1090c3','c3','9c83ec1090c3','d1d083ec1090c3'):
 m.image=Image(bytes.fromhex(data));assert not m.prove(va,body),data
print('PASS prefix proof accepts full overwrite; rejects incoming-flag branch/carry, partial write, tail, call, return, pushf, rotate and native hooks')
