#!/usr/bin/env python3
"""Hash/byte-checked, read-only diagnostic hooks; does not change guest behavior."""
import argparse,hashlib,json,os
from pathlib import Path
import pefile
ROOT=Path(__file__).resolve().parents[1]
HASH='085d1a90b2bd57967e8e911eb1f7c2232fd4b045b7f363c5c4a3056f720abb49'
SITES={0x768827:bytes.fromhex('389c24900000007541'),0x76887d:bytes.fromhex('e82e08000084c0')}
MISSING='L_00768830:\n'
CALL='    RECOMP_CALL(recomp_thething_007690B0); /* 0x0076887D: call 0x7690b0 */\n    RECOMP_FLAGS_IN();\n'
MISSING_HOOK='    thing_font_diagnostic(0x768830u, MEM32(esp + 0x10), MEM32(esp + 0x24), esp + 0x90, 0); /* checked font resolve diagnostic */\n'
CALL_HOOK='    thing_font_diagnostic(0x76887du, MEM32(esp + 0x10), MEM32(esp + 0x24), esp + 0x90, LO8(eax)); /* checked font loader diagnostic */\n'

def patch(source):
    if source.count(MISSING)!=1 or source.count(CALL)!=1:raise ValueError('Expected exactly one font creation branch/callsite')
    for hook in (MISSING_HOOK,CALL_HOOK):
        if source.count(hook)>1:raise ValueError('Duplicate diagnostic hook')
    if MISSING_HOOK not in source:source=source.replace(MISSING,MISSING+MISSING_HOOK)
    if CALL_HOOK not in source:source=source.replace(CALL,CALL+CALL_HOOK)
    if '#include "font_diagnostic.h"' not in source:source=source.replace('#include "functions.h"','#include "functions.h"\n#include "font_diagnostic.h"',1)
    return source

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--apply',action='store_true');p.add_argument('--image',type=Path,default=ROOT/'build/active/bin/thething.exe');p.add_argument('--source',type=Path,default=ROOT/'build/translated/thething/functions_0219.c');a=p.parse_args()
    if hashlib.sha256(a.image.read_bytes()).hexdigest()!=HASH:raise SystemExit('Refusing unknown game executable hash')
    pe=pefile.PE(str(a.image));base=pe.OPTIONAL_HEADER.ImageBase
    for va,b in SITES.items():
        if pe.get_data(va-base,len(b))!=b:raise SystemExit(f'Refusing changed font instructions at{va:#x}')
    before=a.source.read_text();after=patch(before)
    report={'input_sha256':HASH,'sites':{hex(k):v.hex() for k,v in SITES.items()},'source':str(a.source),'before_sha256':hashlib.sha256(before.encode()).hexdigest(),'after_sha256':hashlib.sha256(after.encode()).hexdigest(),'changed':before!=after,'behavior':'read-only error/opt-in diagnostics; no guest register or flag updates'}
    print(json.dumps(report,indent=2))
    if a.apply and before!=after:
        tmp=a.source.with_name(a.source.name+'.font-diagnostic-tmp');tmp.write_text(after);os.replace(tmp,a.source)
        (ROOT/'docs/evidence/font-diagnostic-patch.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
