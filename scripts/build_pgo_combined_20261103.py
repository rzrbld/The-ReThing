#!/usr/bin/env python3
"""Build isolated combined local-x87-top and native-audio candidate; restore production by hash."""
import hashlib,json,pathlib,shutil,subprocess,sys,os
p=pathlib.Path(__file__).resolve().parents[1]
import argparse
parser=argparse.ArgumentParser()
parser.add_argument('--target',choices=['host','aarch64'],default='host')
parser.add_argument('--phase',choices=['baseline','generate','use'],default='generate')
a=parser.parse_args();target=a.target;phase=a.phase
variant='pgo-combined-'+target+'-20261103';b=p/'build'/variant;backup=b/'backup';backup.mkdir(parents=True,exist_ok=True)
paths=['build/translated-local-regs/thething/functions_0232.c','build/game/host/thething/functions_0232.o','build/game/host/thething/functions_0232.flags','build/game/host/thething/lifted.a','build/game/thething.host','build/game/thething.host.sha256']
shutil.copy2(p/'probe/audio/native_vorbis_hook.h',b/'hook.h')
for unit in ('0248','0249','0215','0229'):
 paths += [f'build/translated-local-regs/thething/functions_{unit}.c',f'build/game/host/thething/functions_{unit}.o',f'build/game/host/thething/functions_{unit}.flags']
patches=json.loads((p/'build/fp-local-top-20261102/patches.json').read_text());units={}
for path in (p/'build/translated-local-regs/thething').glob('functions_*.c'):
 text=path.read_text();selected=[x for x in patches if x['original'] in text]
 if selected:units[path]=selected
assert sum(map(len,units.values()))==len(patches)==83
# Native decode hooks and floating-point substitutions must not target the
# same function. Source units can overlap without overlapping function bodies.
audio_addresses={'0079F900','007D0A20','007D0B20','007D1400','007D1B40','007D2070','007D20B0'}
assert not audio_addresses.intersection(x['address']for x in patches)
for path in units:paths += [str(path.relative_to(p)),f'build/game/host/thething/{path.stem}.o',f'build/game/host/thething/{path.stem}.flags']
paths=[n.replace('build/game/host/','build/game/'+target+'/').replace('thething.host','thething.'+target)for n in paths]
paths += [str(x.relative_to(p))for x in sorted((p/'build/game'/target).iterdir())if x.is_file()]
paths += ['build/render-thread-src/runtime/d3d8.c']
# Isolated output tree: only staged source inputs need restoration.
paths=list(dict.fromkeys(n for n in paths if n.endswith('.c')))
profiles=b/'profiles';game=b/'game';game.mkdir(exist_ok=True)
e=p/'docs/evidence'/variant;e.mkdir(exist_ok=True)
if phase=='generate' and profiles.exists() and list(profiles.rglob('*.gcda')):
 raise SystemExit('Training data already exists; preserve it and choose an explicit new experiment.')
def target_command(command):
 if target=='host':return command
 return ['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{p}:{p}:ro','-v',f'{b}:{b}:rw','--workdir',str(p),'sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49',*command]
compiler=subprocess.check_output(target_command(['aarch64-linux-gnu-gcc' if target=='aarch64' else 'cc','--version']),text=True)
normal={str(q.relative_to(p)):hashlib.sha256(q.read_bytes()).hexdigest()for q in (p/'build/game/thething.host',p/'build/game/thething.aarch64')}

manifest={}
for i,n in enumerate(paths):
 src=p/n;shutil.copy2(src,backup/str(i));manifest[n]=hashlib.sha256(src.read_bytes()).hexdigest()
