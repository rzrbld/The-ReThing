#!/usr/bin/env python3
"""Build isolated ARM native audio variant; restore normal artifacts by hash."""
import hashlib,json,os,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];b=root/'build/native-audio-arm-20261102';backup=b/'backup';backup.mkdir(parents=True,exist_ok=True)
evidence=root/'docs/evidence/native-audio-arm-20261102';evidence.mkdir(parents=True,exist_ok=True)
paths=['build/translated-local-regs/thething/functions_0232.c','build/game/aarch64/thething/functions_0232.o','build/game/aarch64/thething/functions_0232.flags','build/game/aarch64/thething/lifted.a','build/game/thething.aarch64','build/game/thething.aarch64.sha256','build/game/aarch64/elf-header.txt','build/game/aarch64/dynamic.txt','build/game/aarch64/link.log']
shutil.copy2(root/'probe/audio/native_vorbis_hook.h',b/'hook.h')
for unit in ('0248','0249'):
 paths += [f'build/translated-local-regs/thething/functions_{unit}.c',f'build/game/aarch64/thething/functions_{unit}.o',f'build/game/aarch64/thething/functions_{unit}.flags']
manifest={}
for i,name in enumerate(paths):
 src=root/name;shutil.copy2(src,backup/str(i));manifest[name]=hashlib.sha256(src.read_bytes()).hexdigest()
assert manifest['build/game/thething.aarch64']=='fd314856d177c4ad616d74fee89a6978557c7a122ee99d91d6d486103fa19bba'
try:
 src=root/paths[0];s=src.read_text();s=s.replace('#include "functions.h"',f'#include "functions.h"\n#include "../../native-audio-arm-20261102/hook.h"',1)
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
  source=root/f'build/translated-local-regs/thething/functions_{unit}.c';text=source.read_text()
  header='guest_stream_impl.h' if unit=='0248' else 'guest_stream.h'
  text=text.replace('#include "functions.h"','#include "functions.h"\n#include "../../../probe/audio/'+header+'"',1)
  for address in addresses:
   marker='void recomp_thething_'+address+'(void) {'
   assert text.count(marker)==1
   text=text.replace(marker,marker+'\n    if(thing_native_stream_dispatch(0x'+address+'u))return;',1)
  source.write_text(text)
 command=['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{root}:/src:ro','-v',f'{root}/build/game:/src/build/game:rw','--workdir','/src','sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49','python3','-u','scripts/build_game.py','--target','aarch64','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4']
 with (b/'build.log').open('w') as log:subprocess.run(command,cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.aarch64',b/'thething-native-audio.debug.aarch64')
finally:
 for i,name in enumerate(paths):shutil.copy2(backup/str(i),root/name)
for name,expected in manifest.items():assert hashlib.sha256((root/name).read_bytes()).hexdigest()==expected,name
result={'restored_files':manifest,'candidate_debug_sha256':hashlib.sha256((b/'thething-native-audio.debug.aarch64').read_bytes()).hexdigest(),'header_sha256':hashlib.sha256((root/'probe/audio/native_vorbis_hook.h').read_bytes()).hexdigest(),'stream_bridge_sha256':hashlib.sha256((root/'probe/audio/guest_stream_impl.h').read_bytes()).hexdigest()}
(evidence/'build-manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
