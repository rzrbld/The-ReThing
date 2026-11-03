#!/usr/bin/env python3
"""Build an additive, own-copy ARM SIMD experiment package."""
import hashlib,json,zipfile,subprocess,tempfile,os
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sha=lambda data:hashlib.sha256(data).hexdigest()
expected='1703de3aa7806070f90f854eeaf2bbf211c724ac066054cd4209a4c8d7f8af4a'
game=(root/'build/skin-simd-arm-20261102/thething-skin-simd.aarch64').read_bytes()
assert sha(game)==expected
settings='''export THING_SKIN_SIMD=1 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0
export DS_RENDER_THREAD=0 THING_GPU_BUFFER_CACHE=0 THING_DRAW_BATCH=0 DS_DRAW_BATCH=0
export THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 DS_RENDER_WIDTH=640 DS_RENDER_HEIGHT=480
'''
run=(root/'packaging/The Thing.sh').read_text().replace('thething.aarch64','thething-skin-simd.aarch64').replace('export GAMEDIR=',settings+'export GAMEDIR=')
measure=(root/'packaging/The Thing Measure.sh').read_text().replace('mode=${THING_MEASURE_CACHE:-0}','mode=0').replace('export THING_INDEX_RANGE=${THING_INDEX_RANGE:-0}','export THING_INDEX_RANGE=0').replace('-cache${mode}-','-skin1-cache${mode}-').replace('THING_SKIN_SIMD=0','THING_SKIN_SIMD=1').replace('thething.aarch64','thething-skin-simd.aarch64').replace('run-game.sh','run-game-simd.sh')
readme='''The Thing: experimental ARM skinning SIMD build, 2026-11-02

The prebuilt ARM executable is provided for the review.
Requires your existing working installation and your own game files.
Extract this ZIP at the SD card root, merging ports and Roms directories.
It adds an alternate executable and shortcuts; it does not replace your
normal executable, launcher, saves or configuration.

Run The Thing SIMD for normal play. It forces skinning SIMD ON, 640x480,
SINGLE renderer, buffer cache OFF, index experiment OFF and batching OFF.
Run The Thing Measure SIMD for a 240-second profiling session. Reproduce
the same slow gameplay route as The Thing Measure; skip cutscenes alike.
Wait for automatic exit, then send the new *-skin1-*.tar.gz from
ports/the-thing/measurements. Normal The Thing Measure is the OFF baseline.
The SIMD shortcut shares your existing saves. Return to The ReThing or
The Thing to run your original build again.

This enables paired-double AArch64 NEON skinning arithmetic, with guarded
fallback to original code. It does not include the rejected call-flags
experiment. Host comparison showed no reliable benefit (-0.38% mean FPS);
ARM hardware may differ. No H700 performance improvement is claimed.

Validation: 96,000 arithmetic equivalence cases passed on ARM under QEMU;
133 imports resolved; a 130-second QEMU/software-GLES saved-game run reached
level 1 with SIMD active and exited 0. This is not a hardware benchmark or
full gameplay qualification. The known low-FPS issue remains unresolved.
'''
files={'ports/the-thing/thething-skin-simd.aarch64':game,'ports/the-thing/run-game-simd.sh':run.encode(),'Roms/PORTS/The Thing SIMD.sh':run.encode(),'Roms/PORTS/The Thing Measure SIMD.sh':measure.encode(),'ports/the-thing/SKIN_SIMD_TEST.txt':readme.encode()}
# Validate the alternate launch path and settings without running the game.
with tempfile.TemporaryDirectory(dir=root/'build',prefix='skin-package-check-') as tmp:
 base=Path(tmp);g=base/'ports/the-thing';s=base/'Roms/PORTS';g.mkdir(parents=True);s.mkdir(parents=True)
 (g/'the-thing-env.sh').write_text('pm_platform_helper() { echo "helper:$1"; }; pm_finish() { echo finish; }\n')
 (g/'thething-skin-simd.aarch64').write_text('#!/bin/bash\n[[ $THING_SKIN_SIMD = 1 && $DS_RENDER_THREAD = 0 && $THING_GPU_BUFFER_CACHE = 0 && $THING_RENDER_WIDTH = 640 && $THING_RENDER_HEIGHT = 480 ]] || exit 99\necho SIMD_SETTINGS_PASS\nexit 7\n')
 (g/'thething-skin-simd.aarch64').chmod(0o755)
 for name,data in files.items():
  if not name.endswith('.sh'):continue
  path=base/name;path.write_bytes(data);subprocess.run(['bash','-n',str(path)],check=True)
 env={k:v for k,v in os.environ.items() if not k.startswith(('THING_','DS_'))}
 result=subprocess.run(['bash',str(s/'The Thing SIMD.sh')],env=env,capture_output=True,text=True)
 assert result.returncode==7,(result.returncode,result.stdout,result.stderr)
 assert 'SIMD_SETTINGS_PASS' in result.stdout and result.stdout.count('\nfinish\n')==1,result.stdout
 assert 'helper:'+str(g/'thething-skin-simd.aarch64') in result.stdout
files['ports/the-thing/skin-simd-manifest.json']=json.dumps({'game_sha256':expected,'hardware_performance':'unverified','files':{n:sha(d) for n,d in files.items()}},indent=2).encode()
target=root/'dist/the-thing-skin-simd-arm-20261102.zip'
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for name,data in sorted(files.items()):
  info=zipfile.ZipInfo(name);info.create_system=3;info.external_attr=(0o100755 if name.endswith(('.sh','.aarch64')) else 0o100644)<<16;info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,data)
with zipfile.ZipFile(target) as z:
 assert z.testzip() is None
 assert sha(z.read('ports/the-thing/thething-skin-simd.aarch64'))==expected
target.with_suffix('.zip.sha256').write_text(sha(target.read_bytes())+'  '+target.name+'\n')
print('PASS wrapper settings, alternate executable, lifecycle, shell syntax, ZIP CRC and SHA256')
print(target,target.stat().st_size)
