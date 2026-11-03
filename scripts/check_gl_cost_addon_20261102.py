#!/usr/bin/env python3
"""Exercise packaged launch/archive flow with a host fixture, not H700 gameplay."""
import hashlib,json,os,subprocess,tarfile,tempfile,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(dir=root/'build',prefix='glcost-package-')as t:
 base=Path(t)
 with zipfile.ZipFile(root/'dist/the-thing-gl-cost-addon-20261102.zip')as z:
  assert z.testzip() is None
  m=json.loads(z.read('ports/the-thing/gl-cost-addon-manifest.json'))
  for name,digest in m['files'].items():assert hashlib.sha256(z.read(name)).hexdigest()==digest
  assert not any(n.endswith('.exe') or '/data/' in n or n.endswith('.sav') or n.endswith('.aarch64') for n in z.namelist())
  z.extractall(base)
 g=base/'ports/the-thing';(g/'bin').mkdir()
 (g/'the-thing-env.sh').write_text('''pm_platform_helper() {
 for name in profile.csv render.csv host-cpu-profile.csv; do echo fixture > "$THING_ROOT/$name"; done
}
''')
 # Substitute host ABI library only in the temporary fixture; ARM ZIP stays exact.
 (g/'libthing-glcost.aarch64.so').write_bytes((root/'build/gl-cost-preload-20261102/libthing-glcost.host.so').read_bytes())
 (g/'libthing-decal-audio.aarch64.so').write_bytes(b'fixture')
 for name in ('sample_device.sh','sample_device_child.sh'):(g/name).write_bytes((root/'scripts'/name).read_bytes())
 trigger=g/'trigger-glcost.sh';s=trigger.read_text();assert 'for delay in 90 60' in s
 trigger.write_text(s.replace('for delay in 90 60','for delay in 1 1'))
 (base/'fakegl.c').write_text('unsigned glGetError(void) { return 0; }\n')
 subprocess.run(['cc','-shared','-fPIC',str(base/'fakegl.c'),'-o',str(g/'libfakegl.so')],check=True)
 (base/'game.c').write_text(r'''
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
extern unsigned glGetError(void);
int ds_rt_active(void) { return 0; }
int main(void) {
 assert(!strcmp(getenv("THING_RENDER_WIDTH"),"640"));
 assert(!strcmp(getenv("DS_RENDER_THREAD"),"0"));
 assert(!strcmp(getenv("THING_GPU_BUFFER_CACHE"),"0"));
 assert(!strcmp(getenv("THING_DECAL_BATCH"),"0"));
 assert(!strcmp(getenv("THING_DRAW_BATCH"),"0"));
 assert(!strcmp(getenv("THING_NATIVE_STREAM_PROBE"),getenv("THING_NATIVE_VORBIS_PROBE")));
 assert(strstr(getenv("LD_PRELOAD"),"libthing-glcost.aarch64.so"));
 int (*begin)(unsigned)=dlsym(RTLD_DEFAULT,"ds_gl_cost_begin");
 void (*present)(void)=dlsym(RTLD_DEFAULT,"ds_gl_cost_present");
 assert(begin && present);
 int needed=atoi(getenv("FIXTURE_WINDOWS")),received=0;
 for(int tick=0;tick<800;tick++) {
  FILE*f=fopen("render.control","r");
  if(f){char command[32]={0};assert(fgets(command,sizeof command,f));fclose(f);unlink("render.control");
   assert(!strcmp(command,"glcost\n"));assert(begin(120));
   for(int i=0;i<120;i++){assert(!glGetError());present();}
   if(++received==needed){puts("GLCOST_PACKAGE_PASS");return 7;}
  }
  usleep(10000);
 }
 return 99;
}
''')
 subprocess.run(['cc',str(base/'game.c'),'-rdynamic','-L'+str(g),'-Wl,-rpath,'+str(g),'-lfakegl','-ldl','-o',str(g/'thething-decal-option.aarch64')],check=True)
 env={k:v for k,v in os.environ.items()if not k.startswith(('THING_','DS_')) and k!='LD_PRELOAD'}
 prior=set()
 for layout,windows in [('Roms/PORTS',2),('ports',1)]:
  result=subprocess.run(['bash',str(base/layout/'The Thing Measure GL Cost.sh')],env={**env,'FIXTURE_WINDOWS':str(windows)},capture_output=True,text=True,timeout=15)
  assert result.returncode==7,(result.returncode,result.stdout,result.stderr)
  files=set((g/'measurements').glob('*glcost-*.tar.gz'));added=files-prior;assert len(added)==1;prior=files
  with tarfile.open(added.pop())as tar:
   names=tar.getnames()
   def read(leaf):return tar.extractfile(next(n for n in names if n.endswith('/'+leaf))).read()
   assert read('exit.txt')==b'7\n'
   assert b'GLCOST_PACKAGE_PASS' in read('log.txt')
   assert b'glGetError' in read('gl-cost.csv')
   assert b'VmRSS' in read('device-telemetry.csv')
   expected='COMPLETE.txt' if windows==2 else 'INCOMPLETE.txt'
   assert any(n.endswith('/'+expected)for n in names)
   assert not any('/previous/' in n or n.endswith('/RUNNING')for n in names)
   assert read('glcost-trigger.log').count(b'Requested GL cost window')==windows
 # Dependencies are checked before moving logs or creating a capture directory.
 (g/'thething-decal-option.aarch64').unlink()
 result=subprocess.run(['bash',str(base/'ports/The Thing Measure GL Cost.sh')],env=env,capture_output=True,text=True,timeout=5)
 assert result.returncode==1 and 'install the decal-option review build first' in result.stderr
 assert set((g/'measurements').glob('*glcost-*.tar.gz'))==prior
out=root/'docs/evidence/gl-cost-addon-20261102';out.mkdir(parents=True,exist_ok=True)
(out/'package-test.txt').write_text('PASS: both packaged launcher layouts; game-only preload, fixed640/SINGLE/cacheOFF/batchOFF and native-audio environment; actual profiler120-frame windows from atomic trigger; complete/incomplete gates; exit7 preservation; telemetry/archive content; previous files excluded; missing dependency fails before capture. Host executable+host profiler substitution in temporary fixture only; NOT H700 evidence.\n')
print((out/'package-test.txt').read_text())
