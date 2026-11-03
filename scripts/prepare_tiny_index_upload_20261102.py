#!/usr/bin/env python3
"""Isolated exact-last-upload experiment. Does not edit normal runtime."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/tiny-index-upload-20261102'
out.mkdir(parents=True,exist_ok=True)
h=(ROOT/'runtime/d3d8_renderer.h').read_text()
h=h.replace('GLuint program, vbo, gpu_program, gpu_vbo, gpu_ibo;',
 '''GLuint program, vbo, gpu_program, gpu_vbo, gpu_ibo;
 unsigned tiny_index_enabled, tiny_index_bytes;
 unsigned char tiny_index_data[32];
 uint64_t tiny_index_hits;''')
s=(ROOT/'runtime/d3d8_renderer.c').read_text()
anchor='static int gpu_prepare('
helper='''/* Only gpu_ibo storage is mirrored, never guest pointers or whole assets.
 * Every larger/disabled upload invalidates the mirror. Native index buffers
 * are separate objects and do not change this object's storage. */
static void tiny_index_upload(D8Renderer *r,size_t bytes,const void *data){
 if(r->tiny_index_enabled && bytes && bytes<=sizeof r->tiny_index_data){
  if(r->tiny_index_bytes==bytes && !memcmp(r->tiny_index_data,data,bytes)){
   ++r->tiny_index_hits;return;
  }
  memcpy(r->tiny_index_data,data,bytes);r->tiny_index_bytes=bytes;
 }else r->tiny_index_bytes=0;
 glBufferData(GL_ELEMENT_ARRAY_BUFFER,bytes,data,GL_STREAM_DRAW);
 d8_renderer_note_buffer_upload(r,1,bytes);
}
'''
assert s.count(anchor)==1
s=s.replace(anchor,helper+anchor)
old='glBufferData(GL_ELEMENT_ARRAY_BUFFER,(size_t)n*isize,uploaded_indices,GL_STREAM_DRAW);d8_renderer_note_buffer_upload(r,1,(size_t)n*isize);'
assert s.count(old)==1
s=s.replace(old,'tiny_index_upload(r,(size_t)n*isize,uploaded_indices);')
(out/'d3d8_renderer.c').write_text(s)
(out/'d3d8_renderer.h').write_text(h)
(out/'adjacent_batch.h').write_bytes((ROOT/'runtime/adjacent_batch.h').read_bytes())
(out/'adjacent_batch.c').write_bytes((ROOT/'build/small-decal-batch-20261102/adjacent_batch.c').read_bytes())
# Replay the same 50 original unbatched draws in both modes.
r=(ROOT/'probe/effects/decal_replay.c').read_text()
r=r.replace('unsigned ordinary=render(NULL,a),batched=render(b,c),changed=0,covered=0;',
 'renderer.tiny_index_enabled=0; unsigned ordinary=render(NULL,a); renderer.tiny_index_enabled=1; unsigned batched=render(NULL,c),changed=0,covered=0;')
r=r.replace('batched<ordinary','batched==ordinary')
r=r.replace('ThingBatch*q=order[pass]?b:NULL;', 'renderer.tiny_index_enabled=order[pass]; ThingBatch*q=NULL;')
r=r.replace(' thing_batch_destroy(b);',' printf("tiny_index_hits=%llu\\n",(unsigned long long)renderer.tiny_index_hits);\n thing_batch_destroy(b);')
r=r.replace('unsigned order[]={0,1,1,0};for(unsigned pass=0;pass<4;pass++)',
 'unsigned order[]={0,1,1,0,1,0,0,1};for(unsigned pass=0;pass<8;pass++)')
r=r.replace('i<1000;i++','i<5000;i++').replace('cpu/1000,elapsed/1000','cpu/5000,elapsed/5000')
r=r.replace('batching=%u','tiny_index=%u')
(out/'replay.c').write_text(r)
print(out)
