#!/usr/bin/env python3
"""Check clock separation and interposition without launching game/GLES."""
import csv,os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/frame-cpu-lines-20261102';out.mkdir(exist_ok=True)
subprocess.run(['gcc','-O2','-Wall','-Wextra','-fPIC','-shared',str(root/'probe/frame_cpu/frame_cpu_line_preload.c'),'-ldl','-pthread','-o',str(out/'frame-cpu.host.so')],check=True)
(out/'fake.c').write_text('void SDL_GL_SwapWindow(void *p){(void)p;}\n')
(out/'check.c').write_text('''#include <stdint.h>
#include <time.h>
#include <unistd.h>
extern void SDL_GL_SwapWindow(void*);
static uint64_t cpu(void){struct timespec t;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;}
int main(int argc,char **argv){(void)argv;SDL_GL_SwapWindow(0);for(int i=0;i<3;i++){uint64_t start=cpu();while(cpu()-start<3000000){}usleep(30000);SDL_GL_SwapWindow(0);}if(argc>1)_exit(0);return 0;}
''')
subprocess.run(['gcc','-shared','-fPIC',str(out/'fake.c'),'-o',str(out/'libfake.so')],check=True)
subprocess.run(['gcc',str(out/'check.c'),'-L'+str(out),'-lfake','-Wl,-rpath,'+str(out),'-o',str(out/'check')],check=True)
subprocess.run([str(out/'check')],env=dict(os.environ,THING_ROOT=str(out),LD_PRELOAD=str(out/'frame-cpu.host.so')),check=True)
rows=list(csv.DictReader((out/'frame-cpu-exact.csv').open()));assert len(rows)==4
for i,r in enumerate(rows[1:],1):
 cpu_ms=float(r['thread_cpu_ms']);wall_ms=(float(r['elapsed_seconds'])-float(rows[i-1]['elapsed_seconds']))*1000
 assert 2.5<=cpu_ms<15,(cpu_ms,wall_ms)
 assert wall_ms>=30 and cpu_ms<wall_ms/2,(cpu_ms,wall_ms)
print('PASS: 4 swaps intercepted, 3ms CPU work separated from 30ms sleeps; no game binary changes')

subprocess.run([str(out/'check'),'abrupt-exit'],env=dict(os.environ,THING_ROOT=str(out),LD_PRELOAD=str(out/'frame-cpu.host.so')),check=True)
text=(out/'frame-cpu-exact.csv').read_text();assert text.endswith('\n')
rows=list(csv.DictReader(text.splitlines()));assert len(rows)==4 and all(all(v is not None for v in row.values())for row in rows)
print('PASS: all 4 complete rows survive _exit without atexit/destructors')
