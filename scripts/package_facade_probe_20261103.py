#!/usr/bin/env python3
import hashlib,json,subprocess,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1];sha=lambda b:hashlib.sha256(b).hexdigest()
b=root/'build/facade-probe-aarch64-20261103';manifest=json.loads((root/'docs/evidence/facade-probe-aarch64-20261103/build-manifest.json').read_text())
assert sha((b/'diagnostic.aarch64').read_bytes())==manifest['diagnostic_sha256']
assert 'AArch64' in subprocess.check_output(['readelf','-h',str(b/'thething-facade.aarch64')],text=True)
for target,name in [('host','facade-probe-host-check-20261103'),('aarch64','facade-probe-arm-final-20261103')]:
 evidence=root/'docs/evidence'/name
 assert (evidence/'exit.txt').read_text().strip()=='0'
 log=(evidence/'run.log').read_text()
 assert 'level1.dll' in log and '[facade-probe] frames=' in log
 assert 'GAME FAULT' not in log and '[d3d8-render-error]' not in log
 build=json.loads((root/f'docs/evidence/facade-probe-{target}-20261103/build-manifest.json').read_text())
 assert json.loads((evidence/'metadata.json').read_text())['binary_sha256']==build['diagnostic_sha256']
 subprocess.run(['python3',str(root/'scripts/analyze_facade_probe_20261103.py'),str(evidence),'--from-seconds','90' if target=='host' else '150','--to-seconds','100' if target=='host' else '180','--output',str(evidence/'validated-counters.json')],check=True)
with zipfile.ZipFile(root/'dist/the-thing-cpu-clip-arm-20261103.zip') as z:
 run=z.read('ports/the-thing/run-game-cpu-clip-0.sh').decode()
 measure=z.read('ports/The Thing Measure CPU Clip OFF.sh').decode()
run=run.replace('thething-cpu-clip.aarch64','thething-facade.aarch64').replace('libthing-cpu-clip-audio','libthing-facade-audio')
run=run.replace('export THING_CPU_CLIP=0 DS_CPU_CLIP=0','export THING_CPU_CLIP=0 DS_CPU_CLIP=0 THING_FACADE_PROBE=1 THING_PERF_PROBE=1')
measure=measure.replace('thething-cpu-clip.aarch64','thething-facade.aarch64').replace('libthing-cpu-clip-audio','libthing-facade-audio').replace('run-game-cpu-clip-0.sh','run-game-facade.sh')
measure=measure.replace('files=(log.txt','files=(facade-calls.csv facade-frame.csv log.txt')
measure=measure.replace('export THING_CPU_PROFILE=1','export THING_FACADE_PROBE=1\nexport THING_CPU_PROFILE=1')
measure=measure.replace('&& -s "$run/render.csv" ]]; then','&& -s "$run/render.csv" && -s "$run/facade-calls.csv" && -s "$run/facade-frame.csv" ]]; then')
measure=measure.replace('cpu_clip=0"','cpu_clip=0 facade_probe=1"')
files={'ports/the-thing/thething-facade.aarch64':(b/'thething-facade.aarch64').read_bytes(),'ports/the-thing/libthing-facade-audio.aarch64.so':(root/'build/native-vorbis-arm/probe.aarch64.so').read_bytes(),'ports/the-thing/run-game-facade.sh':run.encode()}
for n in ('sample_device.sh','sample_device_child.sh'):files['ports/the-thing/'+n]=(root/'scripts'/n).read_bytes()
for scene in ('Wall','Side','Open'):
 s=measure.replace('cpu-clip-0-cache',f'facade-{scene.lower()}-cache')
 for folder in ('Roms/PORTS','ports'):files[f'{folder}/The Thing Measure Facade {scene}.sh']=s.encode()
