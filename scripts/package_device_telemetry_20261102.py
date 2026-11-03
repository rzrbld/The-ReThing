#!/usr/bin/env python3
"""Additional paired measurement shortcuts; no game binary/config replacement."""
import hashlib,json,zipfile,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
files={f'ports/the-thing/{n}':(root/'scripts'/n).read_bytes()for n in ('sample_device.sh','sample_device_child.sh')}
with zipfile.ZipFile(root/'dist/the-thing-cpu-combined-arm-20261102.zip')as z:
 combined=z.read('Roms/PORTS/The Thing Measure CPU Combined.sh').decode()
baseline=(root/'packaging/The Thing Measure.sh').read_text()
for kind,s,binary in [('Baseline',baseline,'thething.aarch64'),('CPU Combined',combined,'thething-cpu-combined.aarch64')]:
 old='    trap - EXIT INT TERM HUP';new=old+'''\n    if [[ -n ${sampler_pid:-} ]]; then
        kill -TERM "$sampler_pid" 2>/dev/null || true
        wait "$sampler_pid" 2>/dev/null || true
    fi'''
 assert s.count(old)==1;s=s.replace(old,new)
 old='measure_pid=$!\nwait "$measure_pid"';new='''measure_pid=$!
bash "$gamedir/sample_device_child.sh" "$measure_pid" "$gamedir/'''+binary+'''" "$run/device-telemetry.csv" > "$run/device-telemetry.log" 2>&1 &
sampler_pid=$!
wait "$measure_pid"'''
 assert s.count(old)==1;s=s.replace(old,new)
 s=s.replace('-scan${THING_INDEX_RANGE}-$$','-scan${THING_INDEX_RANGE}-telemetry-$$')
 files[f'Roms/PORTS/The Thing Measure {kind} Telemetry.sh']=s.encode()
files['ports/the-thing/DEVICE_TELEMETRY_TEST.txt']=b'''Additional paired device diagnostics, 2026-11-02
Requires the existing working port and CPU Combined test package. No game
executables, original assets, settings or saves are included or replaced.
Extract at the SD-card root, merging ports and Roms.
Run The Thing Measure Baseline Telemetry, then The Thing Measure CPU Combined
Telemetry through the same New Game/skip route, including the slow wall and
first room. Both retain the original240-second measurement settings.
Wait for automatic exit and share the two new measurements/*-telemetry-*.tar.gz.

A read-only sampler follows only the exact game executable descended from the
launched wrapper. Every2seconds it records process RSS/high-water/swap/threads,
system available RAM/free swap, readable temperatures and CPU frequencies.
Telemetry uses system uptime, not profile-relative time. It is not CPU-cycle
measurement; frequency snapshots must not be used to normalize FPS. Missing
sensors are omitted. Background sampling adds some overhead; use both paired
telemetry shortcuts for a comparison. Warm the device similarly for both runs.
Main-thread CPU profiles and per-frame logs remain the primary attribution.
The executable hashes and requested/actual settings remain in settings/logs.
No performance claim; H700 sustained30FPS and telemetry behavior unverified.
'''
files['ports/the-thing/device-telemetry-manifest.json']=json.dumps({'files':{n:hashlib.sha256(d).hexdigest()for n,d in files.items()}},indent=2).encode()
out=root/'dist/the-thing-device-telemetry-20261102.zip'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=9)as z:
 for n,d in sorted(files.items()):
  if n.endswith('.sh'):subprocess.run(['bash','-n'],input=d,check=True)
  info=zipfile.ZipInfo(n,(1980,1,1,0,0,0));info.external_attr=(0o100755 if n.endswith('.sh')else 0o100644)<<16;info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,d)
with zipfile.ZipFile(out)as z:assert z.testzip() is None
sha=hashlib.sha256(out.read_bytes()).hexdigest();out.with_suffix('.zip.sha256').write_text(sha+'  '+out.name+'\n');print(out,sha,out.stat().st_size)
