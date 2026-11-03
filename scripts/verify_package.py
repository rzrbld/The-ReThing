#!/usr/bin/env python3
"""Verify every file and checksum in a built local candidate archive."""
import argparse,hashlib,json,tarfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('archive',type=Path);a=p.parse_args()
with tarfile.open(a.archive,'r:gz') as t:
 members=t.getmembers();names=[m.name for m in members]
 if len(names)!=len(set(names)) or any(not m.isfile() for m in members):raise SystemExit('Archive has duplicate names or non-file members')
 name='the-thing/manifest.json'
 if names.count(name)!=1:raise SystemExit('Archive has no unique manifest')
 manifest=json.load(t.extractfile(name));files=manifest['files'];expected={e['path']:e for e in files}
 if len(expected)!=len(files) or set(names)!=(set(expected)|{name}):raise SystemExit('Archive/manifest member mismatch')
 for path,entry in expected.items():
  f=t.extractfile(path);digest=hashlib.sha256();size=0
  while True:
   part=f.read(1024*1024)
   if not part:break
   size+=len(part);digest.update(part)
  if size!=entry['size'] or digest.hexdigest()!=entry['sha256']:raise SystemExit(f'Bad size/hash: {path}')
 if expected['the-thing/thething.aarch64']['sha256']!=manifest['binary_sha256']:raise SystemExit('Binary manifest mismatch')
 print(json.dumps({'archive':str(a.archive),'verified_files':len(files),'binary_sha256':manifest['binary_sha256'],'result':'PASS'},indent=2))
