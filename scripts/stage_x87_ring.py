#!/usr/bin/env python3
"""Fail-closed staging for the circular x87 stack.

The lifters emit direct `_st[i]` / `g_st_i64[i]` accesses and a few
`thing_fpu_env_*` calls that assume the old top-first shifting layout. This
transform rewrites them to the logical-slot accessors FP_ST(i)/FP_SI(i) and to
the window-materialising fp_env_store/fp_env_load wrappers from recomp_types.h.
It never edits its input tree; a transformed copy is written instead.

Usage:
  stage_x87_ring.py --tree build/translated build/translated-x87
  stage_x87_ring.py --file build/game/recovered.c
"""
import argparse
import collections
import hashlib
import json
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOKEN = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_]\w*|[^\s]', re.S)
ENV_STORE = re.compile(
    r'thing_fpu_env_store28\(\s*(\(void\*\)ADDR\(esp\))\s*,\s*_st\s*,\s*_fp_top\s*,\s*&_fpu_cw\s*,\s*([^()]+?)\s*\)')
ENV_LOAD = re.compile(
    r'thing_fpu_env_load28\(\s*(\(const void\*\)ADDR\(esp\))\s*,\s*_st\s*,\s*_fp_top\s*,\s*&_fpu_cw\s*\)')


def tokens(src):
    return [(m.group(), m.start(), m.end()) for m in TOKEN.finditer(src)]


def transform(source, stats):
    source, nstore = ENV_STORE.subn(lambda m: f'fp_env_store({m.group(1)}, _fp_top, &_fpu_cw, {m.group(2)})', source)
    source, nload = ENV_LOAD.subn(lambda m: f'fp_env_load({m.group(1)}, _fp_top, &_fpu_cw)', source)
    stats['env_store'] += nstore
    stats['env_load'] += nload
    t = tokens(source)
    edits = []
    for i, (tok, start, end) in enumerate(t):
        if tok not in ('_st', 'g_st_i64'):
            continue
        if i + 3 >= len(t):
            raise SystemExit(f'unexpected trailing {tok}')
        if t[i + 1][0] != '[' or t[i + 3][0] != ']':
            raise SystemExit(f'{tok} not followed by [<constant>]: tokens {t[i:i+4]}')
        num = t[i + 2][0]
        if not num.isdigit():
            raise SystemExit(f'{tok} index is not a literal: {num}')
        macro = 'FP_ST' if tok == '_st' else 'FP_SI'
        edits.append((start, t[i + 3][2], f'{macro}({num})'))
        stats[macro + '_rewritten'] += 1
    for start, end, text in sorted(edits, reverse=True):
        source = source[:start] + text + source[end:]
    # Fail closed: no old-layout access may survive.
    for tok, _, _ in tokens(source):
        if tok in ('_st', 'g_st_i64'):
            raise SystemExit(f'leftover {tok} access after transform')
    return source


def write_if_changed(path, text):
    if path.exists() and path.read_text() == text:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--tree', nargs=2, metavar=('SOURCE', 'OUTPUT'))
    g.add_argument('--file', type=Path)
    args = ap.parse_args()
    stats = collections.Counter()
    changed = 0
    rows = []
    if args.tree:
        source_dir, output_dir = (Path(x) for x in args.tree)
        source_dir = source_dir.resolve()
        output_dir = output_dir.resolve()
        if source_dir == output_dir:
            raise SystemExit('refusing to transform a tree onto itself')
        for src in sorted(source_dir.rglob('*')):
            if not src.is_file() or src.suffix not in ('.c', '.h'):
                continue
            rel = src.relative_to(source_dir)
            dest = output_dir / rel
            if (src.name.startswith('functions_') and src.suffix == '.c') or src.name == 'recovered.c':
                before = src.read_text()
                after = transform(before, stats)
                if write_if_changed(dest, after):
                    changed += 1
                rows.append({'source': str(src), 'output': str(dest),
                             'sha256_in': hashlib.sha256(before.encode()).hexdigest(),
                             'sha256_out': hashlib.sha256(after.encode()).hexdigest()})
            else:
                dest.parent.mkdir(parents=True, exist_ok=True)
                if not dest.exists() or dest.read_bytes() != src.read_bytes():
                    shutil.copy2(src, dest)
        report = output_dir / 'x87-ring-report.json'
    else:
        path = args.file.resolve()
        before = path.read_text()
        after = transform(before, stats)
        changed = 1 if write_if_changed(path, after) else 0
        rows.append({'source': str(path), 'output': str(path),
                     'sha256_in': hashlib.sha256(before.encode()).hexdigest(),
                     'sha256_out': hashlib.sha256(after.encode()).hexdigest()})
        report = ROOT / 'docs/evidence/x87-ring-recovered-report.json'
    payload = {'stats': dict(stats), 'changed': changed, 'files': rows}
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(payload, indent=2) + '\n')
    print('x87-ring staged:', dict(stats), 'changed', changed)


if __name__ == '__main__':
    main()
