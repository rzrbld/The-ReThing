#!/usr/bin/env python3
"""Build an additive, own-copy ARM fp-local-toping experiment package."""
import hashlib,json,zipfile,subprocess,tempfile,os
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sha=lambda data:hashlib.sha256(data).hexdigest()
game=(root/'build/fp-local-top-arm-20261102/thething-fp-local-top.aarch64').read_bytes()
expected=sha(game)
debug=root/'build/fp-local-top-arm-20261102/thething-fp-local-top.debug.aarch64'
assert sha(debug.read_bytes())=='bb995b0428567595769ad3b969b0e96ccbfc4a6365de35379204aa911e3842e1'
evidence=root/'docs/evidence/fp-local-top-arm-game-20261102'
assert (evidence/'exit.txt').read_text().strip()=='0'
assert json.loads((evidence/'metadata.json').read_text())['binary_sha256']==sha(debug.read_bytes())
assert 'GAME FAULT' not in (evidence/'run.log').read_text()
settings='''export THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0
export DS_RENDER_THREAD=0 THING_GPU_BUFFER_CACHE=0 THING_DRAW_BATCH=0 DS_DRAW_BATCH=0
export THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 DS_RENDER_WIDTH=640 DS_RENDER_HEIGHT=480
'''
run=(root/'packaging/The Thing.sh').read_text().replace('thething.aarch64','thething-fp-local-top.aarch64').replace('export GAMEDIR=',settings+'export GAMEDIR=')
measure=(root/'packaging/The Thing Measure.sh').read_text().replace('mode=${THING_MEASURE_CACHE:-0}','mode=0').replace('export THING_INDEX_RANGE=${THING_INDEX_RANGE:-0}','export THING_INDEX_RANGE=0').replace('-cache${mode}-','-cpufp1-cache${mode}-').replace('THING_DRAW_BATCH=0 DS_DRAW_BATCH=0','THING_DRAW_BATCH=0 DS_DRAW_BATCH=0').replace('thething.aarch64','thething-fp-local-top.aarch64').replace('run-game.sh','run-game-cpu-fp.sh')
readme="""The Thing: experimental ARM CPU translation, 2026-11-02

The prebuilt ARM executable is provided for the review.
Requires your existing working port installation and your own game files.
Extract at the SD root, merging ports and Roms directories. Adds an alternate
executable and shortcuts; preserves the normal executable, settings and saves.

Run The Thing CPU FP for normal play. Forces 640x480, SINGLE, cache OFF,
batching OFF, SIMD OFF, index experiment OFF. No graphics features removed.
Run The Thing Measure CPU FP for a 240-second profiling session. Reproduce the
same slow gameplay route with normal The Thing Measure as the baseline. Skip
cutscenes alike. Wait for automatic exit; send the new *-cpufp1-*.tar.gz from
ports/the-thing/measurements together with the baseline archive.

This caches the emulated floating-point stack position in 83 hot translated
routines, synchronizing it across calls. It disables arithmetic contraction in
those functions. Exact host/ARM boundary tests pass with separate operations;
this is NOT a claim of bit identity to the previous production-fast arithmetic
for arbitrary inputs (a matrix/NaN mismatch was observed during testing).

Matched local software-GLES ABBA: total main CPU time -2.3%, translated-game
self CPU -17.6%, average FPS +2.6%; 1% lows worsened overall. These are local
results, NOT projected H700 gains. ARM saved-game/controller validation passed
under QEMU/Mesa. Full gameplay, audio synchronization and device performance
remain unverified. Use the normal shortcut if any behavior or visuals regress.
This is an experiment; stable 30 FPS is not established.
"""

files={'ports/the-thing/thething-fp-local-top.aarch64':game,'ports/the-thing/run-game-cpu-fp.sh':run.encode(),'Roms/PORTS/The Thing CPU FP.sh':run.encode(),'Roms/PORTS/The Thing Measure CPU FP.sh':measure.encode(),'ports/the-thing/CPU_FP_TEST.txt':readme.encode()}
# Validate the alternate launch path and settings without running the game.
with tempfile.TemporaryDirectory(dir=root/'build',prefix='cpu-fp-package-check-') as tmp:
 base=Path(tmp);g=base/'ports/the-thing';s=base/'Roms/PORTS';g.mkdir(parents=True);s.mkdir(parents=True)
 (g/'the-thing-env.sh').write_text('pm_platform_helper() { echo "helper:$1"; }; pm_finish() { echo finish; }\n')
 (g/'thething-fp-local-top.aarch64').write_text('#!/bin/bash\n[[ $THING_SKIN_SIMD = 0 && $THING_DRAW_BATCH = 0 && $DS_DRAW_BATCH = 0 && $THING_GPU_VERTEX = 1 && $DS_GPU_VERTEX = 1 && $DS_RENDER_THREAD = 0 && $THING_GPU_BUFFER_CACHE = 0 && $THING_RENDER_WIDTH = 640 && $THING_RENDER_HEIGHT = 480 ]] || exit 99\necho CPU_FP_SETTINGS_PASS\nexit 7\n')
 (g/'thething-fp-local-top.aarch64').chmod(0o755)
 for name,data in files.items():
  if not name.endswith('.sh'):continue
  path=base/name;path.write_bytes(data);subprocess.run(['bash','-n',str(path)],check=True)
 env={k:v for k,v in os.environ.items() if not k.startswith(('THING_','DS_'))}
 result=subprocess.run(['bash',str(s/'The Thing CPU FP.sh')],env=env,capture_output=True,text=True)
 assert result.returncode==7,(result.returncode,result.stdout,result.stderr)
 assert 'CPU_FP_SETTINGS_PASS' in result.stdout and result.stdout.count('\nfinish\n')==1,result.stdout
 assert 'helper:'+str(g/'thething-fp-local-top.aarch64') in result.stdout
files['ports/the-thing/fp-local-top-manifest.json']=json.dumps({'game_sha256':expected,'hardware_performance':'unverified','files':{n:sha(d) for n,d in files.items()}},indent=2).encode()
target=root/'dist/the-thing-fp-local-top-arm-20261102.zip'
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for name,data in sorted(files.items()):
  info=zipfile.ZipInfo(name);info.create_system=3;info.external_attr=(0o100755 if name.endswith(('.sh','.aarch64')) else 0o100644)<<16;info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,data)
with zipfile.ZipFile(target) as z:
 assert z.testzip() is None
 assert sha(z.read('ports/the-thing/thething-fp-local-top.aarch64'))==expected
target.with_suffix('.zip.sha256').write_text(sha(target.read_bytes())+'  '+target.name+'\n')
print('PASS wrapper settings, alternate executable, lifecycle, shell syntax, ZIP CRC and SHA256')
print(target,target.stat().st_size)
