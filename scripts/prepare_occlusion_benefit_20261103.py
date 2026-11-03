#!/usr/bin/env python3
"""Stage isolated draw-suppression experiment, not a production visibility policy."""
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/occlusion-benefit-20261103';out.mkdir(exist_ok=True)
# Reuse completed streaming prototype, no regeneration or changes to its evidence.
s=(root/'build/streaming-shadow-20261103/identity.h').read_text()
s=s.replace('extern uint32_t ti_cell', 'extern int thing_occlusion_mode;\nextern uint32_t ti_cell',1)
s=s.replace('uint32_t ti_cell,ti_node,ti_target,ti_render;\nstatic struct', '''#include <stdlib.h>
int thing_occlusion_mode;
__attribute__((constructor)) static void occlusion_mode_init(void){const char *v=getenv("THING_OCCLUSION_BENEFIT");thing_occlusion_mode=v?atoi(v):0;}
uint32_t ti_cell,ti_node,ti_target,ti_render;
static struct''')
s=s.replace('void ti_tag(uint32_t key){','void ti_tag(uint32_t key){if(!thing_occlusion_mode)return;')
s=s.replace('void ti_capture_bounds(uint32_t key,uint32_t world){','void ti_capture_bounds(uint32_t key,uint32_t world){if(!thing_occlusion_mode)return;')
s=s.replace('void ti_present(void){','void ti_present(void){if(!thing_occlusion_mode)return;')
(out/'identity.h').write_text(s)
s=(root/'probe/visibility/streaming_shadow_renderer.h').read_text()
s=s.replace('/* Experimental observer only: NEVER suppresses a draw. */','/* Benefit screening only: preserves original packet/material calls; skips eligible hidden draw execution. */\nextern int thing_occlusion_mode;')
s=s.replace('static int shadow_enabled=-1;','')
s=s.replace('shadow_stream_reset();shadow_epoch++;shadow_draws=0;', 'if(!thing_occlusion_mode)return;shadow_stream_reset();shadow_epoch++;shadow_draws=0;')
s=s.replace('shadow_enabled>0','thing_occlusion_mode>0').replace('no_suppression=1','experimental_suppression=1')
s=s.replace('static void shadow_draw(', 'static int shadow_draw(')
s=s.replace(' if(shadow_enabled<0){const char *v=getenv("THING_STREAMING_SHADOW");shadow_enabled=v&&!strcmp(v,"1");}\n if(!shadow_enabled)return;', ' if(!thing_occlusion_mode)return 0;\n int hidden=0;')
s=s.replace(' if(depth_ok&&flag==0)mesh.bits|=4;', ' if(depth_ok&&flag==0&&primitive==4&&(state.fvf&14)==2&&!state.rs[152])mesh.bits|=4;')
s=s.replace('if(depth_ok&&state.rs[14]', 'if(depth_ok&&!state.rs[152]&&state.rs[14]')
s=s.replace('shadow_hidden+=shadow_stream_step(&mesh,scratch)!=0;', 'hidden=shadow_stream_step(&mesh,scratch)!=0;shadow_hidden+=hidden;')
s=s.replace('done:shadow_ticks+=SDL_GetPerformanceCounter()-begin;', 'done:shadow_ticks+=SDL_GetPerformanceCounter()-begin;return thing_occlusion_mode==1&&hidden;')
(out/'renderer.h').write_text(s)
s=(root/'scripts/build_streaming_shadow_20261103.py').read_text().replace('streaming-shadow-20261103','occlusion-benefit-20261103')
s=s.replace('../../../probe/visibility/streaming_shadow_renderer.h','../../occlusion-benefit-20261103/renderer.h')
s=s.replace('shadow_draw(primitive,count,vertices,stride,vcount,indices,isize,base,vb,voff,ib,ioff);', 'if(shadow_draw(primitive,count,vertices,stride,vcount,indices,isize,base,vb,voff,ib,ioff))return 0;')
(root/'scripts/build_occlusion_benefit_20261103.py').write_text(s)
print('Staged isolated prototype; no production files changed')
