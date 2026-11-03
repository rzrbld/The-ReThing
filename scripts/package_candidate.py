#!/usr/bin/env python3
"""Package local proprietary game files and a stripped ARM diagnostic candidate.

Run only after build_game.sh aarch64. This does not deploy or qualify gameplay.
"""
import hashlib
import json
from pathlib import Path
import tarfile
from stage_mainui import stage_mainui, merge_inputs

ROOT = Path(__file__).resolve().parents[1]


def main():
    binary = ROOT/'build/package/thething.aarch64'
    if not binary.exists():
        raise SystemExit('First create build/package/thething.aarch64 with scripts/package_candidate.sh')
    b = binary.read_bytes()
    if b[:6] != b'\x7fELF\x02\x01' or int.from_bytes(b[18:20], 'little') != 183:
        raise SystemExit('Candidate must be an AArch64 little-endian ELF64')
    inputs = [(binary, 'the-thing/thething.aarch64'),
              (ROOT/'build/launcher/thething-launcher.aarch64', 'the-thing/thething-launcher'),
              (ROOT/'packaging/the-thing-env.sh', 'the-thing/the-thing-env.sh'),
              (ROOT/'packaging/The ReThing.sh', 'The ReThing.sh'),
              (ROOT/'packaging/The Thing.sh', 'The Thing.sh'),
              (ROOT/'packaging/The Thing GPU.sh', 'The Thing GPU.sh'),
              (ROOT/'packaging/The Thing Fast.sh', 'The Thing Fast.sh'),
              (ROOT/'packaging/The Thing Safe.sh', 'The Thing Safe.sh'),
              (ROOT/'packaging/The Thing Threaded.sh', 'The Thing Threaded.sh'),
              (ROOT/'packaging/The Thing Sync.sh', 'The Thing Sync.sh'),
              (ROOT/'packaging/The Thing Tuned.sh', 'The Thing Tuned.sh'),
              (ROOT/'packaging/The Thing Tuned NoFast.sh', 'The Thing Tuned NoFast.sh'),
              (ROOT/'packaging/The Thing Tuned Diagnose.sh', 'The Thing Tuned Diagnose.sh'),
              (ROOT/'packaging/The Thing Overlay.sh', 'The Thing Overlay.sh'),
              (ROOT/'packaging/The Thing 30fps.sh', 'The Thing 30fps.sh'),
              (ROOT/'packaging/The Thing 60fps.sh', 'The Thing 60fps.sh'),
              (ROOT/'packaging/The Thing Uncap.sh', 'The Thing Uncap.sh'),
              (ROOT/'packaging/The Thing Profile.sh', 'The Thing Profile.sh'),
              (ROOT/'packaging/muos-session.sh', 'the-thing/muos-session.sh'),
              (ROOT/'packaging/cpu-session.sh', 'the-thing/cpu-session.sh'),
              (ROOT/'third_party/pcrecomp/LICENSE', 'the-thing/licenses/pcrecomp-LICENSE'),
              (ROOT/'third_party/pcrecomp/NOTICE', 'the-thing/licenses/pcrecomp-NOTICE'),
              (ROOT/'third_party/pl_mpeg/pl_mpeg.h', 'the-thing/licenses/pl_mpeg.h'),
              (ROOT/'probe/LICENSE.desert-pad-source', 'the-thing/licenses/controller-source.txt'),
              (ROOT/'config/controller-buttons.cfg', 'the-thing/controller-buttons.cfg')]
    for name in ['bin', 'data']:
        for p in sorted((ROOT/'build/active'/name).rglob('*')):
            if p.is_file():
                inputs.append((p, 'the-thing/'+str(p.relative_to(ROOT/'build/active'))))
    inputs = merge_inputs(inputs, stage_mainui(ROOT/'build/active/data/mainui.pak',
        ROOT/'build/active/data', ROOT/'build/package/mainui-loose'))
    files = [dict(path=name, size=path.stat().st_size,
                  sha256=hashlib.sha256(path.read_bytes()).hexdigest()) for path, name in inputs]
    manifest = ROOT/'build/package/manifest.json'
    manifest.write_text(json.dumps(dict(status='Experimental candidate; H700 gameplay/performance unverified',
        binary_sha256=hashlib.sha256(b).hexdigest(), files=files), indent=2)+'\n')
    destination = ROOT/'dist/the-thing-candidate.tar.gz'
    destination.parent.mkdir(exist_ok=True)
    temporary = destination.with_name('.'+destination.name+'.tmp')
    try:
        with tarfile.open(temporary, 'w:gz', compresslevel=6) as out:
            for path, name in inputs:
                out.add(path, arcname=name, recursive=False)
            out.add(manifest, arcname='the-thing/manifest.json')
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)
    checksum=hashlib.sha256(destination.read_bytes()).hexdigest()
    destination.with_suffix(destination.suffix+'.sha256').write_text(f'{checksum}  {destination.name}\n')
    print(f'{destination}: {destination.stat().st_size} bytes; SHA-256 {checksum}')


if __name__ == '__main__':
    main()