readme='''The Thing: HLE/D3D8 facade counters, 2026-11-03

The prebuilt ARM executable is provided for the review.
Requires your working port and your own game files. Add-on only; no original
assets, ISO, save files or configs. Extract at SD-card root, merging folders.
Payload belongs in ports/the-thing; shortcuts in Roms/PORTS (muOS) or ports
(PortMaster). Normal game executable/launcher/settings are not replaced.

Use The Thing Measure Facade Wall first:
1. Start New Game; skip intros and cutscenes; dismiss the three tutorial tips.
2. Walk forward about two seconds to face the wall, then stop and hold the view.
3. Try to reach the position within90seconds after starting the shortcut.
4. Let the240-second capture finish automatically.

Optionally use The Thing Measure Facade Side next: repeat the same route,
then turn roughly90degrees from the wall at the same position and hold.

Then use The Thing Measure Facade Open:
Repeat the same route, then turn around to face the open area FROM THE SAME
POSITION. Stop and hold that view for the rest of the capture. These shortcuts
only label the archive: they do NOT move/turn the player automatically.
Please mention roughly when you reached each position if it took over90seconds.
Send the ports/the-thing/measurements/*facade-*.tar.gz archives and identify
which views you captured (Wall0degrees,Side~90degrees,Open~180degrees).

All views use the SAME diagnostic executable,640x480,SINGLE,buffer cacheOFF,
CPU clippingOFF,opaque/decal batchingOFF,native audioON,uncappedFPS.
This is an instrumented diagnostic, not an optimized build or FPS A/B.
Use your original shortcut for normal play afterward.

facade-calls.csv: exact entries into each HLE shim by completed Present frame,
including every D3D8 COM slot, buffer lock/unlock, getters,setters and draws.
facade-frame.csv: lookup attempts/hits,all shim entries,D3D8 entries,and same-value
SetRenderState/TextureStageState/Texture/StreamSource/Indices/VertexShader counts.
Same-value calls still execute normally; no behavior is skipped. State-block
application is counted separately but not included in same-value counters.
Counts include all guest threads under the serialized guest machine lock.
Per-call path does not timestamp,allocate,format strings,write logs or query GL.
Counters are written at Present with buffered I/O; emission CPU cost is logged
on exit. This reports emission cost only, NOT all instrumentation overhead.
No GPU queries,glFinish or forced synchronization are added.

Use existing CPU-clock samples to determine TIME spent in dispatch/facade/driver.
22k calls/frame alone would not prove dispatch is the bottleneck. Correlate actual
call rate,repeated states,draw volume and sampledCPU. render.csv is pre-renderer
guest draw count, not actual low-level GL call volume. Existing GL totals are
logged separately. A final incomplete frame is not written.

Builds validated locally with software GLES; H700 counter capture is unverified
until your run. Timing under emulation is not H700 performance.

Reproduce in source workspace (build targets sequentially):
python3 scripts/build_facade_probe_20261103.py
bash scripts/run_facade_newgame_20261103.sh
python3 scripts/build_facade_probe_20261103.py --target=aarch64
bash scripts/run_facade_arm_20261103.sh
bash scripts/strip_facade_probe_arm_20261103.sh
python3 scripts/package_facade_probe_20261103.py
python3 scripts/check_facade_package_20261103.py
python3 scripts/analyze_facade_probe_20261103.py /path/to/extracted/capture --from-seconds 90 --to-seconds 240 --output summary.json
'''
files['ports/the-thing/FACADE_PROBE_README.txt']=readme.encode()
(root/'docs/FACADE_PROBE_TEST_20261103.txt').write_text(readme)
for n in ('LICENSE.ogg','LICENSE.vorbis'):files['ports/the-thing/facade-audio-'+n]=(root/'build/native-vorbis-arm'/n).read_bytes()
files['ports/the-thing/facade-manifest.json']=json.dumps({'debug_elf_sha256':manifest['diagnostic_sha256'],'stripped_elf_sha256':sha(files['ports/the-thing/thething-facade.aarch64']),'hardware_capture':'unverified','composition':manifest['composition'],'files':{n:sha(d) for n,d in files.items()}},indent=2).encode()
out=root/'dist/the-thing-facade-probe-arm-20261103.zip'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for n,d in sorted(files.items()):
  if n.endswith('.sh'):subprocess.run(['bash','-n'],input=d,check=True)
  i=zipfile.ZipInfo(n,(1980,1,1,0,0,0));i.create_system=3;i.external_attr=(0o100755 if n.endswith(('.sh','.aarch64','.so')) else 0o100644)<<16;i.compress_type=zipfile.ZIP_DEFLATED;z.writestr(i,d)
with zipfile.ZipFile(out) as z:assert z.testzip() is None
out.with_suffix('.zip.sha256').write_text(sha(out.read_bytes())+'  '+out.name+'\n');print(out,out.stat().st_size)
