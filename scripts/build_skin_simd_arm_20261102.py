#!/usr/bin/env python3
"""Build requested ARM inline-skin SIMD variant; restore normal artifacts by hash."""
import hashlib,json,os,shutil,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];b=root/'build/skin-simd-arm-20261102';backup=b/'backup';backup.mkdir(parents=True,exist_ok=True)
evidence=root/'docs/evidence/skin-simd-arm-20261102';evidence.mkdir(parents=True,exist_ok=True)
paths=['build/translated-local-regs/thething/functions_0235.c','build/game/aarch64/thething/functions_0235.o','build/game/aarch64/thething/functions_0235.flags','build/game/aarch64/thething/lifted.a','build/game/thething.aarch64','build/game/thething.aarch64.sha256','build/game/aarch64/elf-header.txt','build/game/aarch64/dynamic.txt','build/game/aarch64/link.log']
manifest={}
for i,name in enumerate(paths):
 src=root/name;shutil.copy2(src,backup/str(i));manifest[name]=hashlib.sha256(src.read_bytes()).hexdigest()
assert manifest['build/game/thething.aarch64']=='fd314856d177c4ad616d74fee89a6978557c7a122ee99d91d6d486103fa19bba'
try:
 src=root/paths[0];text=src.read_text();text=text.replace('#include "functions.h"','#include "functions.h"\n#include "../../skin-simd-inline/inline.h"',1)
 patches=json.loads((root/'build/skin-simd-inline/patches.json').read_text())
 for i,patch in enumerate(patches):
  old=patch['original'];assert text.count(old)==14,'Expected14 copies per skin block'
  label=f'L_SKIN_SIMD_DONE_{i}'
  text=text.replace(old,'    if(thing_skin_simd_enabled()&&'+patch['condition']+') {\n'+patch['body']+'\n        goto '+label+';\n    }\n'+old+label+':;\n')
 src.write_text(text)
 command=['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp','--user',f'{os.getuid()}:{os.getgid()}','-v',f'{root}:/src:ro','-v',f'{root}/build/game:/src/build/game:rw','--workdir','/src','sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49','python3','-u','scripts/build_game.py','--target','aarch64','--allow-traps','--local-regs','--translated-opt=-O2','--render-thread','--jobs','4']
 with (b/'build.log').open('w') as log:subprocess.run(command,cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 shutil.copy2(root/'build/game/thething.aarch64',b/'thething-skin-simd.debug.aarch64')
finally:
 for i,name in enumerate(paths):shutil.copy2(backup/str(i),root/name)
for name,expected in manifest.items():assert hashlib.sha256((root/name).read_bytes()).hexdigest()==expected,name
result={'restored_files':manifest,'candidate_debug_sha256':hashlib.sha256((b/'thething-skin-simd.debug.aarch64').read_bytes()).hexdigest(),'patches_sha256':hashlib.sha256((root/'build/skin-simd-inline/patches.json').read_bytes()).hexdigest(),'header_sha256':hashlib.sha256((root/'build/skin-simd-inline/inline.h').read_bytes()).hexdigest()}
(evidence/'build-manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
