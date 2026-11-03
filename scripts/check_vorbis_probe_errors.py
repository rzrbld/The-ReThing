#!/usr/bin/env python3
"""Probe decoder failure outputs and bounded truncated/corrupt streams."""
import ctypes as C
from pathlib import Path
p=Path(__file__).resolve().parents[1]
lib=C.CDLL(str(p/'build/vorbis-probe-deps/probe.so'))
lib.probe_decode.argtypes=[C.c_void_p,C.c_size_t,C.POINTER(C.c_void_p),C.POINTER(C.c_size_t),C.POINTER(C.c_long),C.POINTER(C.c_int),C.POINTER(C.c_longlong)]
blob=(p/'docs/evidence/audio-cpu-20261102/decode-0001.ogg').read_bytes()
cases=[b'',b'bad',b'\0'*4096]+[blob[:n] for n in (1,4,26,27,50,100,512)]
for b in cases:
 ptr=C.c_void_p();size=C.c_size_t(123);rate=C.c_long();channels=C.c_int();frames=C.c_longlong()
 rc=lib.probe_decode(b,len(b),C.byref(ptr),C.byref(size),C.byref(rate),C.byref(channels),C.byref(frames))
 assert rc<0 and ptr.value is None and size.value==0,(len(b),rc,ptr.value,size.value)
print('PASS',len(cases),'invalid/truncated inputs fail with empty output (guest fallback remains possible)')
