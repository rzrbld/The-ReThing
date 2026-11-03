#!/usr/bin/env python3
"""Summarize Present CSV; this alone cannot establish gameplay or device FPS."""
import argparse
import csv
import json
import math
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('csv', type=Path)
p.add_argument('--skip-seconds', type=float, default=0)
p.add_argument('--scene', default='unqualified capture')
p.add_argument('--output', type=Path)
p.add_argument('--allow-incomplete-final-row', action='store_true',
               help='Discard one EOF-truncated CSV row after a crash; malformed internal rows still fail')
a = p.parse_args()
with a.csv.open(newline='') as f:
    reader = csv.reader(f)
    header = next(reader, [])
    raw_rows = list(reader)
if not {'elapsed_seconds', 'frame_ms'}.issubset(header) or len(set(header)) != len(header):
    raise SystemExit('CSV needs unique columns including elapsed_seconds and frame_ms')
dropped = 0
if raw_rows and len(raw_rows[-1]) <= len(header) and (
        len(raw_rows[-1]) < len(header) or raw_rows[-1][-1] == ''):
    if not a.allow_incomplete_final_row:
        raise SystemExit('Incomplete final CSV row; use --allow-incomplete-final-row to discard it explicitly')
    raw_rows.pop()
    dropped = 1
rows = []
for line, fields in enumerate(raw_rows, 2):
    if len(fields) != len(header) or any(value == '' for value in fields):
        raise SystemExit(f'Malformed CSV row at line {line}')
    row = dict(zip(header, fields))
    try:
        elapsed, frame_ms = float(row['elapsed_seconds']), float(row['frame_ms'])
    except ValueError:
        raise SystemExit(f'Non-numeric timing at line {line}')
    if not math.isfinite(elapsed) or elapsed < 0 or not math.isfinite(frame_ms) or frame_ms <= 0:
        raise SystemExit(f'Invalid timing at line {line}')
    if elapsed >= a.skip_seconds:
        rows.append(row)
values = [float(r['frame_ms']) for r in rows]
if not values or any(not math.isfinite(v) or v <= 0 for v in values):
    raise SystemExit('Need finite positive frame times after the requested warm-up exclusion')
worst = sorted(values, reverse=True)[:max(1, math.ceil(len(values)*0.01))]
result = dict(scene=a.scene, frames=len(values), discarded_incomplete_final_rows=dropped,
              sampled_seconds=sum(values)/1000,
              average_fps=len(values)*1000/sum(values), low_1pct_fps=len(worst)*1000/sum(worst),
              low_1pct_definition='reciprocal of mean frame time among the slowest ceil(1% of frames)',
              p99_frame_ms=sorted(values)[min(len(values)-1, math.ceil(len(values)*0.99)-1)],
              worst_frame_ms=max(values), spikes_gt_33_333ms=sum(v>1000/30 for v in values),
              spikes_gt_50ms=sum(v>50 for v in values), spikes_gt_100ms=sum(v>100 for v in values),
              qualification='Frame timing only; scene correctness and actual hardware require separate evidence')
text=json.dumps(result, indent=2)+'\n'
if a.output:
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(text)
print(text, end='')
