#!/usr/bin/env python3
"""Patch only the verified 1.2 CRT FNSTENV/status edit/FLDENV pairs.

General FNSTENV/FLDENV support remains absent from the lifter. The original PE
and raw failure ledger are preserved. Generated code is replaced atomically.
"""
import argparse
import collections
import hashlib
import json
import os
from pathlib import Path
import pefile

ROOT=Path(__file__).resolve().parents[1]
EXPECTED='085d1a90b2bd57967e8e911eb1f7c2232fd4b045b7f363c5c4a3056f720abb49'
SEQUENCE=bytes.fromhex('d9342481642404ffbc000009442404d92424')
SITES={0x7bfb53:('fnstenv',1),0x7bfb62:('fldenv',1),0x7bfe0b:('fnstenv',3),0x7bfe1a:('fldenv',3)}


def atomic(path,data):
    temporary=path.with_name(path.name+'.fpu-patch-tmp')
    temporary.write_text(data)
    os.replace(temporary,path)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply',action='store_true')
    args=parser.parse_args()
    image=ROOT/'build/active/bin/thething.exe'
    digest=hashlib.sha256(image.read_bytes()).hexdigest()
    if digest!=EXPECTED:raise SystemExit('Refusing unknown game executable hash')
    pe=pefile.PE(str(image));base=pe.OPTIONAL_HEADER.ImageBase
    for va in (0x7bfb53,0x7bfe0b):
        if pe.get_data(va-base,len(SEQUENCE))!=SEQUENCE:
            raise SystemExit(f'Refusing changed CRT environment sequence at {va:#x}')
    changes=[];counts=collections.Counter()
    for path in sorted((ROOT/'build/translated/thething').glob('functions_*.c')):
        original=path.read_text();source=original
        for address,(mnemonic,expected) in SITES.items():
            trap=f'ds_unsupported_instruction(0x{address:08X}u, "{mnemonic} [esp]");'
            marker=f'/* checked x87 environment {address:#x} */'
            n=source.count(trap);already=source.count(marker)
            counts[address]+=n+already
            if mnemonic=='fnstenv':
                replacement='thing_fpu_env_store28((void*)ADDR(esp), _st, _fp_top, &_fpu_cw, _fpu_cmp); '+marker
            else:
                replacement='_fpu_cmp = thing_fpu_env_load28((const void*)ADDR(esp), _st, _fp_top, &_fpu_cw); '+marker
            source=source.replace(trap,replacement)
        if source!=original:
            if '#include "fpu_env.h"' not in source:
                source=source.replace('#include "functions.h"','#include "functions.h"\n#include "fpu_env.h"',1)
            changes.append((path,source,hashlib.sha256(original.encode()).hexdigest(),hashlib.sha256(source.encode()).hexdigest()))
    for address,(_,expected) in SITES.items():
        if counts[address]!=expected:raise SystemExit(f'Unexpected emitted count at {address:#x}: {counts[address]} != {expected}')
    report_path=ROOT/'docs/evidence/translation-thething.json'
    report=json.loads(report_path.read_text())
    if report['sha256']!=EXPECTED:raise SystemExit('Translation report input differs from selected executable')
    removed=[e for e in report['unsupported'] if int(e['address'],16) in SITES]
    if len(removed) not in (0,8):raise SystemExit('Unexpected environment failure ledger count')
    print(f'Verified {sum(counts.values())} emitted sites at {len(SITES)} unique addresses; {len(changes)} source files change')
    if not args.apply:raise SystemExit(0)
    raw=ROOT/'docs/evidence/v12-raw-ledger.json'
    if not raw.exists():
        if len(report['unsupported'])!=27:raise SystemExit('Raw baseline ledger must retain all 27 original failures')
        raw.write_text(json.dumps(report,indent=2)+'\n')
    for path,source,_,_ in changes:atomic(path,source)
    report['unsupported']=[e for e in report['unsupported'] if int(e['address'],16) not in SITES]
    report['unsupported_mnemonics']=dict(collections.Counter(e['instruction'].split()[0] for e in report['unsupported']))
    note='Only the hash-checked CRT status-edit FNSTENV/FLDENV pairs use restricted helpers; all other environment forms remain unsupported.'
    if note not in report['limitations']:report['limitations'].append(note)
    report['checked_fpu_environment_sites']=[hex(x) for x in SITES]
    atomic(report_path,json.dumps(report,indent=2)+'\n')
    summary_path=ROOT/'docs/evidence/translation-summary.json'
    summary=json.loads(summary_path.read_text())
    summary=[report if m['module'].lower()=='thething.exe' else m for m in summary]
    atomic(summary_path,json.dumps(summary,indent=2)+'\n')
    evidence=dict(input_sha256=digest,sequences={hex(va):SEQUENCE.hex() for va in (0x7bfb53,0x7bfe0b)},
        emitted_sites={hex(va):count for va,count in counts.items()},
        unsupported_remaining=len(report['unsupported']),native_test='docs/evidence/fpu-env-selftest.log',
        sources=[dict(path=str(p.relative_to(ROOT)),before=before,after=after) for p,_,before,after in changes],
        limitations=['Only status C bits change between paired instructions.',
            'TOP, tag, CW, pointer, register and exception-bit edits are guarded unsupported.',
            'The fixed-window x87 model does not track sticky exceptions or instruction/data pointers.',
            'Force recompilation of changed generated units after any concurrent compile finishes.'])
    evidence_path=ROOT/'docs/evidence/fpu-env-patch.json'
    if changes or not evidence_path.exists():atomic(evidence_path,json.dumps(evidence,indent=2)+'\n')
    print(f'Applied checked helpers; {len(report["unsupported"])} fatal unsupported sites remain')
