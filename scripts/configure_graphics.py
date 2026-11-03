#!/usr/bin/env python3
"""Create a graphics registry candidate while preserving all unrelated lines.

Input/output are explicit and must differ. Numeric game settings are REG_SZ,
matching the original game's registry readers. Stop the game before installing
the resulting file. This tool does not deploy or modify a running installation.
"""
import argparse
import json
import os
from pathlib import Path
import stat
import tempfile

ROOT = Path(__file__).resolve().parents[1]
REGISTRY_PATH = r'HKEY_CURRENT_USER\SOFTWARE\Computer Artworks\The Thing\1.0'


def canonical_key(path):
    parts = [part for part in path.split('\\') if part]
    if parts and parts[0].casefold() == 'hkcu':
        parts[0] = 'HKEY_CURRENT_USER'
    return '\\'.join(parts).casefold()


def configure(source, settings):
    prefix = canonical_key(REGISTRY_PATH) + '\\'
    desired = {key.casefold(): (key, str(value)) for key, value in settings.items()}
    seen, changes, output = set(), [], []
    newline = '\r\n' if '\r\n' in source else '\n'
    untouched = 0
    for line in source.splitlines(keepends=True):
        stripped = line.rstrip('\r\n')
        key, separator, old_value = stripped.partition('=')
        canonical = canonical_key(key)
        name = canonical[len(prefix):] if canonical.startswith(prefix) else ''
        if separator and name in desired and '\\' not in name:
            display_name, value = desired[name]
            suffix = line[len(stripped):] or newline
            output.append(key + '=sz:' + value + suffix)
            seen.add(name)
            changes.append(dict(key=display_name, previous=old_value, requested='sz:' + value))
        else:
            output.append(line)
            untouched += 1
    if output and not output[-1].endswith(('\r', '\n')):
        output[-1] += newline
    for name, (display_name, value) in desired.items():
        if name not in seen:
            output.append(REGISTRY_PATH + '\\' + display_name + '=sz:' + value + newline)
            changes.append(dict(key=display_name, previous=None, requested='sz:' + value))
    return ''.join(output), changes, untouched


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True, help='Existing native registry.txt to preserve')
    parser.add_argument('--output', type=Path, required=True, help='Separate candidate registry.txt')
    parser.add_argument('--preset', type=Path, default=ROOT / 'config/graphics-low.json')
    parser.add_argument('--texture-bias', type=int, choices=range(-4, 1), help='Override shared texture-loader bias; negative values reduce eligible textures')
    parser.add_argument('--max-texture-size', type=int, choices=[16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384], help='Override shared texture-loader dimension cap; can affect UI/fonts')
    args = parser.parse_args()
    if args.input.resolve() == args.output.resolve():
        parser.error('Input and output must differ; keep the original registry for rollback')
    preset = json.loads(args.preset.read_text())
    if preset.get('schema_version') != 1 or canonical_key(preset.get('registry_path', '')) != canonical_key(REGISTRY_PATH):
        parser.error('Unsupported graphics preset schema or registry path')
    settings = dict(preset['settings'])
    if args.texture_bias is not None:
        settings['TextureLODBias'] = str(args.texture_bias)
    if args.max_texture_size is not None:
        settings['MaxTextureSize'] = str(args.max_texture_size)
    if any(not name or '\\' in name or '=' in name or '\n' in str(value) or '\r' in str(value)
           for name, value in settings.items()):
        parser.error('Preset has an invalid key/value')
    with args.input.open('r', encoding='utf-8', newline='') as handle:
        source = handle.read()
    output, changes, untouched = configure(source, settings)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=args.output.parent,
                                         prefix='.registry-graphics-', delete=False) as handle:
            temporary = Path(handle.name)
            handle.write(output)
        os.chmod(temporary, stat.S_IMODE(args.input.stat().st_mode))
        os.replace(temporary, args.output)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()
    print(json.dumps(dict(input=str(args.input), output=str(args.output), preset=str(args.preset),
                         status=preset['status'], changed_or_added_values=changes,
                         unrelated_lines_preserved=untouched,
                         note='Candidate only; texture settings may affect UI/fonts. No deployment or gameplay/performance validation performed.'), indent=2))


if __name__ == '__main__':
    main()
