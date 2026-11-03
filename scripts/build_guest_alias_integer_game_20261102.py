#!/usr/bin/env python3
"""Isolated host game trial of 70 integer bodies; restore all baseline files."""
import hashlib,json,shutil,subprocess,sys,re,os
from pathlib import Path
root=Path(__file__).resolve().parents[1]
variant=sys.argv[1] if len(sys.argv)>1 else 'alias'
assert variant in ('alias','plain')
target='aarch64' if '--target=aarch64' in sys.argv else 'host'
arch='arm' if target=='aarch64' else 'host'
build_name='guest-alias-integer-game-'+variant+('-arm' if target=='aarch64' else '')+'-20261102'
source=root/'build/guest-alias-integer-20261102'
manifest=json.loads((source/'manifest.json').read_text())
out=root/'build'/build_name
backup=out/'backup';backup.mkdir(parents=True,exist_ok=True)
units={}
for item in manifest['candidates']:units.setdefault(item['unit'],[]).append(item)
ir=(source/arch/(variant+'.ll')).read_text()
for item in manifest['candidates']:
 ir=re.sub(r'^declare[^\n]*@'+item['name']+r'\([^\n]*\n','',ir,flags=re.M)
 ir=ir.replace('@'+item['candidate']+'(', '@'+item['name']+'(')
(out/'game.ll').write_text(ir)
subprocess.run([str(root/'build/tools/llvm21/usr/bin/clang-21'),*(['--target=aarch64-linux-gnu','-mcpu=cortex-a53'] if target=='aarch64' else []),'-O2','-c',str(out/'game.ll'),'-o',str(out/'game.o')],check=True)
paths=[]
for unit in units:
 p=root/'build/translated-local-regs/thething'/unit
 paths += [p,root/'build/game'/target/'thething'/(p.stem+'.o'),root/'build/game'/target/'thething'/(p.stem+'.flags')]
paths += [root/'build/game'/target/'thething/lifted.a',root/'build/game'/('thething.'+target),root/'build/game'/('thething.'+target+'.sha256')]
paths += sorted(p for p in (root/'build/game'/target).iterdir() if p.is_file())
hashes={}
for i,p in enumerate(paths):
 shutil.copy2(p,backup/str(i));hashes[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
(backup/'manifest.json').write_text(json.dumps(dict(files=[str(p.relative_to(root))for p in paths],hashes=hashes),indent=2)+'\n')
try:
 for unit,items in units.items():
  p=root/'build/translated-local-regs/thething'/unit;s=p.read_text()
  replacements=[]
  for item in items:
   a=s.index('void '+item['name']+'(void) {');b=s.find('\nvoid recomp_',a+1)
   body=s[a:b if b>=0 else len(s)].rstrip()+'\n'
   assert hashlib.sha256(body.encode()).hexdigest()==item['original_sha256'],item['name']
   replacements.append((a,b if b>=0 else len(s),item['name']))
  for a,b,name in sorted(replacements,reverse=True):
   s=s[:a]+'/* '+name+' supplied by experimental LLVM object. */\n'+s[b:]
  p.write_text(s)
 with (out/'build.log').open('w') as log:
  command=['python3','-u','scripts/build_game.py','--target',target,'--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4','--extra-object',str((out/'game.o').relative_to(root))]
  if target=='aarch64':
   command=['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{root}:/src:ro','-v',f'{root}/build/game:/src/build/game:rw','--workdir','/src','sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49',*command]
  subprocess.run(command,cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 candidate=out/('diagnostic.'+target);shutil.copy2(root/'build/game'/('thething.'+target),candidate)
 symbols=subprocess.check_output(['readelf','-sW',str(candidate)],text=True)
 for name,align in re.findall(r'^@(g_\w+) = external[^\n]*align (\d+)$',ir,re.M):
  found=[l.split()for l in symbols.splitlines()if l.split()and l.split()[-1]==name]
  assert found and all(int(f[1],16)%int(align)==0 for f in found),name
finally:
 for i,p in enumerate(paths):shutil.copy2(backup/str(i),p)
for p in paths:assert hashlib.sha256(p.read_bytes()).hexdigest()==hashes[str(p.relative_to(root))],p
outmanifest=dict(candidate_sha256=hashlib.sha256(candidate.read_bytes()).hexdigest(),variant=variant,restored=hashes,selected=manifest['candidates'])
e=root/'docs/evidence'/build_name;e.mkdir(exist_ok=True)
(e/'build.json').write_text(json.dumps(outmanifest,indent=2)+'\n')
print('Built isolated '+variant+' integer candidate; all normal artifacts restored')
