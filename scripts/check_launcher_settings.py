#!/usr/bin/env python3
"""Check persisted settings and their effective exports through the real launcher."""
import os,pathlib,subprocess,tempfile
p=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='thing-launcher-') as tmp:
 r=pathlib.Path(tmp);(r/'config').mkdir();capture=r/'capture.sh'
 capture.write_text('''#!/bin/bash
export gamedir="$THING_ROOT" controlfolder="$THING_ROOT/no-portmaster"
source "$THING_ENV_TEST"
printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\\n' "$THING_FPS_LIMIT" "$THING_FPS_OVERLAY" "$THING_BRIGHTNESS" "${THING_RENDER_WIDTH:-native}" "${THING_RENDER_HEIGHT:-native}" "$DS_RENDER_THREAD" "$THING_GPU_BUFFER_CACHE" "$DS_RT_ASYNC_SWAP" "$DS_ONE_UPLOAD" "$DS_RANGE_DRAW" "$DS_ALPHA_ELIDE" "$DS_INVALIDATE_DEPTH"
''')
 env={k:v for k,v in os.environ.items() if not k.startswith(('THING_','DS_'))}
 env.update(THING_ROOT=str(r),THING_ENV_TEST=str(p/'packaging/the-thing-env.sh'),THING_LAUNCHER_SKIP_MENU='1')
 for resolution,w,h in [('native','native','native'),('640x480','640','480'),('512x384','512','384')]:
  for thread in [0,1]:
   for cache in [0,1]:
    (r/'config/launcher.cfg').write_text(f'fps_limit=30\nfps_counter=1\nbrightness=150\nresolution={resolution}\nrender_thread={thread}\nbuffer_cache={cache}\n')
    got=subprocess.check_output([str(p/'build/launcher/thething-launcher.host'),str(r),str(capture)],env=env,text=True).strip()
    assert got==f'30,1,,{w},{h},{thread},{cache},1,0,1,1,1',got
 # Direct Ports entry must also respect saved settings, and explicit measurement overrides.
 env.pop('THING_LAUNCHER_SKIP_MENU');env.update(THING_BRIGHTNESS='1.5',THING_GPU_BUFFER_CACHE='0',DS_RENDER_THREAD='0',THING_RENDER_WIDTH='640',THING_RENDER_HEIGHT='480')
 got=subprocess.check_output(['bash',str(capture)],env=env,text=True).strip()
 assert got=='30,1,,640,480,0,0,,,,,',got
 env['THING_LAUNCHER_SKIP_MENU']='1'
 # Render-option rows: explicit values must reach the game and persist in the canonical file.
 (r/'config/launcher.cfg').write_text('fps_limit=0\nfps_counter=0\nresolution=native\nrender_thread=0\n'
     'buffer_cache=0\nshadow_quality=0\nasync_present=0\none_upload=0\nranged_draws=1\n'
     'alpha_elide=0\ninvalidate_depth=0\n')
 got=subprocess.check_output([str(p/'build/launcher/thething-launcher.host'),str(r),str(capture)],env=env,text=True).strip()
 assert got=='0,0,,native,native,0,0,0,0,1,0,0',got
 got=subprocess.check_output(['bash',str(capture)],env=env,text=True).strip()
 assert got=='0,0,,640,480,0,0,0,0,1,0,0',got
 env.pop('THING_LAUNCHER_SKIP_MENU',None)
 env['THING_LAUNCHER_SELFTEST']='1'
 subprocess.check_call([str(p/'build/launcher/thething-launcher.host'),str(r),str(capture)],env=env)
 cfg=(r/'config/launcher.cfg').read_text()
 assert 'brightness=' not in cfg
 for key in ('async_present=','one_upload=','ranged_draws=','alpha_elide=','invalidate_depth='):assert key in cfg,cfg
print('PASS: 12 launcher configuration/export combinations, render-option rows and persistence, direct launcher settings and diagnostic overrides')
