#!/usr/bin/env python3
"""Stage narrow ordered batching for the measured two-triangle decal format."""
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/small-decal-batch-20261102';out.mkdir(exist_ok=True)
s=(root/'runtime/adjacent_batch.c').read_text()
old='||(d->state->fvf&14)!=2||d->state->rs[27]'
assert s.count(old)==1
s=s.replace(old,'||d->state->fvf!=0x112||!d->state->rs[27]||d->state->rs[19]!=5||d->state->rs[20]!=6')
needle=' unsigned need=d8_fvf_stride(d->state->fvf);';assert s.count(needle)==1
s=s.replace(needle,' if(d->count>2)return finish_with(b,1);\n'+needle)
(out/'adjacent_batch.c').write_text(s)
s=(root/'probe/batch/adjacent_batch_selftest.c').read_text()
s=s.replace('uint32_t color;','').replace('state.fvf=0x152','state.fvf=0x112').replace('state.rs[27]=0;','state.rs[27]=1;state.rs[19]=5;state.rs[20]=6;',1)
s=s.replace('0xff40a0ff,','').replace('vertices[i*6+2].color=0','vertices[i*6+2].n[0]=0')
s=s.replace('state.rs[27]=1;assert(thing_batch_add(b,&d)==1);state.rs[27]=0;','''state.rs[27]=0;assert(thing_batch_add(b,&d)==1);state.rs[27]=1;
 state.rs[19]=2;assert(thing_batch_add(b,&d)==1);state.rs[19]=5;
 state.rs[20]=1;assert(thing_batch_add(b,&d)==1);state.rs[20]=6;
 state.fvf=0x152;assert(thing_batch_add(b,&d)==1);state.fvf=0x112;
 d.count=3;assert(thing_batch_add(b,&d)==1);d.count=1;''')
s=s.replace('if(scene==6)state.rs[60]=i<6?0xffffffff:0xff60ffff;', 'if(scene==6){state.material[3]=i<6?.7f:.3f;state.rs[137]=1;state.rs[139]=0xffffffff;state.rs[60]=i<6?0xffffffff:0xff60ffff;}')
s=s.replace('state.rs[7]=1;state.rs[14]=1;', 'state.material[3]=.65f;state.rs[7]=1;state.rs[14]=1;')
s=s.replace('PASS: 12 exact640x480','PASS: 12 small-decal exact640x480')
s=s.replace('static unsigned emitted;', 'static unsigned emitted;\nstatic unsigned triangles=1;')
s=s.replace('uint16_t ix16[3]={1,3,2};uint32_t ix32[3]={1,3,2};', 'uint16_t ix16[6]={1,3,2,3,4,2};uint32_t ix32[6]={1,3,2,3,4,2};')
s=s.replace('.index_bytes=i%2?12:6,.primitive=4,.count=1,', '.index_bytes=(i%2?12:6)*triangles,.primitive=4,.count=triangles,')
s=s.replace('  vertices[i*6+3].uv[0]=1;', '  vertices[i*6+4].p[0]=x+.6f;vertices[i*6+4].p[1]=y+.6f;vertices[i*6+4].uv[0]=vertices[i*6+4].uv[1]=1;\n  vertices[i*6+3].uv[0]=1;')
s=s.replace(' for(unsigned scene=0;scene<12;scene++){', ' for(triangles=1;triangles<=2;triangles++)for(unsigned scene=0;scene<12;scene++){')
s=s.replace(' validation();puts(', ' triangles=1;validation();puts(').replace('PASS: 12 small-decal', 'PASS: 24 small-decal')
(out/'selftest.c').write_text(s)
print(out)
