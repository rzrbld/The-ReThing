#!/usr/bin/env python3
"""Additive CPU clip experiment: no game assets, saves or configuration."""
import hashlib,json,subprocess,zipfile,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sha=lambda b:hashlib.sha256(b).hexdigest()
# Reject packaging when final gameplay verification was not completed.
verification=root/'docs/evidence/cpu-clip-local-2-20261103'
vlog=(verification/'run.log').read_text()
assert (verification/'exit.txt').read_text().strip()=='0'
assert 'level1.dll' in vlog and 'mode=2 ' in vlog
checks=re.findall(r'verified=(\d+) visible_errors=(\d+)',vlog)
assert checks and max(int(n) for n,e in checks)>2000 and all(int(e)==0 for n,e in checks)
assert 'GAME FAULT' not in vlog and '[d3d8-render-error]' not in vlog
assert json.loads((verification/'metadata.json').read_text())['binary_sha256']==json.loads((root/'docs/evidence/cpu-clip-host-20261103/build-manifest.json').read_text())['diagnostic_sha256']
arm=root/'docs/evidence/cpu-clip-arm-play-20261103'
alog=(arm/'run.log').read_text()
assert (arm/'exit.txt').read_text().strip()=='0'
assert 'level1.dll' in alog and 'mode=1 ' in alog and 'GAME FAULT' not in alog
assert '[d3d8-render-error]' not in alog
assert json.loads((arm/'metadata.json').read_text())['binary_sha256']==json.loads((root/'docs/evidence/cpu-clip-aarch64-20261103/build-manifest.json').read_text())['diagnostic_sha256']
readme='''The Thing: conservative CPU clip experiment (2026-11-03)

The prebuilt ARM executable is provided for the review.
Requires your working port and your own original game files. No original assets,
ISO, save files or configuration are included. This is NOT a full installation.

This opt-in candidate rejects XYZ triangles wholly outside the camera clip volume
before GPU upload/driver calls. It does NOT provide room/portal occlusion or skip
CPU animation of hidden objects. LOD selection already operates in the guest.
Default game executable, launcher and settings are not replaced.

H700: extract the ARM ZIP at your SD-card root, merging folders. Payload belongs
in ports/the-thing. Shortcuts are provided in Roms/PORTS (muOS) and ports
(PortMaster). Use The Thing CPU Clip OFF / ON for normal testing. Both launch the
same executable; only clipping differs. They force 640x480, SINGLE, buffer cache
OFF, batching OFF, native audio ON. Your original shortcut remains available.

For measurements, warm up the handheld then run The Thing Measure CPU Clip OFF,
then ON. Load the SAME save, skip the same cutscenes, face the same hangar wall
at the SAME position and wait without moving. Each run ends after 240 seconds
including startup. Repeat in reverse order if possible. Also test the open area
and walking into the first room. Send both new *cpu-clip-*.tar.gz files from
ports/the-thing/measurements. They include hashes, timings, CPU samples and
RAM/temperature/frequency telemetry. Do not compare different camera views.

Local Linux x86_64: extract the LOCAL ZIP anywhere. Provide an absolute path to
your working installed game folder (containing bin/, data/ and userdata/):
  bash run-local.sh /absolute/path/to/installed/the-thing 0
  bash run-local.sh /absolute/path/to/installed/the-thing 1
This local build targets this development machine: glibc >= 2.43, SDL2,
SDL2_ttf, GLESv2/EGL and libraries reported by ldd are required. It is not a
portable build for older Linux distributions. Its development RUNPATH falls
back to system libraries when that directory is absent.
Runs windowed at 640x480 with the same renderer settings. Logs/profiles are
written under the specified game folder; existing saves are used. It is uncapped.
Back up saves before testing. For a timed capture use:
  THING_CPU_PROFILE=1 THING_PROFILE_SECONDS=240 bash run-local.sh /path/to/game 1
Optional mode 2 keeps every draw and checks proposed rejections with GPU queries;
this is a slow correctness diagnostic, never a performance comparison.

Check for missing geometry at screen edges, room transitions, shadows/effects,
and camera rotations. Report any differences and preserve the log. Check that
[cpu-clip] mode=0 or mode=1 matches the selected shortcut.

Evidence: preliminary paired software-GLES wall sample was 134.2 -> 139.0 FPS,
1% low 79.5 -> 83.1 (single pair, not a hardware prediction). A turned-camera
comparison was discarded because camera positions differed. No claim of stable
30 FPS on H700. See docs/evidence/cpu-clip-comparison-20261103/RESULTS.txt in the
source repository for final verification and limitations.

Reproduce in the prepared source workspace:
  python3 scripts/build_cpu_clip_20261103.py
  python3 scripts/build_cpu_clip_20261103.py --target=aarch64
  bash scripts/run_cpu_clip_local_20261103.sh 2
  bash scripts/run_cpu_clip_arm_20261103.sh
  bash scripts/strip_cpu_clip_arm_20261103.sh
  strip --strip-unneeded -o build/cpu-clip-host-20261103/thething-cpu-clip.host build/cpu-clip-host-20261103/diagnostic.host
  python3 scripts/package_cpu_clip_20261103.py
Build host and ARM sequentially: the builder temporarily stages translated code
and restores production files by SHA256. ARM uses the pinned offline Docker image
specified in the builder. This add-on does not change the production installer.
'''
with zipfile.ZipFile(root/'dist/the-thing-decal-option-arm-20261102.zip') as z:
 run=z.read('ports/the-thing/run-game-decal-option-0.sh').decode()
 measure=z.read('Roms/PORTS/The Thing Measure Decals OFF.sh').decode()
