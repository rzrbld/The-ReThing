#!/usr/bin/env python3
"""Base install -> official 1.1 -> official 1.2 -> supplied clear EXE.

The old BIN/CUE is not required. Originals remain unchanged. Native xdelta
checksum verification remains enabled for every patch.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ISO = Path(os.environ.get('THING_ISO', str(ROOT / 'The_NEW_ISO_UNPROTECTED/the_thing.iso'))).resolve()
ISO_SHA = '1f3ac2916d8185f0e76b811fe24bc15af9412bf4da18f4bf4579ee483683fe61'
EXE_SHA = '085d1a90b2bd57967e8e911eb1f7c2232fd4b045b7f363c5c4a3056f720abb49'


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for data in iter(lambda: f.read(1024 * 1024), b''):
            h.update(data)
    return h.hexdigest()


def inventory(root):
    return [dict(path=str(p.relative_to(root)), size=p.stat().st_size, sha256=digest(p))
            for p in sorted(root.rglob('*')) if p.is_file()]


def run(command, log, library=None):
    env = os.environ.copy()
    if library:
        env['LD_LIBRARY_PATH'] = str(library)
    with log.open('w') as output:
        subprocess.run([str(x) for x in command], env=env, check=True,
                       stdout=output, stderr=subprocess.STDOUT)


def verify(root, files):
    for entry in files:
        path = root / entry['path']
        if not path.is_file() or digest(path) != entry['sha256']:
            raise RuntimeError(f'Checksum mismatch: {path}')


def main():
    evidence = ROOT / 'docs/evidence'
    build = ROOT / 'build'
    evidence.mkdir(parents=True, exist_ok=True)
    if digest(ISO) != ISO_SHA:
        raise RuntimeError('Supplied ISO does not match recorded SHA-256')
    iso_root = build / 'new-iso'
    iso_manifest = evidence / 'new-iso.json'
    if not iso_root.exists():
        run(['7z', 'x', '-y', ISO, f'-o{iso_root}'], evidence / 'new-iso-extraction.log')
    if iso_manifest.exists():
        verify(iso_root, json.loads(iso_manifest.read_text())['files'])
    else:
        iso_manifest.write_text(json.dumps(dict(iso=str(ISO), sha256=ISO_SHA,
            size=ISO.stat().st_size, files=inventory(iso_root)), indent=2) + '\n')
    cabinets = {'data1.cab': '304633852a386e4f16410500122750751379073ab74813bfbbe8240835c5b177',
                'data1.hdr': '0585fae90465a911d9c134672f0b24337d49aa9f86ac4a2af27f4e3e63913b7c',
                'data2.cab': '8c52557a9d2892db9e443f926f6785f52151a1c834f04c21514bb99c74ab595f'}
    for name, expected in cabinets.items():
        if digest(iso_root / name) != expected:
            raise RuntimeError(f'Unexpected base cabinet: {name}')
    payload = build / 'game-data'
    if not payload.exists():
        tool = Path(shutil.which('unshield')) if os.environ.get('THING_SYSTEM_TOOLS') == '1' else build / 'tools/unshield/usr/bin/unshield'
        run([tool, '-d', payload, 'x', iso_root / 'data1.cab'],
            evidence / 'new-iso-payload-extract.log', None if os.environ.get('THING_SYSTEM_TOOLS') == '1' else tool.parents[1] / 'lib/x86_64-linux-gnu')
    payload_manifest = evidence / 'payload.json'
    if payload_manifest.exists():
        verify(payload, json.loads(payload_manifest.read_text())['files'])
    else:
        payload_manifest.write_text(json.dumps(dict(files=inventory(payload)), indent=2) + '\n')
    patches = {}
    for version, dirname, filename in [('11', 'Version 1.1 Eng', 'thething_patch_10_11.exe'),
                                        ('12', 'Version 1.2 Eng', 'thething_patch_11_12.exe')]:
        destination = build / f'patch{version}'
        if not (destination / 'Disk1').exists():
            run(['7z', 'x', '-y', iso_root / 'UPDATE' / dirname / filename, f'-o{destination}'],
                evidence / f'patch{version}-extract.log')
        patches[version] = destination / 'Disk1'
    active = build / 'active'
    manifest = evidence / 'patched-v12.json'
    if active.exists() and manifest.exists():
        verify(active, json.loads(manifest.read_text())['files'])
        print('Verified existing fully patched 1.2 working installation')
        return
    if active.exists():
        raise RuntimeError('build/active exists without manifest; preserve it before rerunning preparation')
    operations = []
    with tempfile.TemporaryDirectory(prefix='thing-v12-', dir=build) as tmp:
        stage = Path(tmp) / 'install'
        shutil.copytree(payload / 'US_bin/bin', stage / 'bin')
        for component in ['levels', 'English_data', 'FMV', 'Default_language_resources', 'Global_data']:
            shutil.copytree(payload / component / 'data', stage / 'data', dirs_exist_ok=True)
        tool = Path(shutil.which('xdelta')) if os.environ.get('THING_SYSTEM_TOOLS') == '1' else build / 'tools/xdelta/usr/bin/xdelta'
        library = None if os.environ.get('THING_SYSTEM_TOOLS') == '1' else tool.parents[1] / 'lib/x86_64-linux-gnu'
        targets = [('thething.exe', 'bin/thething.exe'), ('english.pak', 'data/english.pak'),
                   ('resources.pak', 'data/resources.pak'), ('mainui.pak', 'data/mainui.pak'),
                   ('level5a.dll', 'data/Levels/level5a.dll'), ('level6.dll', 'data/Levels/level6.dll')]
        for version, updates in [('11', targets), ('12', [('TheThing.exe', 'bin/thething.exe')])]:
            for delta, target in updates:
                old = stage / target
                new = old.with_suffix(old.suffix + '.patched')
                before = digest(old)
                run([tool, 'patch', patches[version] / delta, old, new],
                    evidence / f'patch{version}-{delta}.log', library)
                operations.append(dict(patch=version, delta=delta, delta_sha256=digest(patches[version]/delta),
                    target=target, before_sha256=before, after_sha256=digest(new),
                    xdelta_checksum_verification=True))
                new.replace(old)
        replacement = iso_root / 'UPDATE/Version 1.2 Eng/CRACK/TheThing.exe'
        if digest(replacement) != EXE_SHA:
            raise RuntimeError('Unexpected supplied 1.2 unprotected executable')
        operations.append(dict(operation='Select supplied unprotected 1.2 executable',
            source=str(replacement.relative_to(ROOT)), protected_sha256=digest(stage/'bin/thething.exe'),
            unprotected_sha256=EXE_SHA))
        shutil.copy2(replacement, stage / 'bin/thething.exe')
        result = dict(iso_sha256=ISO_SHA, sequence=['original install', 'official 1.1', 'official 1.2',
                       'supplied unprotected 1.2 executable'], operations=operations, files=inventory(stage))
        stage.rename(active)
        manifest.write_text(json.dumps(result, indent=2) + '\n')
    print('Prepared build/active: original -> official 1.1 -> official 1.2 -> supplied clear 1.2 executable')


if __name__ == '__main__':
    main()
