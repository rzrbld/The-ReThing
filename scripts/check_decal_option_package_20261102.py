#!/usr/bin/env python3
import os,subprocess,tempfile,zipfile,tarfile,json,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(dir=root/'build',prefix='batch-ab-package-check-')as t:
 base=Path(t)
 with zipfile.ZipFile(root/'dist/the-thing-decal-option-arm-20261102.zip')as z:
  z.extractall(base);manifest=json.loads(z.read('ports/the-thing/decal-option-manifest.json'))
  for name,sha in manifest['files'].items():assert hashlib.sha256(z.read(name)).hexdigest()==sha
 g=base/'ports/the-thing';(g/'bin').mkdir();(g/'libthing-decal-audio.aarch64.so').write_bytes(b'fixture')
 (g/'the-thing-env.sh').write_text('''pm_platform_helper() {
 for name in profile.csv render.csv host-cpu-profile.csv; do echo fixture > "$THING_ROOT/$name"; done
}
''')
 (base/'fixture.c').write_text('''#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
int main(void){
 const char *keys[]={"THING_DECAL_BATCH","DS_DECAL_BATCH"};
 for(int i=0;i<2;i++)if(!getenv(keys[i])||strcmp(getenv(keys[i]),getenv("THING_EXPECT_BATCH")))return 99;
 if(strcmp(getenv("THING_DRAW_BATCH"),"0")||strcmp(getenv("DS_DRAW_BATCH"),"0"))return 96;
 if(strcmp(getenv("THING_RENDER_WIDTH"),"640")||strcmp(getenv("THING_GPU_BUFFER_CACHE"),"0")||strcmp(getenv("DS_RENDER_THREAD"),"0"))return 98;
 if(!getenv("THING_NATIVE_STREAM_PROBE")||strcmp(getenv("THING_NATIVE_STREAM_PROBE"),getenv("THING_NATIVE_VORBIS_PROBE")))return 97;
 puts("BATCH_SETTINGS_PASS");fflush(stdout);sleep(4);return 7;}
''')
 subprocess.run(['cc',str(base/'fixture.c'),'-o',str(g/'thething-decal-option.aarch64')],check=True)
 for mode,label in [(0,'OFF'),(1,'ON')]:
  env={k:v for k,v in os.environ.items()if not k.startswith(('THING_','DS_'))};env['THING_EXPECT_BATCH']=str(mode)
  r=subprocess.run(['bash',str(base/('Roms/PORTS' if mode==0 else 'ports')/f'The Thing Measure Decals {label}.sh')],env=env,capture_output=True,text=True,timeout=12)
  assert r.returncode==7,(r.returncode,r.stdout,r.stderr)
  archives=list((g/'measurements').glob(f'*decal-option-{mode}-*.tar.gz'));assert len(archives)==1
  with tarfile.open(archives[0])as tar:
   names=tar.getnames()
   def read(leaf):return tar.extractfile(next(n for n in names if n.endswith('/'+leaf))).read()
   assert read('exit.txt')==b'7\n';assert b'BATCH_SETTINGS_PASS' in read('log.txt')
   assert b'VmRSS' in read('device-telemetry.csv') and b'MemAvailable' in read('device-telemetry.csv')
   assert b'method=proc-stat-ppid' in read('device-telemetry.log')
   assert not any('/previous' in n or n.endswith('/RUNNING')for n in names)
 e=root/'docs/evidence/decal-option-package-20261102';e.mkdir(exist_ok=True)
 (e/'package-test.txt').write_text('PASS both actual packaged run/measurement wrappers: decal batching OFF/ON reaches executable with opaque OFF; muOS and PortMaster roots resolve, native audio and640/SINGLE/cacheOFF fixed, exit7 preserved, proc-stat sampler attaches, telemetry archived, old/RUNNING excluded, payload hashes verified. Local fixtures, not H700 gameplay evidence.\n')
 print((e/'package-test.txt').read_text())
