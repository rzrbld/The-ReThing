#!/usr/bin/env python3
"""Verify explicit gcov flush survives _exit and matching profiles are consumed."""
import hashlib,json,os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/pgo-workflow-20261103';profiles=out/'profiles';profiles.mkdir(parents=True,exist_ok=True)
e=root/'docs/evidence/pgo-workflow-20261103';e.mkdir(exist_ok=True)
(out/'guest.c').write_text('''#include <unistd.h>
extern void SDL_GL_SwapWindow(void *);
volatile unsigned value;
int main(void){for(unsigned i=0;i<3;i++){value+=i?7:3;SDL_GL_SwapWindow(0);} _exit(value==17?0:1);}
''')
(out/'swap.c').write_text('void SDL_GL_SwapWindow(void *p){(void)p;}\n')
logs=[]
def run(cmd,**kw):
 r=subprocess.run(cmd,cwd=root,capture_output=True,text=True,**kw);logs.append('$ '+' '.join(map(str,cmd))+'\n'+r.stdout+r.stderr);r.check_returncode();return r
run(['gcc','-O2','-c',str(out/'swap.c'),'-o',str(out/'swap.o')])
run(['gcc','-O2','-c',str(root/'probe/pgo/flush.c'),'-o',str(out/'flush.o')])
run(['gcc','-O2','-fprofile-generate='+str(profiles),'-fprofile-update=atomic','-c',str(out/'guest.c'),'-o',str(out/'guest.o')])
run(['gcc',str(out/'guest.o'),str(out/'swap.o'),str(out/'flush.o'),'-fprofile-generate='+str(profiles),'-Wl,--wrap=SDL_GL_SwapWindow','-o',str(out/'training')])
r=run([str(out/'training')],env=dict(os.environ,THING_PGO_DUMP_AT_FRAME='2'));assert '[pgo] training counters dumped at swap=2' in r.stderr
files=list(profiles.rglob('*.gcda'));assert files and all(f.stat().st_size for f in files)
run(['gcc','-O2','-fprofile-use='+str(profiles),'-fprofile-partial-training','-Werror=coverage-mismatch','-Werror=missing-profile','-c',str(out/'guest.c'),'-o',str(out/'guest.o')])
run(['gcc',str(out/'guest.o'),str(out/'swap.o'),'-o',str(out/'optimized')]);run([str(out/'optimized')])
# Argument guards fail before touching the standard build tree.
for args in [['--pgo=generate','--profile-dir',str(profiles)],['--pgo=use','--profile-dir',str(out/'missing'),'--output-root',str(out/'isolated')]]:
 r=subprocess.run(['python3','scripts/build_game.py',*args],cwd=root,capture_output=True,text=True);assert r.returncode==2;logs.append(r.stderr)
(e/'check.log').write_text('\n'.join(logs));(e/'result.json').write_text(json.dumps(dict(exit=0,scope='Host compiler workflow fixture; NOT game performance or ARM profile compatibility',profiles=[str(f.relative_to(root))for f in files],optimized_sha256=hashlib.sha256((out/'optimized').read_bytes()).hexdigest()),indent=2)+'\n');print('PASS explicit flush + _exit, profile-use with strict coverage checks, and isolated-output guards')
