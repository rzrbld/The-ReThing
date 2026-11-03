#!/usr/bin/env python3
"""Stage opt-in GPU upload streaming; original renderer remains untouched."""
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/gpu-stream-20261102';out.mkdir(exist_ok=True)
s=(root/'runtime/d3d8_renderer.c').read_text();h=(root/'runtime/d3d8_renderer.h').read_text()
h=h.replace('size_t ring_used;', '''size_t ring_used;
    unsigned gpu_stream_enabled,gpu_stream_orphan[2];
    size_t gpu_stream_capacity[2],gpu_stream_used[2],gpu_stream_limit,gpu_stream_index_offset;''',1)
s=s.replace('void d8_renderer_begin_frame(D8Renderer *r){','''void d8_renderer_begin_frame(D8Renderer *r){
    r->gpu_stream_used[0]=r->gpu_stream_used[1]=0;
    r->gpu_stream_orphan[0]=r->gpu_stream_orphan[1]=1;''',1)
marker='    const char *shstat=setting("DS_SHADER_STATS");'
assert s.count(marker)==1
s=s.replace(marker,'''    {const char *stream=setting("DS_GPU_STREAM");r->gpu_stream_enabled=stream&&!strcmp(stream,"1");
     r->gpu_stream_limit=4u*1024u*1024u;
     const char *limit=setting("DS_GPU_STREAM_CAP");if(limit){unsigned long n=strtoul(limit,NULL,10);if(n>=256&&n<=r->gpu_stream_limit)r->gpu_stream_limit=n;}
     r->gpu_stream_orphan[0]=r->gpu_stream_orphan[1]=1;
     if(r->gpu_stream_enabled)fprintf(stderr,"[d3d8] experimental GPU streaming uploads, cap=%zu per buffer\\n",r->gpu_stream_limit);}
'''+marker,1)
marker='static int gpu_prepare('
assert s.count(marker)==1
helper='''/* Orphan at frame start and on capacity rollover. No upload overwrites a
 * range already used in this storage generation. GLES retains old storage
 * for queued draws. No persistent asset cache, mapping, or unsynchronized IO. */
static size_t gpu_stream_upload(D8Renderer *r,unsigned kind,size_t bytes,const void *data){
 GLenum target=kind?GL_ELEMENT_ARRAY_BUFFER:GL_ARRAY_BUFFER;
 if(!r->gpu_stream_enabled||bytes>r->gpu_stream_limit){
  glBufferData(target,bytes,data,GL_STREAM_DRAW);r->gl_stats.buffer_data++;
  r->gpu_stream_capacity[kind]=r->gpu_stream_used[kind]=0;r->gpu_stream_orphan[kind]=1;return 0;
 }
 size_t offset=(r->gpu_stream_used[kind]+15u)&~(size_t)15u;
 size_t capacity=r->gpu_stream_capacity[kind];
 if(r->gpu_stream_orphan[kind]||bytes>capacity||offset>capacity-bytes){
  if(capacity<65536u)capacity=65536u;
  if(capacity>r->gpu_stream_limit)capacity=r->gpu_stream_limit;
  while(capacity<bytes){capacity*=2;if(capacity>r->gpu_stream_limit)capacity=r->gpu_stream_limit;}
  glBufferData(target,capacity,NULL,GL_STREAM_DRAW);r->gl_stats.buffer_data++;
  r->gpu_stream_capacity[kind]=capacity;r->gpu_stream_orphan[kind]=0;offset=0;
 }
 glBufferSubData(target,offset,bytes,data);r->gl_stats.buffer_subdata++;
 r->gpu_stream_used[kind]=offset+bytes;return offset;
}
'''
s=s.replace(marker,helper+marker,1)
old='glBufferData(GL_ARRAY_BUFFER,vertex_bytes,(const char*)data+((size_t)base+first)*stride,GL_STREAM_DRAW);'
assert s.count(old)==1;s=s.replace(old,'attribute_base=gpu_stream_upload(r,0,vertex_bytes,(const char*)data+((size_t)base+first)*stride);',1)
old='glBufferData(GL_ELEMENT_ARRAY_BUFFER,(size_t)n*isize,uploaded_indices,GL_STREAM_DRAW);'
assert s.count(old)==1;s=s.replace(old,'r->gpu_stream_index_offset=gpu_stream_upload(r,1,(size_t)n*isize,uploaded_indices);',1)
old='native&&native->index?native->index_offset:0';assert s.count(old)==1
s=s.replace(old,'native&&native->index?native->index_offset:r->gpu_stream_index_offset',1)
(out/'d3d8_renderer.c').write_text(s);(out/'d3d8_renderer.h').write_text(h)
print('Staged GPU streaming candidate; default renderer unchanged')

# Reuse the established image corpus, adding repeated uploads and frame resets.
test=(root/'probe/d3d8_gpu_equivalence_selftest.c').read_text()
marker='    target_read(&g_target_gpu, img_gpu);'
assert test.count(marker)==1
test=test.replace(marker,'''    if(!c->native_gpu){
      uint32_t indices32[1024];unsigned count=d8_primitive_vertices(c->primitive,c->primitive_count);
      if(c->indices){if(count>1024)return -1;for(unsigned k=0;k<count;k++)indices32[k]=c->indices[k];}
      for(unsigned rep=0;rep<12;rep++){
        if(rep%4==0)d8_renderer_begin_frame(&g_gpu);
        gl_extra_reset();target_clear(&g_target_gpu);
        const void *ix=(rep&1)&&c->indices?(const void *)indices32:(const void *)c->indices;
        unsigned size=ix?((rep&1)?4:c->index_size):0;
        int rc=d8_renderer_draw_native(&g_gpu,c->state,HEIGHT,c->primitive,c->primitive_count,c->vertices,c->stride,c->vertex_count,ix,size,c->base,c->textures,NULL);
        if(rc||glGetError()!=GL_NO_ERROR){fprintf(stderr,"stream repeat failed %s rep=%u\\n",c->name,rep);return -1;}
      }
    }
'''+marker,1)
test=test.replace('    printf("overall=%s', '    printf("stream_calls data=%llu subdata=%llu\\n",(unsigned long long)g_gpu.gl_stats.buffer_data,(unsigned long long)g_gpu.gl_stats.buffer_subdata);\n    if(!g_gpu.gl_stats.buffer_subdata)overall=0;\n    printf("overall=%s',1)
test=test.replace('static int run_case(', (root/'probe/gpu_stream_lifetime.inc').read_text()+'\nstatic int run_case(',1)
test=test.replace('    compare_images(img_cpu, img_gpu, out);','    compare_images(img_cpu, img_gpu, out);\n    stream_lifetime_check(c,images_dir);',1)
(out/'selftest.c').write_text(test)
