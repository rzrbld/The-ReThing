#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
check=$(mktemp -d "$PWD/build/installer-ui-XXXXXX")
mkdir -p "$check/installer" "$check/tmp"
cat > "$check/installer/install_game.py" <<'PY'
import os,sys,time
from pathlib import Path
root=Path(sys.argv[2]);attempt=root/'attempt'
n=int(attempt.read_text())+1 if attempt.exists() else 1
attempt.write_text(str(n))
(root/'tmp/install-status.txt').write_text('-1\nTEST RETRY AND CANCEL\n')
if n==1:sys.exit(1)
(root/'worker.pid').write_text(str(os.getpid()))
time.sleep(60)
PY
cat > "$check/events.c" <<'C'
#define _GNU_SOURCE
#include <SDL.h>
#include <dlfcn.h>
#include <string.h>
int SDL_PollEvent(SDL_Event *event){
 static int stage=0;
 int (*original)(SDL_Event*)=dlsym(RTLD_NEXT,"SDL_PollEvent");
 Uint32 now=SDL_GetTicks();
 if((stage==0&&now>1000)||(stage==1&&now>2500)){
  memset(event,0,sizeof *event);event->type=SDL_CONTROLLERBUTTONDOWN;
  event->cbutton.button=stage==0?SDL_CONTROLLER_BUTTON_A:SDL_CONTROLLER_BUTTON_B;
  stage++;return 1;
 }
 return original(event);
}
C
cc -shared -fPIC $(pkg-config --cflags sdl2) "$check/events.c" -ldl -o "$check/events.so"
THING_LAUNCHER_WINDOWED=1 LD_PRELOAD="$check/events.so" xvfb-run -a build/launcher/thething-launcher.host "$check"
python3 - "$check" <<'PY'
from pathlib import Path
import os,sys
root=Path(sys.argv[1]);assert (root/'attempt').read_text()=='2'
pid=int((root/'worker.pid').read_text())
try:os.kill(pid,0)
except ProcessLookupError:pass
else:raise AssertionError('Cancelled installer still running')
print('PASS native SDL installer: first error, controller A retries, controller B cancels and reaps worker')
PY
