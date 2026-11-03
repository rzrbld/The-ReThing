#!/usr/bin/env python3
"""Isolated flag-call candidate from verified original prefixes, no production edits."""
import hashlib,json,re,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[1];audit=json.loads((root/'docs/evidence/call-flag-inputs/audit.json').read_text())
source=root/'build/translated-local-regs';target=root/'build/call-flag-inputs/translated'
assert hashlib.sha256((root/'build/active/bin/thething.exe').read_bytes()).hexdigest()==audit['original_pe_sha256']
proofs={row['va'][2:]:row for row in audit['proofs']}
# Reject stale proofs after any native hook/translation edit.
for path in sorted((source/'thething').glob('functions_*.c')):
 chunks=re.split(r'void recomp_thething_([0-9A-F]{8})\(void\) \{',path.read_text())
 for i in range(1,len(chunks),2):
  if chunks[i] in proofs:assert hashlib.sha256(chunks[i+1].encode()).hexdigest()==proofs[chunks[i]]['body_sha256'],chunks[i]
rows=[]
for path in sorted(source.rglob('*')):
 if not path.is_file() or path.suffix not in ('.c','.h'):continue
 dest=target/path.relative_to(source);dest.parent.mkdir(parents=True,exist_ok=True)
 text=path.read_text();count=[0]
 if path.parent.name=='thething' and path.name.startswith('functions_'):
  def replace(m):
   if m[1] not in proofs:return m[0]
   count[0]+=1;return 'RECOMP_CALL_FLAGS_KILLED(recomp_thething_'+m[1]+')'
  text=re.sub(r'RECOMP_CALL\(recomp_thething_([0-9A-F]{8})\)',replace,text)
  if count[0]:text=text.replace('#include "functions.h"','#include "functions.h"\n#include "../fast_call.h"',1)
 if not dest.exists() or dest.read_text()!=text:dest.write_text(text)
 if count[0]:rows.append(dict(source=str(path.relative_to(root)),calls=count[0],sha256=hashlib.sha256(text.encode()).hexdigest()))
shutil.copy2(root/'probe/flags/proven_call.h',target/'fast_call.h')
(root/'docs/evidence/call-flag-inputs/staging.json').write_text(json.dumps(rows,indent=2)+'\n')
print('Staged',sum(r['calls'] for r in rows),'calls in',len(rows),'units; production translation untouched')
