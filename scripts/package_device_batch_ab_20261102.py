#!/usr/bin/env python3
"""Same combined ELF/native audio on both sides; vary only opaque batching."""
import hashlib,json,zipfile,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with zipfile.ZipFile(root/'dist/the-thing-cpu-combined-arm-20261102.zip')as z:
 run=z.read('ports/the-thing/run-game-cpu-combined.sh').decode()
 measure=z.read('Roms/PORTS/The Thing Measure CPU Combined.sh').decode()
files={f'ports/the-thing/{n}':(root/'scripts'/n).read_bytes()for n in ('sample_device.sh','sample_device_child.sh')}
for mode,label in [(0,'OFF'),(1,'ON')]:
 launcher=f'run-game-cpucombo-batch{mode}.sh'
 r=run.replace('THING_DRAW_BATCH=1 DS_DRAW_BATCH=1',f'THING_DRAW_BATCH={mode} DS_DRAW_BATCH={mode}')
 assert f'THING_DRAW_BATCH={mode} DS_DRAW_BATCH={mode}' in r
 files['ports/the-thing/'+launcher]=r.encode()
 m=measure.replace('run-game-cpu-combined.sh',launcher).replace('cpucombo1-',f'cpucombo-batch{mode}-').replace('THING_DRAW_BATCH=1 DS_DRAW_BATCH=1',f'THING_DRAW_BATCH={mode} DS_DRAW_BATCH={mode}').replace('opaque_batch=on',f'opaque_batch={label.lower()}')
 m=m.replace('    trap - EXIT INT TERM HUP','''    trap - EXIT INT TERM HUP
    if [[ -n ${sampler_pid:-} ]]; then
        kill -TERM "$sampler_pid" 2>/dev/null || true
        wait "$sampler_pid" 2>/dev/null || true
    fi''')
 m=m.replace('measure_pid=$!\nwait "$measure_pid"','''measure_pid=$!
bash "$gamedir/sample_device_child.sh" "$measure_pid" "$gamedir/thething-cpu-combined.aarch64" "$run/device-telemetry.csv" > "$run/device-telemetry.log" 2>&1 &
sampler_pid=$!
wait "$measure_pid"''')
 files[f'Roms/PORTS/The Thing Measure Combined Batch {label}.sh']=m.encode()
files['ports/the-thing/COMBINED_BATCH_AB.txt']=b'''Controlled batching A/B for the combined build
Requires the existing working port and CPU Combined package. No game binary,
original data, saves or configuration are supplied or replaced.
Extract at the SD-card root. Run The Thing Measure Combined Batch OFF and then
The Thing Measure Combined Batch ON, using the same New Game/skip route.
Both use the SAME combined executable, native audio,83-routine FP changes,
640x480,SINGLE,buffer cacheOFF,index/SIMD experimentsOFF. Only batching differs.
For a useful comparison, face the same slow wall at the same distance and hold
still for at least60seconds in both runs. Let each run auto-exit after240seconds.
Send both new measurements/*cpucombo-batch*.tar.gz archives. Audio improvements
remain enabled in both. Temperature/frequency/RAM sampler now uses /proc/stat
parent relationships, without depending on optional kernel children files.
Sampler discovery is logged even when attachment fails. Read-only telemetry
adds overhead; both sides use the same sampler. No30FPS claim.
'''
files['ports/the-thing/combined-batch-ab-manifest.json']=json.dumps({'combined_elf_sha256':'1a0ff7eaa7f69a727a57abc77d04318f0af11cfe667da3bb5e5235e81759b881','files':{n:hashlib.sha256(d).hexdigest()for n,d in files.items()}},indent=2).encode()
out=root/'dist/the-thing-combined-batch-ab-20261102.zip'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=9)as z:
 for n,d in sorted(files.items()):
  if n.endswith('.sh'):subprocess.run(['bash','-n'],input=d,check=True)
  info=zipfile.ZipInfo(n,(1980,1,1,0,0,0));info.external_attr=(0o100755 if n.endswith('.sh')else 0o100644)<<16;info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,d)
with zipfile.ZipFile(out)as z:assert z.testzip() is None
sha=hashlib.sha256(out.read_bytes()).hexdigest();out.with_suffix('.zip.sha256').write_text(sha+'  '+out.name+'\n');print(out,sha,out.stat().st_size)
