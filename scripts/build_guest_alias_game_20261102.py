#!/usr/bin/env python3
"""Isolated four-leaf GCC-fallback/LLVM-fast game build; restore baseline."""
import hashlib,json,shutil,subprocess,sys,os,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
target='aarch64' if '--target=aarch64' in sys.argv else 'host'
variant='guest-alias-arm-game-20261102' if target=='aarch64' else 'guest-alias-game-20261102'
out=root/'build'/variant;backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
selected=[(i,p)for i,p in enumerate(json.loads((root/'build/leaf-registers-20261102/patches.json').read_text()))if i!=2]
clang=root/'build/tools/llvm21/usr/bin/clang-21';extract=root/'build/tools/llvm21/usr/bin/llvm-extract-21'
subprocess.run([str(extract),'-S',*['--func=fast_candidate'+str(i)for i,p in selected],str(root/'build/guest-alias-guarded-20261102'/('arm' if target=='aarch64' else 'host')/'leaves.ll'),'-o',str(out/'fast.ll')],check=True)
ir=(out/'fast.ll').read_text()
for i,p in selected:ir=ir.replace('@fast_candidate'+str(i)+'(', '@thing_alias_fast_'+p['address']+'(')
(out/'fast.ll').write_text(ir)
target_flags=['--target=aarch64-linux-gnu','-mcpu=cortex-a53']if target=='aarch64'else[]
subprocess.run([str(clang),*target_flags,'-O2','-ffp-contract=off','-c',str(out/'fast.ll'),'-o',str(out/'fast.o')],check=True)
units={}
for path in (root/'build/translated-local-regs/thething').glob('functions_*.c'):
 s=path.read_text();items=[(i,p)for i,p in selected if p['original'] in s]
 if items:units[path]=items
assert sum(map(len,units.values()))==4
paths=[]
for p in units:paths += [p,root/'build/game'/target/'thething'/(p.stem+'.o'),root/'build/game'/target/'thething'/(p.stem+'.flags')]
paths += [root/'build/game'/target/'thething/lifted.a',root/'build/game'/('thething.'+target),root/'build/game'/('thething.'+target+'.sha256')]
paths += sorted(p for p in (root/'build/game'/target).iterdir()if p.is_file())
def validate_candidate(candidate):
 # Honor the actual target IR requirements: ARM arrays require 8, host 16.
 symbols=subprocess.check_output(['readelf','-sW',str(candidate)],text=True)
 required={}
 for name in ('g_st','g_st_i64'):
  match=re.search(r'^@'+name+r' = .*align (\d+)$',ir,re.M)
  assert match,name
  alignment=int(match.group(1));required[name]=alignment
  found=[line.split()for line in symbols.splitlines()if line.split()and line.split()[-1]==name]
  assert found and all(int(fields[1],16)%alignment==0 for fields in found),name
 return required
if '--verify-existing' in sys.argv:
 candidate=out/('diagnostic.'+target)
 restored={}
 for i,p in enumerate(paths):
  expected=hashlib.sha256((backup/str(i)).read_bytes()).hexdigest()
  assert hashlib.sha256(p.read_bytes()).hexdigest()==expected,str(p)
  restored[str(p.relative_to(root))]=expected
 alignment=validate_candidate(candidate)
 e=root/'docs/evidence'/variant;e.mkdir(exist_ok=True)
 (e/'build.json').write_text(json.dumps(dict(restored=restored,selected=[p['address']for i,p in selected],candidate_sha256=hashlib.sha256(candidate.read_bytes()).hexdigest(),object_sha256=hashlib.sha256((out/'fast.o').read_bytes()).hexdigest(),required_alignment=alignment,recovered_after_alignment_check=True),indent=2)+'\n')
 print('Existing candidate validated; all baseline files match build backups')
 sys.exit(0)
manifest={}
for i,p in enumerate(paths):shutil.copy2(p,backup/str(i));manifest[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
try:
 for path,items in units.items():
  s=path.read_text();declarations=['#include "../../../probe/guest_alias_guards.h"']
  for i,p in items:
   name='recomp_thething_'+p['address'];fallback='thing_alias_original_'+p['address'];fast='thing_alias_fast_'+p['address']
   original=p['original'].replace('void '+name,'__attribute__((noinline,noclone)) static void '+fallback,1)
   wrapper=f'\nvoid {name}(void){{if(thing_alias_eligible({i})){fast}();else {fallback}();}}\n'
   assert s.count(p['original'])==1;s=s.replace(p['original'],original+wrapper,1)
   declarations.append(f'extern void {fast}(void);')
  s=s.replace('#include "functions.h"','#include "functions.h"\n'+'\n'.join(declarations),1);path.write_text(s)
 command=['python3','-u','scripts/build_game.py','--target',target,'--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4','--extra-object',str((out/'fast.o').relative_to(root))]
 if target=='aarch64':command=['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{root}:/src:ro','-v',f'{root}/build/game:/src/build/game:rw','--workdir','/src','sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49',*command]
 with (out/'build.log').open('w') as log:subprocess.run(command,cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 candidate=out/('diagnostic.'+target)
 shutil.copy2(root/'build/game'/('thething.'+target),candidate)
 validate_candidate(candidate)
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==manifest[str(p.relative_to(root))]
e=root/'docs/evidence'/variant;e.mkdir(exist_ok=True)
(e/'build.json').write_text(json.dumps(dict(restored=manifest,selected=[p['address']for i,p in selected],candidate_sha256=hashlib.sha256(candidate.read_bytes()).hexdigest(),object_sha256=hashlib.sha256((out/'fast.o').read_bytes()).hexdigest()),indent=2)+'\n')
print('Four-leaf guarded game built; normal source/artifacts restored')
