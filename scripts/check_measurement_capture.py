#!/usr/bin/env python3
"""Fresh capture provenance and failure handling without launching the game."""
import os,pathlib,subprocess,tempfile,tarfile
R=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='measure-check-',dir=R/'build') as t:
 base=pathlib.Path(t)
 for mode in ('complete','failed'):
  root=base/mode;root.mkdir();(root/'bin').mkdir()
  (root/'the-thing-env.sh').write_text('');(root/'thething.aarch64').write_text('fixture')
  for n in ('log.txt','profile.csv','render.csv','host-cpu-profile.csv','host-cpu-timeline.csv','cpu-timeline-info.txt'):(root/n).write_text('OLD STALE DATA')
  script=root/'run-game.sh'
  script.write_text('''#!/bin/bash
[[ "$THING_RENDER_WIDTH:$THING_RENDER_HEIGHT:$DS_RENDER_THREAD:$THING_GPU_BUFFER_CACHE:$THING_DRAW_BATCH" == 640:480:0:0:0 ]] || exit 99
cd "$THING_ROOT"
echo new > log.txt
'''+('echo startup-failed >&2; exit 7\n' if mode=='failed' else "for n in profile.csv render.csv host-cpu-profile.csv host-cpu-timeline.csv cpu-timeline-info.txt; do printf 'header\\nnew-sample\\n' > \"$n\"; done\n"))
  env={k:v for k,v in os.environ.items() if not k.startswith(('THING_','DS_'))};env['THING_ROOT']=str(root)
  proc=subprocess.run(['bash',str(R/'packaging/The Thing Measure.sh')],env=env,capture_output=True,text=True)
  assert proc.returncode==(7 if mode=='failed' else 0),(proc.returncode,proc.stderr)
  run=next(p for p in (root/'measurements').iterdir() if p.is_dir())
  assert not (run/'RUNNING').exists()
  assert (run/('INCOMPLETE.txt' if mode=='failed' else 'COMPLETE.txt')).exists()
  assert (run/'previous/host-cpu-profile.csv').read_text()=='OLD STALE DATA'
  assert (run/'measurement-launcher.log').exists()
  with tarfile.open(str(run)+'.tar.gz') as tar:
   assert not any('/previous/' in n for n in tar.getnames())
  if mode=='failed':assert not (run/'host-cpu-profile.csv').exists()
  else:
   assert (run/'host-cpu-profile.csv').read_text()=='header\nnew-sample\n'
   assert (run/'host-cpu-timeline.csv').read_text()=='header\nnew-sample\n'
   assert (run/'previous/host-cpu-timeline.csv').read_text()=='OLD STALE DATA'
  print('PASS',mode,'fresh-only archive, explicit status, stderr and exit captured')
