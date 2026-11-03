#!/usr/bin/env python3
"""Stage hot integer-only bodies for a separate alias-analysis experiment.

Does not change game sources or build defaults. Calls retain their original
callee symbols and full emulated-state synchronization. This is preparation,
not validation or a claim that any candidate is faster.
"""
import hashlib
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root / 'build/guest-alias-integer-20261102'
out.mkdir(exist_ok=True)
profile = json.loads((root / 'docs/evidence/guest-alias-integer-20261102/candidates.json').read_text())
source_dir = root / 'build/translated-local-regs/thething'
parts = ['#define RECOMP_GENERATED_CODE 1\n#define RECOMP_LOCAL_REGS 1\n#include "functions.h"\n']
manifest = []
for item in profile['candidates']:
    source = (source_dir / item['unit']).read_text()
    start = source.index('void ' + item['name'] + '(void) {')
    end = source.find('\nvoid recomp_', start + 1)
    body = source[start:end if end >= 0 else len(source)].rstrip() + '\n'
    # Ignore comments for the integer-only gate, not actual expressions.
    code = re.sub(r'/\*.*?\*/|//[^\n]*', '', body, flags=re.S)
    assert not re.search(r'\b(?:float|double|FP_ST|FP_SI|FPU_CMP)\b|\b(?:g_)?fp_\w*', code), item['name']
    assert code.count('{') == code.count('}'), item['name']
    index = len(manifest)
    candidate = 'candidate' + str(index)
    parts.append(body.replace('void ' + item['name'] + '(', 'void ' + candidate + '(', 1))
    manifest.append(dict(item, candidate=candidate, original_sha256=hashlib.sha256(body.encode()).hexdigest()))
(out / 'candidates.c').write_text('\n'.join(parts))
(out / 'manifest.json').write_text(json.dumps(dict(
    note='UNVALIDATED staged bodies; no game mutation; no performance claim',
    source_include=str(source_dir), candidates=manifest,
    source_sha256=hashlib.sha256((out / 'candidates.c').read_bytes()).hexdigest()), indent=2) + '\n')
print(f'Staged {len(manifest)} integer-only functions; {sum(x["samples"] for x in manifest)} profile samples')