try:
 for path,selected in units.items():
  text=path.read_text()
  for x in selected:
   assert text.count(x['original'])==1;text=text.replace(x['original'],x['candidate'],1)
  text=text.replace('#include "functions.h"','#include "functions.h"\n#include "../../../probe/fp_local_top/local_top.h"',1)
  path.write_text(text)
 src=p/paths[0];s=src.read_text();s=s.replace('#include "functions.h"',f'#include "functions.h"\n#include "../../{variant}/hook.h"',1)
 marker='    edi = MEM32(esp + 0x2FC); /* 0x0079F93E: mov edi, dword ptr [esp + 0x2fc] */'
 assert s.count(marker)==1
 patch="""    { uint32_t obj=MEM32(esp+0x304);
      RECOMP_FLAGS_OUT(); RECOMP_REGS_OUT();
      if(native_vorbis_whole(MEM32(esp+0x2FC),MEM32(esp+0x300),obj)) {
        esi=obj; ebx=1; esp-=4; goto L_NATIVE_VORBIS_TAIL;
      }
    }
"""
 s=s.replace(marker,patch+marker)
 tail='    eax = MEM32(esi + 0x4); /* 0x0079FAB0: mov eax, dword ptr [esi + 4] */'
 assert s.count(tail)==1
 s=s.replace(tail,'L_NATIVE_VORBIS_TAIL:;\n'+tail)
 src.write_text(s)
 for unit,addresses in [('0248',('007D0A20','007D0B20','007D1400')),('0249',('007D1B40','007D2070','007D20B0'))]:
  source=p/f'build/translated-local-regs/thething/functions_{unit}.c';text=source.read_text()
  header='guest_stream_impl.h' if unit=='0248' else 'guest_stream.h'
  text=text.replace('#include "functions.h"','#include "functions.h"\n#include "../../../probe/audio/'+header+'"',1)
  for address in addresses:
   marker='void recomp_thething_'+address+'(void) {'
   assert text.count(marker)==1
   text=text.replace(marker,marker+'\n    if(thing_native_stream_dispatch(0x'+address+'u))return;',1)
  source.write_text(text)
 subprocess.run(['python3','scripts/stage_early_queue_bounds_20261103.py'],cwd=p,check=True)
 inputs=set()
 for directory in ['build/translated-local-regs','runtime','probe','third_party/pcrecomp/runtime','build/render-thread-src/runtime']:
  for suffix in ('*.c','*.h'):inputs.update((p/directory).rglob(suffix))
 inputs.update((p/'build/game').glob('*.c'))
 source_hashes={str(q.relative_to(p)):hashlib.sha256(q.read_bytes()).hexdigest()for q in sorted(inputs)}
 if phase=='use':
  trained=json.loads((e/'generate-manifest.json').read_text())
  assert trained['source_hashes']==source_hashes,'Sources changed since training'
  assert trained['compiler']==compiler,'Compiler changed since training'
  assert list(profiles.rglob('*.gcda')),'No training profile'
 command=['python3','-u','scripts/build_game.py','--target',target,'--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4','--output-root',str(game)]
 if phase!='baseline':command+=['--pgo',phase,'--profile-dir',str(profiles)]
 with (b/(phase+'-build.log')).open('w') as log:subprocess.run(target_command(command),cwd=p,stdout=log,stderr=subprocess.STDOUT,check=True)
 artifact=b/(dict(baseline='baseline',generate='training',use='diagnostic')[phase]+'.'+target)
 shutil.copy2(game/('thething.'+target),artifact)

finally:
 for i,n in enumerate(paths):shutil.copy2(backup/str(i),p/n)
for n,h in manifest.items():assert hashlib.sha256((p/n).read_bytes()).hexdigest()==h,n
for n,h in normal.items():assert hashlib.sha256((p/n).read_bytes()).hexdigest()==h,n
profile_hashes={str(q.relative_to(b)):hashlib.sha256(q.read_bytes()).hexdigest()for q in sorted(profiles.rglob('*.gcda'))} if profiles.exists() else {}
(e/(phase+'-manifest.json')).write_text(json.dumps(dict(phase=phase,target=target,compiler=compiler,composition=['83localTOP','nativeaudio','earlybounds defaultOFF','no matrix shortcuts or skin inlining'],source_hashes=source_hashes,restored_files=manifest,normal_binaries=normal,profile_hashes=profile_hashes,artifact=str(artifact.relative_to(p)),artifact_sha256=hashlib.sha256(artifact.read_bytes()).hexdigest()),indent=2)+'\n')
print('PGO phase '+phase+' completed; staged sources restored and normal binaries unchanged')
shutil.rmtree(backup)
