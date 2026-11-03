#!/usr/bin/env python3
"""Additive controlled decal A/B; never package original game assets or saves."""
import hashlib,json,subprocess,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1];sha=lambda b:hashlib.sha256(b).hexdigest()
b=root/'build/cpu-audio-decal-option-aarch64-20261102';debug=b/'diagnostic.aarch64'
manifest=json.loads((root/'docs/evidence/cpu-audio-decal-option-aarch64-20261102/build-manifest.json').read_text())
assert sha(debug.read_bytes())==manifest['diagnostic_sha256']
e=root/'docs/evidence/cpu-audio-decal-option-arm-play-20261102';log=(e/'run.log').read_text()
assert (e/'exit.txt').read_text().strip()=='0'
assert json.loads((e/'metadata.json').read_text())['binary_sha256']==manifest['diagnostic_sha256']
assert 'level1.dll' in log and 'intro.dll' not in log and 'mode=small-decals' in log
assert '[native-vorbis-probe] decoded' in log
assert 'GAME FAULT' not in log and '[d3d8-render-error]' not in log
assert 'AArch64' in subprocess.check_output(['readelf','-h',str(debug)],text=True)
exe=(b/'thething-decal-option.aarch64').read_bytes();codec=(root/'build/native-vorbis-arm/probe.aarch64.so').read_bytes()
assert sha(codec)=='dc4bee95be2d5b7e858e430d1dcce17f39e0f9ef3e7b5113ddb872ee082bc6d2'
# Reuse tested session/root handling; only candidate settings and filenames differ.
with zipfile.ZipFile(root/'dist/the-thing-cpu-combined-arm-20261102.zip')as z:
 run=z.read('ports/the-thing/run-game-cpu-combined.sh').decode()
 measure=z.read('Roms/PORTS/The Thing Measure CPU Combined.sh').decode()
files={'ports/the-thing/thething-decal-option.aarch64':exe,'ports/the-thing/libthing-decal-audio.aarch64.so':codec}
for n in ('sample_device.sh','sample_device_child.sh'):files['ports/the-thing/'+n]=(root/'scripts'/n).read_bytes()
for mode,label in [(0,'OFF'),(1,'ON')]:
 launcher=f'run-game-decal-option-{mode}.sh'
 settings=f'THING_DRAW_BATCH=0 DS_DRAW_BATCH=0 THING_DECAL_BATCH={mode} DS_DECAL_BATCH={mode}'
 r=run.replace('thething-cpu-combined.aarch64','thething-decal-option.aarch64').replace('libthing-audio.aarch64.so','libthing-decal-audio.aarch64.so').replace('THING_DRAW_BATCH=1 DS_DRAW_BATCH=1',settings)
 assert settings in r
 files['ports/the-thing/'+launcher]=r.encode()
 m=measure.replace('thething-cpu-combined.aarch64','thething-decal-option.aarch64').replace('libthing-audio.aarch64.so','libthing-decal-audio.aarch64.so').replace('run-game-cpu-combined.sh',launcher).replace('cpucombo1-',f'decal-option-{mode}-').replace('THING_DRAW_BATCH=1 DS_DRAW_BATCH=1',settings).replace('opaque_batch=on',f'opaque_batch=off decal_batch={mode}')
 m=m.replace('    trap - EXIT INT TERM HUP','''    trap - EXIT INT TERM HUP
    if [[ -n ${sampler_pid:-} ]]; then
        kill -TERM "$sampler_pid" 2>/dev/null || true
        wait "$sampler_pid" 2>/dev/null || true
    fi''')
 m=m.replace('measure_pid=$!\nwait "$measure_pid"','''measure_pid=$!
bash "$gamedir/sample_device_child.sh" "$measure_pid" "$gamedir/thething-decal-option.aarch64" "$run/device-telemetry.csv" > "$run/device-telemetry.log" 2>&1 &
sampler_pid=$!
wait "$measure_pid"''')
 for prefix in ('Roms/PORTS','ports'):files[f'{prefix}/The Thing Measure Decals {label}.sh']=m.encode()
