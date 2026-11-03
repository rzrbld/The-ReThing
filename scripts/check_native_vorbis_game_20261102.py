#!/usr/bin/env python3
"""Verify captured integrated native output against original game fixtures."""
import json
from pathlib import Path
import numpy as np
p=Path(__file__).resolve().parents[1]/'docs/evidence'
a=p/'audio-cpu-20261102';b=p/'native-vorbis-game-pcm-20261102'
def rows(path):return [json.loads(s) for s in (path/'decode-calls.jsonl').read_text().splitlines()]
old={r['sha256']:r for r in rows(a) if 'sha256'in r};new=rows(b);checks=[]
for r in new:
 assert 'error' not in r,r
 prior=old[r['sha256']]
 assert r['pcm_bytes']==prior['pcm_bytes'] and r['waveformat_hex']==prior['waveformat_hex'],r
 assert r['return_eax']&255==prior['return_eax']&255==1,r
 x=np.frombuffer((a/f"decode-{prior['id']:04}.pcm").read_bytes(),dtype='<i2').astype(np.int32)
 y=np.frombuffer((b/f"decode-{r['id']:04}.pcm").read_bytes(),dtype='<i2').astype(np.int32)
 assert len(x)==len(y)
 delta=int(np.abs(x-y).max()) if len(x) else 0
 assert delta<=1,(r,delta)
 checks.append(dict(sha256=r['sha256'],pcm_bytes=r['pcm_bytes'],max_sample_delta=delta))
assert len(checks)==len(old)==282,(len(checks),len(old))
log=(p/'native-vorbis-game-20261102/run.log').read_text()
assert '[native-vorbis-probe] decoded' in log
assert 'level1.dll' in log
assert (p/'native-vorbis-game-20261102/exit.txt').read_text().strip()=='0'
(b/'comparison.json').write_text(json.dumps(dict(checks=checks,note='Host integrated whole-sound path only. Hardware FPS, streaming path and audible output unverified.'),indent=2)+'\n')
print('PASS 282 integrated native decodes: length/format/success preserved, PCM delta <=1; saved level1 run exit 0')
