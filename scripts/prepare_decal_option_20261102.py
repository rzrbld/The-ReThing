#!/usr/bin/env python3
"""Stage opt-in decals without replacing existing opaque batching defaults."""
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/decal-option-20261102';out.mkdir(exist_ok=True)
s=(root/'runtime/adjacent_batch.c').read_text()
s=s.replace(' D8State state;', ' int small_decals;\n D8State state;',1)
s=s.replace(' b->vertices=malloc(bytes);', ''' const char *mode=getenv("THING_DECAL_BATCH");if(!mode)mode=getenv("DS_DECAL_BATCH");
 b->small_decals=mode&&!strcmp(mode,"1");
 b->vertices=malloc(bytes);''',1)
old=' if(d->primitive!=4||!d->vertex_identity||(d->state->fvf&14)!=2||d->state->rs[27]||d->state->rs[52]||d->state->rs[151]||d->state->rs[167])return finish_with(b,1);'
shape='d->primitive!=4||!d->vertex_identity||d->state->fvf!=0x112||!d->state->rs[27]||d->state->rs[19]!=5||d->state->rs[20]!=6||d->state->rs[52]||d->state->rs[151]||d->state->rs[167]'
assert s.count(old)==1;s=s.replace(old,' if(b->small_decals){if('+shape+')return finish_with(b,1);}\n else '+old.lstrip())
old=' unsigned need=d8_fvf_stride(d->state->fvf);';assert s.count(old)==1
s=s.replace(old,' if(b->small_decals&&d->count>2)return finish_with(b,1);\n'+old)
(out/'adjacent_batch.c').write_text(s)
s=(root/'build/render-thread-src/runtime/d3d8.c').read_text()
old='static int batch_requested(void){';assert s.count(old)==1
s=s.replace(old,'''static int decal_batch_requested(void){
 static int enabled=-1;if(enabled<0){const char*v=setting("DS_DECAL_BATCH");enabled=v&&!strcmp(v,"1");}return enabled;
}
'''+old)
old='return enabled&&!(ds_rt_active&&ds_rt_active());';assert s.count(old)==1
s=s.replace(old,'return (enabled||decal_batch_requested())&&!(ds_rt_active&&ds_rt_active());')
old='if(batch_requested()&&!cache_buffers()';assert s.count(old)==1
s=s.replace(old,'if(batch_requested()&&(!decal_batch_requested()||(state.fvf==0x112&&state.rs[27]&&state.rs[19]==5&&state.rs[20]==6&&primitive==4&&count<=2&&!state.rs[52]&&!state.rs[151]&&!state.rs[167]))&&!cache_buffers()')
old='if(draw_batch)fprintf(stderr,"[draw-batch] experimental SINGLE/cache-OFF queue enabled\\n");';assert s.count(old)==1
s=s.replace(old,'if(draw_batch)fprintf(stderr,"[draw-batch] experimental SINGLE/cache-OFF queue enabled mode=%s\\n",decal_batch_requested()?"small-decals":"opaque");')
assert 'if(rc==1){batch_barrier();rc=d8_renderer_draw_native(' in s
(out/'d3d8.c').write_text(s)
print(out)
