#!/usr/bin/env python3
"""Additive own-copy ARM audio candidate. Never replace user state/baseline."""
import hashlib,json,zipfile,subprocess,tempfile,os
from pathlib import Path
root=Path(__file__).resolve().parents[1];sha=lambda d:hashlib.sha256(d).hexdigest()
exe=(root/'build/cpu-audio-combined-aarch64-20261102/thething-cpu-combined.aarch64').read_bytes();codec=(root/'build/native-vorbis-arm/probe.aarch64.so').read_bytes()
debug=root/'build/cpu-audio-combined-aarch64-20261102/diagnostic.aarch64'
assert sha(debug.read_bytes())=='2202857a3e61e18dacad57ddf3e97288b79f934ac2be411fea7f131d88fb97da'
for name in ('cpu-audio-combined-arm-play-20261102','cpu-audio-combined-arm-transition-20261102'):
 evidence=root/'docs/evidence'/name
 assert (evidence/'exit.txt').read_text().strip()=='0'
 assert json.loads((evidence/'metadata.json').read_text())['binary_sha256']==sha(debug.read_bytes())
 log=(evidence/'run.log').read_text()
 assert 'GAME FAULT' not in log and '[native-vorbis-probe] decoded' in log
 if 'transition' in name:
  assert 'intro.dll' in log and 'level1.dll' in log and log.count('[native-stream-probe] opened')>=2
