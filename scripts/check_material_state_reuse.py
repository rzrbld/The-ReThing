#!/usr/bin/env python3
"""Check material-state reuse against user-owned original x86 code (Unicorn)."""
from pathlib import Path
import ctypes,subprocess,random,struct,pefile,hashlib
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
p=Path('build/material-state-check');p.mkdir(parents=True,exist_ok=True)
(p/'check.c').write_text('#include <stdint.h>\n#include "../../runtime/material_state_reuse.h"\nint check(unsigned char *m,unsigned *rs,int recording){return !recording && thing_material_prefix_matches(m,rs); }\n')
subprocess.run(['cc','-shared','-fPIC','-O2',str(p/'check.c'),'-o',str(p/'check.so')],check=True)
lib=ctypes.CDLL(str(p.resolve()/'check.so'));lib.check.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int]
exe=Path('build/active/bin/thething.exe')
assert hashlib.sha256(exe.read_bytes()).hexdigest()=='085d1a90b2bd57967e8e911eb1f7c2232fd4b045b7f363c5c4a3056f720abb49', 'unsupported original executable'
pe=pefile.PE(str(exe));u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,0x600000);u.mem_write(0x400000,pe.get_memory_mapped_image())
u.mem_map(0x2000000,0x10000);mat=0x2001000;wrapper=0x2002000;dev=0x2003000;vt=0x2004000;sp=0x200f000;stub=0x2005000
u.mem_write(wrapper+0x538,struct.pack('<I',dev));u.mem_write(dev,struct.pack('<I',vt));u.mem_write(vt+0xc8,struct.pack('<I',stub))
rs={};calls=[]
def hook(u,a,size,data):
 if a==stub:
  s=u.reg_read(UC_X86_REG_ESP);ret,obj,k,v=struct.unpack('<4I',u.mem_read(s,16));rs[k]=v;calls.append((k,v));u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,s+16);u.reg_write(UC_X86_REG_EIP,ret)
u.hook_add(UC_HOOK_CODE,hook)
regs=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EFLAGS]
rng=random.Random(76);matches=0
for trial in range(2000):
 m=bytearray(rng.randbytes(512));m[0x13]=trial%3
 # Cover common values and out-of-range inherited enum behavior.
 for k,maxv in [(0x12,2),(0x15,2),(0x1b,2),(0x1d,2),(0x16,5),(0x17,6),(0x19,10),(0x1c,12)]:m[k]=rng.randrange(maxv)
 u.mem_write(mat,bytes(m));initial=[rng.getrandbits(32) for _ in regs];initial[4]=wrapper;initial[6]=mat;initial[7]=sp;initial[8]=2
 def run(start,states):
  global rs,calls
  rs=dict(enumerate(states));calls=[]
  for r,v in zip(regs,initial):u.reg_write(r,v)
  u.emu_start(start,0x76fe3f,count=10000)
  assert u.reg_read(UC_X86_REG_EIP)==0x76fe3f
  return [rs[i] for i in range(256)],[u.reg_read(r) for r in regs]
 states=[rng.getrandbits(32) for _ in range(256)];target,_=run(0x76fc46,states)
 # Half of trials retain correct prefix states; half perturb one state.
 if trial%2:target[rng.randrange(256)]=rng.getrandbits(32)
 ma=(ctypes.c_ubyte*512).from_buffer_copy(m);sa=(ctypes.c_uint*256)(*target)
 hit=lib.check(ma,sa,0)
 assert lib.check(ma,sa,1)==0
 if hit:
  matches+=1;a,ar=run(0x76fc46,target);b,br=run(0x76fe24,target)
  assert a==b,(trial,'states')
  # FE3F immediately replaces flags. All other live/scratch GPRs match.
  assert ar[:-1]==br[:-1],(trial,'regs',ar,br)
print(f'PASS 2000 original-x86 trials; {matches} accepted bypasses, states and GPRs identical; recording rejected')
