#!/usr/bin/env python3
"""Download pinned official codec archives, verify, then extract into build/."""
import hashlib,tarfile,urllib.request
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'build/vorbis-arm-sources';p.mkdir(parents=True,exist_ok=True)
for component,name,digest in [('ogg','libogg-1.3.6','5c8253428e181840cd20d41f3ca16557a9cc04bad4a3d04cce84808677fa1061'),('vorbis','libvorbis-1.3.7','b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b')]:
 archive=p/(name+'.tar.xz')
 if not archive.exists():
  with urllib.request.urlopen('https://downloads.xiph.org/releases/'+component+'/'+archive.name) as response:archive.write_bytes(response.read())
 assert hashlib.sha256(archive.read_bytes()).hexdigest()==digest,archive
 if not (p/name).exists():
  with tarfile.open(archive) as t:t.extractall(p,filter='data')
 print('verified',archive.name,digest)
