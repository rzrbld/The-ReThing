#!/usr/bin/env python3
"""Drive a bounded QEMU campaign by its actual movie/level events."""
import os,pathlib,time,json,shutil
p=pathlib.Path(__file__).resolve().parents[1];root=p/'build/qemu-gameplay';out=p/'docs/evidence/qemu-gameplay';log=out/'run.log'
start=time.monotonic(); phase='movies';stamp=0;events=[];saved=set();last_pulse=0
while time.monotonic()-start<500:
 text=log.read_text(errors='replace') if log.exists() else ''
 now=time.monotonic();elapsed=now-start
 if 'GAME FAULT' in text or 'guest exceptions are not handled' in text:break
 if phase=='movies' and text.count('EC_COMPLETE')>=3:
  phase='menu-wait';stamp=now;events.append([round(elapsed,1),'three movies complete'])
 if phase=='menu-wait' and now-stamp>=6:
  with (root/'input.commands').open('a') as f:f.write('tap a\n')
  phase='difficulty';stamp=now;events.append([round(elapsed,1),'new game'])
 if phase=='difficulty' and now-stamp>=5:
  with (root/'input.commands').open('a') as f:f.write('tap a\n')
  phase='intro';stamp=now;events.append([round(elapsed,1),'normal aiming'])
 if phase in ('intro','level1') and 'LoadLibraryA("C:\\data\\Levels\\level1.dll")' in text and phase!='level1':
  phase='level1';stamp=now;events.append([round(elapsed,1),'level1 loaded'])
 if phase!='movies' and now-last_pulse>=1:
  (root/'capture.request').touch();last_pulse=now
 bmp=root/'capture.bmp'
 if bmp.exists():
  m=bmp.stat().st_mtime_ns
  if m not in saved:
   saved.add(m)
   try:shutil.copyfile(bmp,out/f'{phase}-{elapsed:06.1f}.bmp')
   except FileNotFoundError:pass
 time.sleep(.15)
(out/'control.json').write_text(json.dumps({'phase':phase,'events':events,'elapsed':round(time.monotonic()-start,1)},indent=2)+'\n')
