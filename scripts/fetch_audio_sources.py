#!/usr/bin/env python3
"""Fetch and verify the public Xiph source releases used by the native codec."""
import hashlib
from pathlib import Path
import shutil
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT/'build/vorbis-arm-sources'
DEST.mkdir(parents=True, exist_ok=True)
SOURCES = [
    ('ogg', 'libogg-1.3.6', '5c8253428e181840cd20d41f3ca16557a9cc04bad4a3d04cce84808677fa1061'),
    ('vorbis', 'libvorbis-1.3.7', 'b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b'),
]
for project, name, expected in SOURCES:
    archive = DEST/(name+'.tar.xz')
    if not archive.exists():
        temporary = archive.with_suffix('.download')
        with urllib.request.urlopen(f'https://downloads.xiph.org/releases/{project}/{archive.name}') as response, temporary.open('wb') as output:
            shutil.copyfileobj(response, output)
        if hashlib.sha256(temporary.read_bytes()).hexdigest() != expected:
            raise RuntimeError('Download checksum mismatch: '+archive.name)
        temporary.replace(archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
        raise RuntimeError('Cached source checksum mismatch: '+archive.name)
    if not (DEST/name).exists():
        with tarfile.open(archive) as source:
            # These pinned archives need only ordinary files/directories.
            for item in source.getmembers():
                if (not item.isfile() and not item.isdir()) or Path(item.name).is_absolute() or '..' in Path(item.name).parts or Path(item.name).parts[0] != name:
                    raise RuntimeError('Unsafe source archive entry: '+item.name)
            source.extractall(DEST)
    print('Verified '+archive.name)
