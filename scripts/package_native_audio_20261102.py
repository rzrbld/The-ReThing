#!/usr/bin/env python3
"""Additive own-copy ARM audio candidate. Never replace user state/baseline."""
import hashlib,json,zipfile,subprocess,tempfile,os
from pathlib import Path
root=Path(__file__).resolve().parents[1];sha=lambda d:hashlib.sha256(d).hexdigest()
exe=(root/'build/native-audio-arm-20261102/thething-native-audio.aarch64').read_bytes();codec=(root/'build/native-vorbis-arm/probe.aarch64.so').read_bytes()
assert sha(exe)=='1863fc9f82343ed8fe6f994038d6660ee3d2fe41cada8c6092bb953585b4bf94'
assert sha(codec)=='dc4bee95be2d5b7e858e430d1dcce17f39e0f9ef3e7b5113ddb872ee082bc6d2'
settings='''export THING_NATIVE_STREAM_PROBE="$gamedir/libthing-audio.aarch64.so"
export THING_NATIVE_VORBIS_PROBE="$THING_NATIVE_STREAM_PROBE"
export THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0
export DS_RENDER_THREAD=0 THING_GPU_BUFFER_CACHE=0 THING_DRAW_BATCH=0 DS_DRAW_BATCH=0
export THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 DS_RENDER_WIDTH=640 DS_RENDER_HEIGHT=480
'''
run=(root/'packaging/The Thing.sh').read_text().replace('thething.aarch64','thething-native-audio.aarch64').replace('export GAMEDIR=',settings+'export GAMEDIR=')
measure=(root/'packaging/The Thing Measure.sh').read_text().replace('mode=${THING_MEASURE_CACHE:-0}','mode=0').replace('export THING_INDEX_RANGE=${THING_INDEX_RANGE:-0}','export THING_INDEX_RANGE=0').replace('-cache${mode}-','-audio1-cache${mode}-').replace('thething.aarch64','thething-native-audio.aarch64').replace('run-game.sh','run-game-native-audio.sh')
measure=measure.replace('    sha256sum "$gamedir/thething-native-audio.aarch64"','    sha256sum "$gamedir/thething-native-audio.aarch64" "$gamedir/libthing-audio.aarch64.so"')
readme='''The Thing: native audio test candidate, 2026-11-02

The prebuilt ARM executable is provided for the review.
Requires your existing working installation and your own original game files.
Extract at the SD-card root, merging ports and Roms folders. This adds separate
shortcuts/executable/library; your normal binary, settings and saves stay intact.

The Thing Audio: native whole-sound and streaming Vorbis decoding enabled.
The Thing Measure Audio: 240-second device capture, saved under
ports/the-thing/measurements/*-audio1-*.tar.gz after automatic exit.
Both force640x480, SINGLE, cacheOFF, skinSIMD/index/batching experimentsOFF.
Compare the same NewGame route and cutscene skips against The Thing Measure.
Include the slow hangar wall and movement/collision stutters. Check music,
effects, pause/resume and level transitions. Send the finalized measurements
archive and note any audio or stability problems. A saved game may not restart
the same soundtrack, so use NewGame when testing streaming.
Return to The ReThing or your normal The Thing shortcut for the old executable.

Host evidence: whole-sound CPU reduced73% (four runs); streaming CPU per decoded
MiB reduced76% (one OFF/ON pair). These are decoderCPU results, not gameFPS gains.
ARM corpus tests preserve282 sound lengths/formats; PCM maxdifference one signed
16-bit unit. Partial reads,1410 seeks and callback/cleanup ABI tests pass inQEMU.
The native codec is bundled; no system Vorbis package installation is required.
ARM QEMU reached the level1 engine cutscene and exited0; it did not qualify
full controllable gameplay. Host gameplay and transitions were tested separately.
H700 FPS, audible synchronization and sustained hardware behavior are unverified.
The sustained low-FPS issue is not claimed fixed. This remains a test candidate.
'''
files={'ports/the-thing/thething-native-audio.aarch64':exe,'ports/the-thing/libthing-audio.aarch64.so':codec,'ports/the-thing/run-game-native-audio.sh':run.encode(),'Roms/PORTS/The Thing Audio.sh':run.encode(),'Roms/PORTS/The Thing Measure Audio.sh':measure.encode(),'ports/the-thing/NATIVE_AUDIO_TEST.txt':readme.encode()}
for name in ('LICENSE.ogg','LICENSE.vorbis'):files['ports/the-thing/native-audio-'+name]=(root/'build/native-vorbis-arm'/name).read_bytes()
with tempfile.TemporaryDirectory(dir=root/'build',prefix='audio-package-check-') as tmp:
 base=Path(tmp);g=base/'ports/the-thing';s=base/'Roms/PORTS';g.mkdir(parents=True);s.mkdir(parents=True)
 (g/'the-thing-env.sh').write_text('pm_platform_helper() { echo "helper:$1"; }; pm_finish() { echo finish; }\n')
 (g/'thething-native-audio.aarch64').write_text('#!/bin/bash\n[[ $THING_NATIVE_STREAM_PROBE = "$THING_ROOT/libthing-audio.aarch64.so" && $THING_NATIVE_VORBIS_PROBE = "$THING_NATIVE_STREAM_PROBE" && $DS_RENDER_THREAD = 0 && $THING_GPU_BUFFER_CACHE = 0 && $THING_RENDER_WIDTH = 640 && $THING_SKIN_SIMD = 0 ]] || exit 99\necho AUDIO_SETTINGS_PASS\nexit 7\n');(g/'thething-native-audio.aarch64').chmod(0o755)
 for name,data in files.items():
  if name.endswith('.sh'):
   path=base/name;path.write_bytes(data);subprocess.run(['bash','-n',str(path)],check=True)
 env={k:v for k,v in os.environ.items() if not k.startswith(('THING_','DS_'))}
 r=subprocess.run(['bash',str(s/'The Thing Audio.sh')],env=env,capture_output=True,text=True)
 assert r.returncode==7 and 'AUDIO_SETTINGS_PASS' in r.stdout and r.stdout.count('\nfinish\n')==1,(r.returncode,r.stdout,r.stderr)
 assert 'helper:'+str(g/'thething-native-audio.aarch64') in r.stdout
files['ports/the-thing/native-audio-manifest.json']=json.dumps({'debug_elf_sha256':'5455e8a2a923f254b8156a65af857c78a93efe380dacd43354fd90709c877139','hardware_performance':'unverified','files':{n:sha(d)for n,d in files.items()}},indent=2).encode()
target=root/'dist/the-thing-native-audio-arm-20261102.zip'
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for name,data in sorted(files.items()):
  info=zipfile.ZipInfo(name);info.create_system=3;info.external_attr=(0o100755 if name.endswith(('.sh','.aarch64','.so')) else 0o100644)<<16;info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,data)
with zipfile.ZipFile(target) as z:assert z.testzip() is None
print('PASS wrapper, paths/settings, exit status, lifecycle, ZIP CRC')
target.with_suffix('.zip.sha256').write_text(sha(target.read_bytes())+'  '+target.name+'\n');print(target,target.stat().st_size)
