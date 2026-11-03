#!/usr/bin/env python3
"""Build an isolated native whole-sound Vorbis candidate; restore production by hash."""
import hashlib,json,pathlib,shutil,subprocess,sys
p=pathlib.Path(__file__).resolve().parents[1];variant='native-vorbis-timing';b=p/'build'/variant;backup=b/'backup';backup.mkdir(parents=True,exist_ok=True)
paths=['build/translated-local-regs/thething/functions_0232.c','build/game/host/thething/functions_0232.o','build/game/host/thething/functions_0232.flags','build/game/host/thething/lifted.a','build/game/thething.host','build/game/thething.host.sha256']
shutil.copy2(p/'probe/audio/native_vorbis_hook.h',b/'hook.h')
for unit in ('0231','0249'):
 paths += [f'build/translated-local-regs/thething/functions_{unit}.c',f'build/game/host/thething/functions_{unit}.o',f'build/game/host/thething/functions_{unit}.flags']
shutil.copy2(p/'probe/audio/timing.h',b/'timing.h')
manifest={}
for i,n in enumerate(paths):
 src=p/n;shutil.copy2(src,backup/str(i));manifest[n]=hashlib.sha256(src.read_bytes()).hexdigest()
try:
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
 # Time whole-sound and ov_read calls. Streaming function may remain live
 # as a worker loop, so each read is timed rather than just that function.
 import re
 for unit,address,kind in [('0232','0079F920','whole'),('0249','007D20B0','read')]:
  source=p/f'build/translated-local-regs/thething/functions_{unit}.c'
  text=source.read_text().replace('#include "functions.h"',f'#include "functions.h"\n#include "../../{variant}/timing.h"',1)
  match=re.search(r'void recomp_thething_'+address+r'\(void\) \{.*?(?=\nvoid recomp_|\Z)',text,re.S)
  assert match
  body=match.group()
  data='MEM32(g_esp+4)' if kind=='read' else 'MEM32(g_esp+12)'
  length='MEM32(g_esp+12)' if kind=='read' else 'MEM32(g_esp+8)'
  if kind=='whole':
   text=text.replace('#include "../../'+variant+'/timing.h"','#include "../../'+variant+'/timing.h"\n__thread unsigned audio_probe_whole_depth;',1)
   match=re.search(r'void recomp_thething_'+address+r'\(void\) \{.*?(?=\nvoid recomp_|\Z)',text,re.S)
  guard='++audio_probe_whole_depth; int audio_active=1;' if kind=='whole' else 'int audio_active=!audio_probe_whole_depth;'
  body=body.replace('{','{\n    '+guard+'\n    uint32_t audio_ctx='+data+',audio_bytes='+length+';\n    AudioProbeTime audio_time=audio_active?audio_probe_begin():(AudioProbeTime){0};',1)
  marker='RECOMP_REGS_OUT(); return;'
  assert marker in body
  body=body.replace(marker,'if(audio_active)audio_probe_end("'+kind+'",audio_ctx,audio_bytes,audio_time); '+('--audio_probe_whole_depth; ' if kind=='whole' else '')+marker)
  text=text[:match.start()]+body+text[match.end():]
  source.write_text(text)
 with (b/'build.log').open('w') as log:subprocess.run(['python3','-u','scripts/build_game.py','--target','host','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4'],cwd=p,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(p/'build/game/thething.host',b/'diagnostic.host')
finally:
 for i,n in enumerate(paths):shutil.copy2(backup/str(i),p/n)
for n,h in manifest.items():assert hashlib.sha256((p/n).read_bytes()).hexdigest()==h,n
(p/'docs/evidence'/variant).mkdir(parents=True,exist_ok=True)
(p/'docs/evidence'/variant/'build-manifest.json').write_text(json.dumps({'restored_files':manifest,'diagnostic_sha256':hashlib.sha256((b/'diagnostic.host').read_bytes()).hexdigest()},indent=2)+'\n')
print('Native Vorbis probe built; normal source/object/archive/executable restored by hash')