readme='''The Thing: optional decal batching experiment

The prebuilt ARM executable is provided for the review.
Requires your working port and your own original game files. No original game
assets, ISO, save files, configs or recorded draw fixtures are included.

Extract at the SD-card root, merging folders. Payload goes in ports/the-thing;
shortcuts are supplied in Roms/PORTS for muOS and ports for PortMaster layouts.
The normal executable/launcher and your saves/configs are not replaced.

Run The Thing Measure Decals OFF, then ON. Both use this SAME new executable,
native audio,83 existing floating-point routine optimizations,unused-uniform
optimization,640x480,SINGLE,buffer cache OFF,opaque batching OFF and uncappedFPS.
Only the opt-in small-decal batching setting differs. Normal defaults are unchanged.

Load the SAME save and repeat the same movement route. For footprint testing,
run several laps on the snow, face the same wall at the same distance, then stop
moving for60seconds as tracks fade. Skip the same cutscenes. Repeat OFF/ON if
possible; NPC positions/weather can change CPU load even with similar camera views.
Let each240-second measurement exit automatically. Send both new archives from
ports/the-thing/measurements/*decal-option-*.tar.gz. The archives contain timing,
CPU samples,settings,executable hashes,temperature/frequency/RAM telemetry.
Check the log for mode=small-decals only in ON. Use your normal shortcut to return
to the existing build. Report any altered footprints/shadows,lighting,save/load,
controller or audio behavior; do not keep using a visibly broken candidate.

Evidence and limits:
- 12 existing opaque and24 narrow-decal640x480 pixel tests passed.
- Exact real-decal replay reduced50draws to2 with identical pixels; local CPU
  for that subset .061838->.0312735ms. This is not a whole-game50%gain.
- Earlier gameplay A/B had differing NPC work and no demonstrated totalCPUgain.
- This combined ARM executable passed saved-game gameplay under QEMU/software
  GLES with dummy audio. That verifies neither Mali performance nor audible sync.
- H700 FPS,including stable30FPS,remains unverified. This is an experiment.
- Preserve the native audio improvements on both sides of the comparison.

Build/validation/package commands in the source repository:
python3 scripts/build_cpu_audio_decal_option_20261102.py --target=aarch64
bash scripts/run_cpu_audio_decal_option_arm_20261102.sh
bash scripts/strip_cpu_audio_decal_option_arm_20261102.sh
python3 scripts/package_decal_option_arm_20261102.py
python3 scripts/check_decal_option_package_20261102.py
'''
files['ports/the-thing/DECAL_OPTION_TEST.txt']=readme.encode()
for n in ('LICENSE.ogg','LICENSE.vorbis'):files['ports/the-thing/decal-audio-'+n]=(root/'build/native-vorbis-arm'/n).read_bytes()
files['ports/the-thing/decal-option-manifest.json']=json.dumps({'debug_elf_sha256':manifest['diagnostic_sha256'],'stripped_elf_sha256':sha(exe),'hardware_performance':'unverified','composition':manifest['composition'],'files':{n:sha(d)for n,d in files.items()}},indent=2).encode()
out=root/'dist/the-thing-decal-option-arm-20261102.zip'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=6)as z:
 for n,d in sorted(files.items()):
  if n.endswith('.sh'):subprocess.run(['bash','-n'],input=d,check=True)
  i=zipfile.ZipInfo(n,(1980,1,1,0,0,0));i.create_system=3;i.external_attr=(0o100755 if n.endswith(('.sh','.aarch64','.so'))else 0o100644)<<16;i.compress_type=zipfile.ZIP_DEFLATED;z.writestr(i,d)
with zipfile.ZipFile(out)as z:assert z.testzip() is None
out.with_suffix('.zip.sha256').write_text(sha(out.read_bytes())+'  '+out.name+'\n');print(out,out.stat().st_size)
