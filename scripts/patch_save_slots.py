#!/usr/bin/env python3
"""Restore original missing-save catch path, only at the slot-scan callsite."""
import hashlib,json,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
image=(ROOT/'build/active/bin/thething.exe').read_bytes()
assert hashlib.sha256(image).hexdigest()=='085d1a90b2bd57967e8e911eb1f7c2232fd4b045b7f363c5c4a3056f720abb49'
# This executable's .text raw offsets equal RVA (verified PE section mapping).
pe=struct.unpack_from('<I',image,60)[0];n=struct.unpack_from('<H',image,pe+6)[0];size=struct.unpack_from('<H',image,pe+20)[0]
def read(va,nbytes):
 for i in range(n):
  o=pe+24+size+40*i;vsize,rva,rawsize,raw=struct.unpack_from('<IIII',image,o+8)
  if rva<=va-0x400000<rva+rawsize:return image[raw+va-0x400000-rva:raw+va-0x400000-rva+nbytes]
 raise ValueError(hex(va))
assert read(0x4063fd,17)==bytes.fromhex('6a018d55a88d4dd8c645fc07e872c9ffff')
assert read(0x40648d,6)==bytes.fromhex('b893644000c3')
p=ROOT/'build/translated/thething/functions_0001.c';s=p.read_text()
marker='    /* restored original missing-save catch path */\n'
needle='    PUSH32(esp, 1); /* 0x004063FD: push 1 */'
count=s.count(needle);assert count==8,count
hook='''    /* restored original missing-save catch path */
    if(thing_save_slot_absent(MEM32(ebp + (-0x54)))) {
        /* Unwind the slot-name string exactly as the successful path does.
         * No stream was constructed; [ebp-0x18] was initialized to zero. */
        PUSH32(esp, 1);
        ecx = ebp + (-0x58);
        MEM8(ebp + (-0x4)) = 6;
        RECOMP_CALL(recomp_thething_0040A080);
        RECOMP_FLAGS_IN();
        RECOMP_CALL(recomp_thething_0040644A);
        RECOMP_FLAGS_IN();
        ebx = MEM32(ebp + (-0x20));
        edi = MEM32(ebp + (-0x18));
        goto L_00406499;
    }
'''
if marker not in s:
 s=s.replace(needle,hook+needle)
 s=s.replace('#include "functions.h"','#include "functions.h"\nextern int thing_save_slot_absent(uint32_t);',1)
 p.write_text(s)
else:assert s.count(marker)==count
print(json.dumps({'sites':count,'original_call':'0x406409','original_catch':'0x40644a','resume':'0x406499','scope':'only absent Slot1.sav..Slot8.sav in slot enumeration'},indent=2))
# The load-menu slot scan has the same try/catch with different saved registers.
assert read(0x402987,17)==bytes.fromhex('6a018d55a88d4dd8c645fc07e8e8030000')
assert read(0x402a19,6)==bytes.fromhex('b81f2a4000c3')
p=ROOT/'build/translated/thething/functions_0000.c';s=p.read_text()
needle='    PUSH32(esp, 1); /* 0x00402987: push 1 */'
count=s.count(needle);assert count==7,count
hook=hook.replace('recomp_thething_0040644A','recomp_thething_004029D4').replace('ebx = MEM32(ebp + (-0x20));\n        edi = MEM32(ebp + (-0x18));','ebx = MEM32(ebp + (-0x20));\n        esi = MEM32(ebp + (-0x18));').replace('L_00406499','L_00402A25')
if marker not in s:
 s=s.replace(needle,hook+needle).replace('#include "functions.h"','#include "functions.h"\nextern int thing_save_slot_absent(uint32_t);',1);p.write_text(s)
else:assert s.count(marker)==count
print('load-menu missing-slot catch: restored',count,'translated aliases')
