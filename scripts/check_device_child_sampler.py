#!/usr/bin/env python3
import csv,subprocess,tempfile,time,os
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(dir=root/'build',prefix='child-sampler-check-')as tmp:
 p=Path(tmp);unrelated=subprocess.Popen(['sleep','12']);launcher=subprocess.Popen(['bash','-c','sleep 5 & echo $! > "$1"; wait','test',str(p/'child.pid')])
 try:
  with (p/'sampler.log').open('w')as log:
   r=subprocess.run(['bash',str(root/'scripts/sample_device_child.sh'),str(launcher.pid),'/usr/bin/sleep',str(p/'telemetry.csv')],stderr=log,timeout=10)
  assert r.returncode==0,(r.returncode,(p/'sampler.log').read_text())
  child=int((p/'child.pid').read_text());log=(p/'sampler.log').read_text();assert f'pid={child} ' in log and f'pid={unrelated.pid} ' not in log
  rows=list(csv.DictReader((p/'telemetry.csv').open()));keys={r['metric']for r in rows}
  assert {'VmRSS','VmHWM','MemAvailable','Threads'}<=keys,keys
  before=(p/'telemetry.csv').read_bytes()
  r=subprocess.run(['bash',str(root/'scripts/sample_device.sh'),str(unrelated.pid),str(p/'telemetry.csv')],capture_output=True,timeout=3)
  assert r.returncode!=0 and (p/'telemetry.csv').read_bytes()==before
  e=root/'docs/evidence/device-child-sampler-20261102';e.mkdir(exist_ok=True)
  (e/'results.txt').write_text('PASS exact executable among launcher descendants; unrelated process excluded; PID identity guard; terminal exit; RAM/system metrics present; existing output preserved. Local Linux test, not H700 validation.\n'+log)
  print((e/'results.txt').read_text())
 finally:
  for proc in (launcher,unrelated):
   if proc.poll() is None:proc.terminate()
   proc.wait()
