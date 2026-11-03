#!/usr/bin/env python3
"""Stage an isolated OFF/shadow/experimental-suppression build, default OFF."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
subprocess.run(['python3','scripts/prepare_streaming_shadow_20261103.py'],cwd=root,check=True)
out=root/'build/streaming-cull-20261103';out.mkdir(exist_ok=True)
for name in ['core.h','identity.h']:(out/name).write_bytes((root/'build/streaming-shadow-20261103'/name).read_bytes())
s=(root/'probe/visibility/streaming_shadow_renderer.h').read_text().replace('/* Experimental observer only: NEVER suppresses a draw. */','/* Isolated experiment: default OFF; mode1 shadow, mode2 suppress hidden draws. */')
s=s.replace('#include "../../build/streaming-shadow-20261103/core.h"','#include "core.h"').replace('static void shadow_draw(', 'static int shadow_draw(').replace('getenv("THING_STREAMING_SHADOW")','getenv("THING_STREAMING_CULL")').replace('shadow_enabled=v&&!strcmp(v,"1");','shadow_enabled=v&&!strcmp(v,"2")?2:v&&!strcmp(v,"1")?1:0;').replace('if(!shadow_enabled)return;','if(!shadow_enabled)return 0;').replace('uint64_t begin=SDL_GetPerformanceCounter();shadow_calls++;','int hidden=0;uint64_t begin=SDL_GetPerformanceCounter();shadow_calls++;')
s=s.replace('if(depth_ok&&flag==0)mesh.bits|=4;', 'if(depth_ok&&flag==0&&(state.fvf&14)==2&&primitive==4&&!state.rs[152])mesh.bits|=4;')
s=s.replace('if(depth_ok&&state.rs[14]', 'if(depth_ok&&!state.rs[152]&&state.rs[14]')
s=s.replace('shadow_hidden+=shadow_stream_step(&mesh,scratch)!=0;', 'hidden=shadow_stream_step(&mesh,scratch)!=0;shadow_hidden+=hidden;')
s=s.replace('done:shadow_ticks+=SDL_GetPerformanceCounter()-begin;','done:shadow_ticks+=SDL_GetPerformanceCounter()-begin;return shadow_enabled==2&&hidden;')
s=s.replace('[streaming-shadow]', '[streaming-cull]').replace('no_suppression=1\\n','mode=%d\\n').replace('shadow_ticks*1000.0/SDL_GetPerformanceFrequency());','shadow_ticks*1000.0/SDL_GetPerformanceFrequency(),shadow_enabled);')
(out/'renderer.h').write_text(s)
