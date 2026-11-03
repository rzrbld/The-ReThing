"""Read buffered CPU CSV safely after a guest _exit bypasses stdio cleanup."""
import csv,io,math
from pathlib import Path
COLUMNS=('swap','elapsed_seconds','thread_cpu_ms','swap_thread_cpu_ms','swap_wall_ms')
def read_complete(path,through=93):
 text=Path(path).read_text();reader=csv.DictReader(io.StringIO(text));assert tuple(reader.fieldnames or ())==COLUMNS
 raw=list(reader);rows=[]
 for i,row in enumerate(raw):
  try:
   valid=set(row)==set(COLUMNS) and all(row[k] is not None and math.isfinite(float(row[k])) for k in COLUMNS)
  except (ValueError,TypeError):valid=False
  if not valid:
   assert i==len(raw)-1 and not text.endswith('\n'),f'invalid non-trailing CSV row {i}'
   continue
  rows.append(row)
 assert rows and float(rows[-1]['elapsed_seconds'])>=through,'timing CSV does not cover full requested window'
 assert all(float(a['elapsed_seconds'])<=float(b['elapsed_seconds']) for a,b in zip(rows,rows[1:])), 'nonmonotonic CPU timeline'
 return rows
