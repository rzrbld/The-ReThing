#!/usr/bin/env python3
"""Audit .gitignore candidates without initializing or staging the research tree."""
import hashlib,json,re,subprocess,tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='rething-git-audit-') as t:
 g=Path(t)/'repo.git';subprocess.run(['git','init','--bare','--quiet',str(g)],check=True)
 command=['git','--git-dir='+str(g),'--work-tree='+str(R)]
 entries=subprocess.check_output(command+['ls-files','--others','--exclude-standard','-z'],cwd=R).decode().split('\0')
 entries=[n for n in entries if n]
 assert all(n in entries for n in ('.gitignore','README.md','BUILDING.md','LICENSE','THIRD_PARTY.md','launcher/native_setup.cpp','packaging/portmaster/screenshot.png'))
 forbidden=['build/active/bin/thething.exe','dist/the_rething.zip','The_NEW_ISO_UNPROTECTED/the_thing.iso','docs/evidence/translation-summary.json','docs/analysis/disassembly.md','.agents/state.json','.codex/config.toml','.aws/credentials','HANDOFF_PERFORMANCE_2026-11-01.md','scripts/test_original.exe','runtime/test_original.dll','config/Slot1.sav','scripts/new-experiment.py','packaging/The Thing GPU.sh']
 for n in forbidden:
  p=subprocess.run(command+['check-ignore','-q','--',n],cwd=R)
  assert p.returncode==0,'Not ignored: '+n
 manifest=[]
 for n in entries:
  p=R/n
  assert not p.is_symlink(),'Review symlink before publication: '+n
  data=p.read_bytes()
  assert not data.startswith((b'\x7fELF',b'MZ',b'PK\x03\x04')), 'Unexpected compiled/archive file: '+n
  assert not re.search(rb'^-----BEGIN (?:OPENSSH |RSA |EC )?PRIVATE KEY-----',data,re.M),'Private key: '+n
  assert not any(x in Path(n).parts for x in ('.agents','.codex','.aws','__pycache__'))
  manifest.append({'path':n,'size':len(data),'sha256':hashlib.sha256(data).hexdigest()})
 out=R/'build/publication';out.mkdir(parents=True,exist_ok=True)
 (out/'source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 print('PASS ignore rules: original files, archives, generated code, evidence, agent state and user data excluded')
 print('Candidate source files:',len(manifest),'total bytes:',sum(x['size'] for x in manifest))
 print('Review manifest:',out/'source-manifest.json')
