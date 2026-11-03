#!/usr/bin/env python3
"""Build isolated combined local-x87-top and native-audio candidate; restore production by hash."""
import hashlib,json,pathlib,shutil,subprocess,sys,os
p=pathlib.Path(__file__).resolve().parents[1]
target='aarch64' if '--target=aarch64' in sys.argv else 'host'
variant='vector-scalar-'+target+'-20261103';b=p/'build'/variant;backup=b/'backup';backup.mkdir(parents=True,exist_ok=True)
paths=['build/translated-local-regs/thething/functions_0232.c','build/game/host/thething/functions_0232.o','build/game/host/thething/functions_0232.flags','build/game/host/thething/lifted.a','build/game/thething.host','build/game/thething.host.sha256']
shutil.copy2(p/'probe/audio/native_vorbis_hook.h',b/'hook.h')
for unit in ('0248','0249','0005'):
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
paths=list(dict.fromkeys(paths))
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
 subprocess.run(['python3','scripts/stage_vector_scalar_game_20261103.py'],cwd=p,check=True)
 command=['python3','-u','scripts/build_game.py','--target',target,'--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4']
 if target=='aarch64':command=['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{p}:/src:ro','-v',f'{p}/build/game:/src/build/game:rw','--workdir','/src','sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49',*command]
 with (b/'build.log').open('w') as log:subprocess.run(command,cwd=p,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(p/'build/game'/('thething.'+target),b/('diagnostic.'+target))

finally:
 for i,n in enumerate(paths):shutil.copy2(backup/str(i),p/n)
for n,h in manifest.items():assert hashlib.sha256((p/n).read_bytes()).hexdigest()==h,n
(p/'docs/evidence'/variant).mkdir(parents=True,exist_ok=True)
(p/'docs/evidence'/variant/'build-manifest.json').write_text(json.dumps({'composition':['83 local x87 top routines','native whole-sound Vorbis','native streaming Vorbis','guarded scalar vector add and scale'],'fp_patches_sha256':hashlib.sha256((p/'build/fp-local-top-20261102/patches.json').read_bytes()).hexdigest(),'restored_files':manifest,'diagnostic_sha256':hashlib.sha256((b/('diagnostic.'+target)).read_bytes()).hexdigest()},indent=2)+'\n')
print('Combined CPU/audio candidate built; normal source/object/archive/executable restored by hash')

shutil.rmtree(backup) # Successful verified restoration: do not retain redundant rollback binaries.