assert 'AArch64' in subprocess.check_output(['readelf','-h',str(debug)],text=True)
assert sha(codec)=='dc4bee95be2d5b7e858e430d1dcce17f39e0f9ef3e7b5113ddb872ee082bc6d2'
settings='''export THING_GPU_VERTEX=1 DS_GPU_VERTEX=1
export THING_NATIVE_STREAM_PROBE="$gamedir/libthing-audio.aarch64.so"
export THING_NATIVE_VORBIS_PROBE="$THING_NATIVE_STREAM_PROBE"
export THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0
export DS_RENDER_THREAD=0 THING_GPU_BUFFER_CACHE=0 THING_DRAW_BATCH=1 DS_DRAW_BATCH=1
export THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 DS_RENDER_WIDTH=640 DS_RENDER_HEIGHT=480
'''
run=(root/'packaging/The Thing.sh').read_text().replace('thething.aarch64','thething-cpu-combined.aarch64').replace('export GAMEDIR=',settings+'export GAMEDIR=')
measure=(root/'packaging/The Thing Measure.sh').read_text().replace('mode=${THING_MEASURE_CACHE:-0}','mode=0').replace('export THING_INDEX_RANGE=${THING_INDEX_RANGE:-0}','export THING_INDEX_RANGE=0').replace('-cache${mode}-','-cpucombo1-cache${mode}-').replace('thething.aarch64','thething-cpu-combined.aarch64').replace('run-game.sh','run-game-cpu-combined.sh')
measure=measure.replace('    echo "index_range=', '    echo "combined_cpu=83_routines native_audio=on opaque_batch=on"\n    echo "index_range=')
measure=measure.replace('export THING_DRAW_BATCH=0 DS_DRAW_BATCH=0','export THING_DRAW_BATCH=1 DS_DRAW_BATCH=1')
measure=measure.replace('    sha256sum "$gamedir/thething-cpu-combined.aarch64"','    sha256sum "$gamedir/thething-cpu-combined.aarch64" "$gamedir/libthing-audio.aarch64.so"')
readme='The Thing: combined CPU/audio test build, 2026-11-02\n\nThe prebuilt ARM executable is provided for the review.\nRequires your existing working installation and your own original game files.\nNo original game assets or ISO are included.\n\nExtract at the SD-card root, merging ports and Roms folders. This adds separate\nexecutables and shortcuts. The normal executable, configs and saves stay intact.\nRun The Thing CPU Combined for play. Run The Thing Measure CPU Combined for a\n240-second diagnostic capture. Both force 640x480, SINGLE, buffer cache OFF,\nopaque batching ON, SIMD/index experiments OFF and native Vorbis decoding.\n\nFor comparison, run the same route using The Thing Measure with SINGLE,\ncache OFF, 640x480. Use New Game and skip the same cutscenes in both runs so\nstreaming music is exercised. Include the slow hangar wall, movement against\nwalls and the first room. Wait for automatic exit; collect the new archive\nunder ports/the-thing/measurements/*-cpucombo1-*.tar.gz and the baseline archive.\nReport music/effects, loading screens, save/load and any stability differences.\nUse the normal shortcut to return to the baseline build.\n\nChanges: local emulated floating-point stack position in 83 hot routines;\nnative whole-sound and streaming Vorbis; existing adjacent opaque-draw batching.\nNo graphics settings or effects removed. The 83 routines use separate floating\npoint operations; this is not a claim of bit identity for all unusual inputs.\n\nFour local 30-FPS-capped runs: main-thread CPU 6.394 to 5.917 ms/frame (-7.45%).\nRun-to-run variation was material. These software-GLES host measurements are\nNOT projected H700 gains. Diagnostic screenshots affect local low-frame results.\nNative sound decoding and batching were active; streaming did not open in that\nsaved-game route. Separate ARM QEMU checks covered saved gameplay and New Game\nstream opens across intro/level1. QEMU uses dummy audio: audible synchronization\nand sustained device performance, including stable 30 FPS, remain unverified.\nThe native codec is included; no system Vorbis installation is required.\n'
files={'ports/the-thing/thething-cpu-combined.aarch64':exe,'ports/the-thing/libthing-audio.aarch64.so':codec,'ports/the-thing/run-game-cpu-combined.sh':run.encode(),'Roms/PORTS/The Thing CPU Combined.sh':run.encode(),'Roms/PORTS/The Thing Measure CPU Combined.sh':measure.encode(),'ports/the-thing/CPU_COMBINED_TEST.txt':readme.encode()}
for name in ('LICENSE.ogg','LICENSE.vorbis'):files['ports/the-thing/native-audio-'+name]=(root/'build/native-vorbis-arm'/name).read_bytes()
with tempfile.TemporaryDirectory(dir=root/'build',prefix='audio-package-check-') as tmp:
 base=Path(tmp);g=base/'ports/the-thing';s=base/'Roms/PORTS';g.mkdir(parents=True);s.mkdir(parents=True)
 (g/'the-thing-env.sh').write_text('pm_platform_helper() { echo "helper:$1"; }; pm_finish() { echo finish; }\n')
 (g/'thething-cpu-combined.aarch64').write_text('#!/bin/bash\n[[ $THING_NATIVE_STREAM_PROBE = "$THING_ROOT/libthing-audio.aarch64.so" && $THING_NATIVE_VORBIS_PROBE = "$THING_NATIVE_STREAM_PROBE" && $DS_RENDER_THREAD = 0 && $THING_GPU_BUFFER_CACHE = 0 && $THING_RENDER_WIDTH = 640 && $THING_SKIN_SIMD = 0 && $THING_DRAW_BATCH = 1 && $DS_DRAW_BATCH = 1 && $THING_GPU_VERTEX = 1 ]] || exit 99\necho COMBINED_SETTINGS_PASS\nexit 7\n');(g/'thething-cpu-combined.aarch64').chmod(0o755)
 for name,data in files.items():
  if name.endswith('.sh'):
   path=base/name;path.write_bytes(data);subprocess.run(['bash','-n',str(path)],check=True)
 env={k:v for k,v in os.environ.items() if not k.startswith(('THING_','DS_'))}
 r=subprocess.run(['bash',str(s/'The Thing CPU Combined.sh')],env=env,capture_output=True,text=True)
 assert r.returncode==7 and 'COMBINED_SETTINGS_PASS' in r.stdout and r.stdout.count('\nfinish\n')==1,(r.returncode,r.stdout,r.stderr)
 assert 'helper:'+str(g/'thething-cpu-combined.aarch64') in r.stdout
files['ports/the-thing/cpu-combined-manifest.json']=json.dumps({'debug_elf_sha256':'2202857a3e61e18dacad57ddf3e97288b79f934ac2be411fea7f131d88fb97da','hardware_performance':'unverified','files':{n:sha(d)for n,d in files.items()}},indent=2).encode()
target=root/'dist/the-thing-cpu-combined-arm-20261102.zip'
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for name,data in sorted(files.items()):
  info=zipfile.ZipInfo(name);info.create_system=3;info.external_attr=(0o100755 if name.endswith(('.sh','.aarch64','.so')) else 0o100644)<<16;info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,data)
with zipfile.ZipFile(target) as z:assert z.testzip() is None
print('PASS wrapper, paths/settings, exit status, lifecycle, ZIP CRC')
target.with_suffix('.zip.sha256').write_text(sha(target.read_bytes())+'  '+target.name+'\n');print(target,target.stat().st_size)
