#!/usr/bin/env python3
"""Regression: select gameplay frames without counting loading/menu samples."""
import json,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='thing-cpu-timeline-') as tmp:
 p=Path(tmp)
 (p/'maps').write_text('1000-2000 r-xp 00000000 00:00 0 /game\n')
 (p/'frames').write_text('frame,elapsed_seconds\n1,10\n2,60\n3,90\n4,120\n')
 (p/'samples').write_text('frame,host_pc,cpu_samples\n1,0x1100,5\n2,0x1200,7\n3,0x1300,11\n4,0x1400,13\n')
 command=['python3',str(root/'scripts/analyze_native_profile.py'),str(p/'samples'),str(p/'maps'),'--frame-profile',str(p/'frames'),'--from-seconds','60','--to-seconds','120']
 result=json.loads(subprocess.check_output(command,text=True));assert result['total_interval_samples']==18
 (p/'samples').write_text('host_pc,cpu_samples\n0x1100,5\n')
 failure=subprocess.run(command,text=True,capture_output=True);assert failure.returncode and 'requires host-cpu-timeline.csv' in failure.stderr
print('PASS frame-window selection includes start, excludes end, excludes loading/menu and rejects untagged samples')