for target,kind in [('aarch64','arm'),('host','local')]:
 b=root/f'build/cpu-clip-{target}-20261103'
 manifest=json.loads((root/f'docs/evidence/cpu-clip-{target}-20261103/build-manifest.json').read_text())
 assert sha((b/f'diagnostic.{target}').read_bytes())==manifest['diagnostic_sha256']
 exe=(b/f'thething-cpu-clip.{target}').read_bytes()
 files={}
 if target=='aarch64':
  prefix='ports/the-thing/'
  files[prefix+'thething-cpu-clip.aarch64']=exe
  files[prefix+'libthing-cpu-clip-audio.aarch64.so']=(root/'build/native-vorbis-arm/probe.aarch64.so').read_bytes()
  for n in ('sample_device.sh','sample_device_child.sh'):files[prefix+n]=(root/'scripts'/n).read_bytes()
  for mode,label in [(0,'OFF'),(1,'ON')]:
   launcher=f'run-game-cpu-clip-{mode}.sh'
   def adapt(s):
    s=s.replace('thething-decal-option.aarch64','thething-cpu-clip.aarch64').replace('libthing-decal-audio.aarch64.so','libthing-cpu-clip-audio.aarch64.so').replace('run-game-decal-option-0.sh',launcher).replace('decal-option-0-',f'cpu-clip-{mode}-')
    s=s.replace('export THING_INDEX_RANGE=0',f'export THING_CPU_CLIP={mode} DS_CPU_CLIP={mode}\nexport THING_INDEX_RANGE=0')
    s=s.replace('export THING_GPU_VERTEX=1 DS_GPU_VERTEX=1',f'export THING_CPU_CLIP={mode} DS_CPU_CLIP={mode}\nexport THING_GPU_VERTEX=1 DS_GPU_VERTEX=1')
    return s.replace('decal_batch=0"',f'decal_batch=0 cpu_clip={mode}"')
   r=adapt(run);m=adapt(measure)
   files[prefix+launcher]=r.encode()
   for folder in ('Roms/PORTS','ports'):
    files[f'{folder}/The Thing CPU Clip {label}.sh']=r.encode()
    files[f'{folder}/The Thing Measure CPU Clip {label}.sh']=m.encode()
  for n in ('LICENSE.ogg','LICENSE.vorbis'):files[prefix+'cpu-clip-audio-'+n]=(root/'build/native-vorbis-arm'/n).read_bytes()
 else:
  prefix=''
  files['thething-cpu-clip.host']=exe
  files['libthing-cpu-clip-audio.host.so']=(root/'build/vorbis-probe-deps/stream-probe.so').read_bytes()
  files['run-local.sh']=b'''#!/bin/bash
set -euo pipefail
payload=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
[[ $# -ge 1 && -d "$1/bin" && -d "$1/data" ]] || { echo "Usage: bash run-local.sh /path/to/installed/game [0|1|2]" >&2; exit 1; }
export THING_ROOT=$(cd -- "$1" && pwd)
mode=${2:-1}
case "$mode" in 0|1|2) ;; *) echo "Mode must be 0, 1 or 2" >&2; exit 1;; esac
export THING_CPU_CLIP=$mode DS_CPU_CLIP=$mode
export THING_NATIVE_STREAM_PROBE="$payload/libthing-cpu-clip-audio.host.so" THING_NATIVE_VORBIS_PROBE="$payload/libthing-cpu-clip-audio.host.so"
export THING_WINDOWED=1 THING_GPU_VERTEX=1 DS_GPU_VERTEX=1
export THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 DS_RENDER_WIDTH=640 DS_RENDER_HEIGHT=480
export DS_RENDER_THREAD=0 THING_GPU_BUFFER_CACHE=0 THING_DRAW_BATCH=0 DS_DRAW_BATCH=0 THING_DECAL_BATCH=0 DS_DECAL_BATCH=0
export THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0 THING_FPS_LIMIT=0
cd "$THING_ROOT"
"$payload/thething-cpu-clip.host" 2>&1 | tee "cpu-clip-$mode.log"
'''
  for n in ('LICENSE.ogg','LICENSE.vorbis'):files['cpu-clip-audio-'+n]=(root/'build/native-vorbis-arm'/n).read_bytes()
 files[prefix+'CPU_CLIP_README.txt']=readme.encode()
 files[prefix+'cpu-clip-manifest.json']=json.dumps({'debug_elf_sha256':manifest['diagnostic_sha256'],'stripped_elf_sha256':sha(exe),'hardware_performance':'unverified','composition':manifest['composition'],'files':{n:sha(d) for n,d in files.items()}},indent=2).encode()
 out=root/f'dist/the-thing-cpu-clip-{kind}-20261103.zip'
 with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for n,d in sorted(files.items()):
   if n.endswith('.sh'):subprocess.run(['bash','-n'],input=d,check=True)
   i=zipfile.ZipInfo(n,(1980,1,1,0,0,0));i.create_system=3;i.external_attr=(0o100755 if n.endswith(('.sh','.aarch64','.host','.so')) else 0o100644)<<16;i.compress_type=zipfile.ZIP_DEFLATED;z.writestr(i,d)
 with zipfile.ZipFile(out) as z:assert z.testzip() is None
 out.with_suffix('.zip.sha256').write_text(sha(out.read_bytes())+'  '+out.name+'\n')
 print(out,out.stat().st_size)
