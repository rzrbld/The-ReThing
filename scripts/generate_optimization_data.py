#!/usr/bin/env python3
"""Generate private optimization patches from the user's supported translation.

The public selection manifest contains addresses and hashes only. No profiling
capture, previous build, or original/generated game code is needed from us.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def transform(body):
    clean = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
    if re.search(r'\bg_(?:fp_top|st|st_i64)\b|\b_fp_top\b', clean):
        raise ValueError('Unexpected direct floating-point stack access')
    if body.count('RECOMP_REGS_LOCALS') != 1:
        raise ValueError('Expected one local-register declaration')
    result = body.replace('RECOMP_REGS_LOCALS',
                          'RECOMP_REGS_LOCALS\n int _thing_fp_top=g_fp_top;')
    for old, new in [
        ('fp_push(', 'thing_fp_push(&_thing_fp_top,'),
        ('fp_pop()', 'thing_fp_pop(&_thing_fp_top)'),
        ('FP_ST(', 'THING_FP_ST('), ('FP_SI(', 'THING_FP_SI('),
        ('fp_xch(', 'THING_FP_XCH('), ('RECOMP_CALL(', 'THING_FP_CALL('),
        ('RECOMP_ICALL(', 'THING_FP_ICALL('),
    ]:
        result = result.replace(old, new)
    result = result.replace('void recomp_thething_',
        '__attribute__((optimize("fp-contract=off"))) void recomp_thething_', 1)
    return result.replace('RECOMP_REGS_OUT()',
                          '(g_fp_top=_thing_fp_top,RECOMP_REGS_OUT())')


def generate(exe, source, output):
    lock = json.loads((ROOT/'config/optimization-functions.json').read_text())
    if sha(exe.read_bytes()) != lock['exe_sha256']:
        raise ValueError('Unsupported executable; see BUILDING.md for the required SHA-256')
    expected = {x['address']: x for x in lock['functions']}
    if len(expected) != 83:
        raise ValueError('Invalid optimization selection manifest')
    found = {}
    pattern = r'void recomp_thething_([0-9A-F]+)\(void\) \{.*?(?=\nvoid recomp_|\Z)'
    for path in sorted(source.glob('functions_*.c')):
        for match in re.finditer(pattern, path.read_text(), re.S):
            address = match[1]
            if address not in expected:
                continue
            if address in found:
                raise ValueError(f'Duplicate function {address}')
            body = match[0]
            entry = expected[address]
            if sha(body.encode()) != entry['source_sha256']:
                raise ValueError(f'Translation mismatch at {address}; regenerate with this checkout')
            candidate = transform(body)
            if sha(candidate.encode()) != entry['candidate_sha256']:
                raise ValueError(f'Optimization mismatch at {address}')
            found[address] = dict(address=address, unit=path.name, original=body,
                                  candidate=candidate, source_sha256=entry['source_sha256'])
    missing = sorted(expected.keys() - found.keys())
    if missing:
        raise ValueError('Missing translated functions: ' + ', '.join(missing))
    patches = [found[x['address']] for x in lock['functions']]
    # Publish only after every input and generated replacement passes validation.
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(patches, indent=2) + '\n')
    temporary.replace(output)
    print(f'Generated {len(patches)} verified optimization patches: {output}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=ROOT/'build/active/bin/thething.exe')
    parser.add_argument('--source', type=Path, default=ROOT/'build/translated-local-regs/thething')
    parser.add_argument('--output', type=Path, default=ROOT/'build/optimization/patches.json')
    args = parser.parse_args()
    try:
        generate(args.exe, args.source, args.output)
    except (ValueError, OSError) as error:
        parser.exit(1, f'Cannot generate optimization data: {error}\n')


if __name__ == '__main__':
    main()
