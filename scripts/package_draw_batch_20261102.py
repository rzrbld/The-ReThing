#!/usr/bin/env python3
"""Build an additive, own-copy ARM draw-batching experiment package."""
import hashlib,json,zipfile,subprocess,tempfile,os
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sha=lambda data:hashlib.sha256(data).hexdigest()
expected='7c0ae7511ab36ffd17d48fd3a211e4080dbb730228c951e71a08de21ea71d683'
with zipfile.ZipFile(root/'dist/the-thing-cpu-measure-20261102.zip') as source:
 game=source.read('ports/the-thing/thething.aarch64')
# Require completed ARM gameplay evidence with actual positive batch counters.
evidence=root/'docs/evidence/batch-arm-retry-20261102'
assert (evidence/'exit.txt').read_text().strip()=='0'
import re
assert any(int(n)>0 for n in re.findall(r'\[draw-batch\].*saved=(\d+)',(evidence/'run.log').read_text()))
assert sha(game)==expected
settings='''export THING_GPU_VERTEX=1 DS_GPU_VERTEX=1 THING_SKIN_SIMD=0 THING_MATRIX_SIMD=0 THING_INDEX_RANGE=0
export DS_RENDER_THREAD=0 THING_GPU_BUFFER_CACHE=0 THING_DRAW_BATCH=1 DS_DRAW_BATCH=1
export THING_RENDER_WIDTH=640 THING_RENDER_HEIGHT=480 DS_RENDER_WIDTH=640 DS_RENDER_HEIGHT=480
'''
run=(root/'packaging/The Thing.sh').read_text().replace('thething.aarch64','thething-draw-batch.aarch64').replace('export GAMEDIR=',settings+'export GAMEDIR=')
measure=(root/'packaging/The Thing Measure.sh').read_text().replace('mode=${THING_MEASURE_CACHE:-0}','mode=0').replace('export THING_INDEX_RANGE=${THING_INDEX_RANGE:-0}','export THING_INDEX_RANGE=0').replace('-cache${mode}-','-batch1-cache${mode}-').replace('THING_DRAW_BATCH=0 DS_DRAW_BATCH=0','THING_DRAW_BATCH=1 DS_DRAW_BATCH=1').replace('thething.aarch64','thething-draw-batch.aarch64').replace('run-game.sh','run-game-batch.sh')
readme='''The Thing: experimental ARM draw batching, 2026-11-02

The prebuilt ARM executable is provided for the review.
Requires your existing working installation and your own game files.
Extract this ZIP at the SD card root, merging ports and Roms directories.
Adds an alternate executable and shortcuts; keeps the normal build and saves.

Run The Thing Batch for normal play. Settings: 640x480, SINGLE renderer,
buffer cache OFF, SIMD OFF, index experiment OFF, adjacent draw batching ON.
Run The Thing Measure Batch for a 240-second profiling session. Reproduce
one slow gameplay route; use the same route with normal The Thing Measure
for the baseline. Skip cutscenes alike. Wait for automatic exit, then send
the new *-batch1-*.tar.gz from ports/the-thing/measurements.

This uses the exact baseline ARM code with an existing experimental batching
option enabled. It combines compatible adjacent opaque triangles while
preserving draw order and resource barriers. This is separate from the buffer
cache and threaded-renderer experiments; both remain OFF.

Local host evidence: 12 exact 640x480 differential rendering comparisons;
a prior matched saved-game ABBA averaged approximately 3% faster, with mixed
1% lows. ARM saved-game validation under QEMU/software GLES is recorded in
docs/evidence/batch-arm-retry-20261102. None of this establishes H700 FPS gain
or broad gameplay correctness. Revert with the normal game shortcut if visuals
or performance regress. The known low-FPS problem remains unresolved.
'''

files={'ports/the-thing/thething-draw-batch.aarch64':game,'ports/the-thing/run-game-batch.sh':run.encode(),'Roms/PORTS/The Thing Batch.sh':run.encode(),'Roms/PORTS/The Thing Measure Batch.sh':measure.encode(),'ports/the-thing/DRAW_BATCH_TEST.txt':readme.encode()}
# Validate the alternate launch path and settings without running the game.
with tempfile.TemporaryDirectory(dir=root/'build',prefix='batch-package-check-') as tmp:
 base=Path(tmp);g=base/'ports/the-thing';s=base/'Roms/PORTS';g.mkdir(parents=True);s.mkdir(parents=True)
 (g/'the-thing-env.sh').write_text('pm_platform_helper() { echo "helper:$1"; }; pm_finish() { echo finish; }\n')
 (g/'thething-draw-batch.aarch64').write_text('#!/bin/bash\n[[ $THING_SKIN_SIMD = 0 && $THING_DRAW_BATCH = 1 && $DS_DRAW_BATCH = 1 && $THING_GPU_VERTEX = 1 && $DS_GPU_VERTEX = 1 && $DS_RENDER_THREAD = 0 && $THING_GPU_BUFFER_CACHE = 0 && $THING_RENDER_WIDTH = 640 && $THING_RENDER_HEIGHT = 480 ]] || exit 99\necho BATCH_SETTINGS_PASS\nexit 7\n')
 (g/'thething-draw-batch.aarch64').chmod(0o755)
 for name,data in files.items():
  if not name.endswith('.sh'):continue
  path=base/name;path.write_bytes(data);subprocess.run(['bash','-n',str(path)],check=True)
 env={k:v for k,v in os.environ.items() if not k.startswith(('THING_','DS_'))}
 result=subprocess.run(['bash',str(s/'The Thing Batch.sh')],env=env,capture_output=True,text=True)
 assert result.returncode==7,(result.returncode,result.stdout,result.stderr)
 assert 'BATCH_SETTINGS_PASS' in result.stdout and result.stdout.count('\nfinish\n')==1,result.stdout
 assert 'helper:'+str(g/'thething-draw-batch.aarch64') in result.stdout
files['ports/the-thing/draw-batch-manifest.json']=json.dumps({'game_sha256':expected,'hardware_performance':'unverified','files':{n:sha(d) for n,d in files.items()}},indent=2).encode()
target=root/'dist/the-thing-draw-batch-arm-20261102.zip'
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for name,data in sorted(files.items()):
  info=zipfile.ZipInfo(name);info.create_system=3;info.external_attr=(0o100755 if name.endswith(('.sh','.aarch64')) else 0o100644)<<16;info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,data)
with zipfile.ZipFile(target) as z:
 assert z.testzip() is None
 assert sha(z.read('ports/the-thing/thething-draw-batch.aarch64'))==expected
target.with_suffix('.zip.sha256').write_text(sha(target.read_bytes())+'  '+target.name+'\n')
print('PASS wrapper settings, alternate executable, lifecycle, shell syntax, ZIP CRC and SHA256')
print(target,target.stat().st_size)
