#!/usr/bin/env python3
"""Validate the private ARM decoder corpus outputs against guest PCM."""
import json,csv,collections,hashlib
from pathlib import Path
import numpy as np
root=Path(__file__).resolve().parents[1]
src=root/'docs/evidence/audio-cpu-20261102';out=root/'docs/evidence/native-vorbis-arm-corpus-20261102'
old={r['id']:r for r in map(json.loads,(src/'decode-calls.jsonl').read_text().splitlines())};rows=[]
for line in csv.reader((out/'decoded.csv').open()):
 i,n,rate,ch,frames,ns=map(int,line);r=old[i];fmt=bytes.fromhex(r['waveformat_hex'])
 assert n==r['pcm_bytes'] and n==frames*ch*2
 assert rate==int.from_bytes(fmt[4:8],'little') and ch==int.from_bytes(fmt[2:4],'little')
 a=np.frombuffer((src/f'decode-{i:04}.pcm').read_bytes(),dtype='<i2').astype(np.int32)
 b=np.frombuffer((out/f'decode-{i:04}.pcm').read_bytes(),dtype='<i2').astype(np.int32)
 assert len(a)==len(b);delta=int(np.abs(a-b).max());assert delta<=1,(i,delta)
 rows.append(dict(id=i,bytes=n,max_delta=delta))
assert len(rows)==282
(out/'comparison.json').write_text(json.dumps(dict(note='ARM native decoder executed under QEMU. Not hardware performance evidence.',library_sha256=hashlib.sha256((root/'build/native-vorbis-arm/probe.aarch64.so').read_bytes()).hexdigest(),results=rows),indent=2)+'\n')
print('PASS ARM QEMU corpus: 282 lengths/formats match, max PCM delta <=1:',collections.Counter(r['max_delta']for r in rows))
