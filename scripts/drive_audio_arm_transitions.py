#!/usr/bin/env python3
"""Drive only observed level transitions in the bounded ARM functional run."""
import time,json
from pathlib import Path
root=Path(__file__).resolve().parents[1];name='native-audio-arm-transition-20261102';out=root/'docs/evidence'/name;game=root/'build'/name
start=time.monotonic();pending={};done=set();events=[]
while time.monotonic()-start<180:
 if (out/'exit.txt').exists():break
 log=(out/'run.log').read_text(errors='replace') if (out/'run.log').exists() else ''
 for level in ('intro','level1'):
  marker='Levels\\'+level+'.dll") ->'
  if marker in log and level not in done and level not in pending:pending[level]=time.monotonic()+5
 for level,due in list(pending.items()):
  if time.monotonic()>=due:
   with (game/'input.commands').open('a')as f:f.write('tap start\n')
   events.append(dict(level=level,action='tap start',reason='module observed loaded five seconds earlier',driver_elapsed=time.monotonic()-start));done.add(level);del pending[level]
   (out/'transition-driver.json').write_text(json.dumps(events,indent=2)+'\n')
 if len(done)==2:break
 time.sleep(.25)
print('Observed transitions driven:',sorted(done))
