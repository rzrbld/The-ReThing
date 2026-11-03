#!/usr/bin/env python3
"""Extract ISO9660 primary-volume files from this single-track MODE2/2352 CD.

No original files are modified. Refuses unsafe names, Form 2 data and overwrite.
Does not claim to verify CD EDC/ECC or interpret the SBI sidecar.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def sha256(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def extract(image, destination):
    size = image.stat().st_size
    if size % 2352:
        raise ValueError('Image is not a whole number of raw sectors')
    with image.open('rb') as f:
        def read(lba, count):
            out = bytearray()
            while len(out) < count:
                f.seek(lba * 2352)
                sector = f.read(2352)
                if (len(sector) != 2352 or sector[:12] != b'\x00' + b'\xff' * 10 + b'\x00'
                        or sector[15] != 2 or sector[18] & 0x20
                        or sector[16:20] != sector[20:24]):
                    raise ValueError(f'Invalid/unsupported Mode 2 Form 1 sector {lba}')
                out.extend(sector[24:2072])
                lba += 1
            return bytes(out[:count])

        pvd = read(16, 2048)
        if pvd[:7] != b'\x01CD001\x01':
            raise ValueError('Missing ISO9660 primary volume descriptor')
        records = []
        visited = set()

        def walk(rec, parent=Path()):
            lba, count = struct.unpack_from('<I', rec, 2)[0], struct.unpack_from('<I', rec, 10)[0]
            if lba in visited:
                raise ValueError('Repeated directory extent')
            visited.add(lba)
            data = read(lba, count)
            pos = 0
            while pos < len(data):
                length = data[pos]
                if length == 0:
                    pos = (pos // 2048 + 1) * 2048
                    continue
                if length < 34 or pos + length > len(data):
                    raise ValueError('Invalid directory record')
                r = data[pos:pos + length]
                pos += length
                raw_name = r[33:33 + r[32]]
                if raw_name in (b'\x00', b'\x01'):
                    continue
                name = raw_name.decode('ascii').split(';')[0]
                if not name or name in ('.', '..') or any(c in name for c in '/\\:'):
                    raise ValueError('Unsafe ISO filename')
                if r[25] & 0x80 or r[26] or r[27]:
                    raise ValueError('Multi-extent/interleaved files unsupported')
                rel = parent / name
                if r[25] & 2:
                    walk(r, rel)
                    continue
                file_lba = struct.unpack_from('<I', r, 2)[0]
                file_size = struct.unpack_from('<I', r, 10)[0]
                payload = read(file_lba, file_size)
                target = destination / rel
                target.parent.mkdir(parents=True, exist_ok=True)
                with target.open('xb') as output:
                    output.write(payload)
                records.append(dict(path=rel.as_posix(), lba=file_lba, size=file_size,
                                    sha256=hashlib.sha256(payload).hexdigest()))

        walk(pvd[156:190])
    return dict(image=str(image), size=size, sha256=sha256(image),
                format='MODE2/2352, ISO9660 primary volume',
                volume=pvd[40:72].decode('ascii').rstrip(), files=records,
                limitations=['CD EDC/ECC not verified', 'SBI not interpreted',
                             'Only primary ISO9660 tree extracted'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--manifest', type=Path, required=True)
    args = parser.parse_args()
    result = extract(args.image, args.destination)
    args.manifest.parent.mkdir(parents=True, exist_ok=True)
    args.manifest.write_text(json.dumps(result, indent=2) + '\n')
    print(f"Extracted {len(result['files'])} files; image SHA-256 {result['sha256']}")
