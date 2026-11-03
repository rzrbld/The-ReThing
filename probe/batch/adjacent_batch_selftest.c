/* Real GLES A/B: owned geometry, rebased indices and ordered flush boundaries. */
#include "adjacent_batch.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define W 640
#define H 480
#define N 12
struct Vertex {float p[3],n[3];uint32_t color;float uv[2];};
static D8Renderer renderer;
static D8State state;
static struct Vertex vertices[N*6];
static GLuint tex[2];
static unsigned emitted;
static ThingBatch *benchmark_queue;
static unsigned benchmark_group;
static int emit(void *opaque,const ThingBatchDraw *d){
 (void)opaque;D8NativeBuffers native={.default_framebuffer=d->default_framebuffer};emitted++;
 return d8_renderer_draw_native(&renderer,d->state,d->target_height,d->primitive,d->count,d->vertices,d->stride,d->vcount,d->indices,d->index_size,d->base,d->textures,&native);
}
static void setup(unsigned scene){
 d8_state_defaults(&state,W,H);state.fvf=0x152;state.rs[22]=1;state.rs[137]=scene==1||scene==11;state.rs[143]=1;
 state.rs[7]=1;state.rs[14]=1;state.rs[23]=4;state.rs[27]=0;
 if(scene==1||scene==11){uint32_t type=3;memcpy(&state.light[0][0],&type,4);state.light[0][1]=.7f;state.light[0][2]=.8f;state.light[0][3]=.9f;state.light[0][18]=1;state.light_on[0]=1;state.rs[139]=0xff202020;}
 if(scene==2){state.rs[28]=1;state.rs[140]=3;float a=.1f,b=1.0f;memcpy(&state.rs[36],&a,4);memcpy(&state.rs[37],&b,4);state.rs[34]=0xff102040;}
 if(scene==3){state.rs[15]=1;state.rs[25]=5;state.rs[24]=120;}
 for(unsigned i=0;i<N;i++){
  float x=-.9f+(i%4)*.45f,y=-.8f+(i/4)*.5f,z=.3f+(i%2)*.2f;
  for(unsigned j=0;j<6;j++){vertices[i*6+j]=(struct Vertex){{x,y,z},{0,0,-1},0xff40a0ff,{0,0}};}
  vertices[i*6+1].p[0]=x;vertices[i*6+1].p[1]=y;
  vertices[i*6+3].p[0]=x+.6f;vertices[i*6+3].p[1]=y;
  vertices[i*6+2].p[0]=x;vertices[i*6+2].p[1]=y+.6f;
  vertices[i*6+3].uv[0]=1;vertices[i*6+2].uv[1]=1;
 }
 glClearColor(.02f,.03f,.04f,1);glDepthMask(GL_TRUE);glColorMask(1,1,1,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);d8_renderer_invalidate_raster(&renderer);
}
static void external_texture(unsigned version){
 unsigned char pixels[16]={255,255,255,255,255,0,0,0,0,255,0,255,0,0,255,255};
 if(version)for(unsigned i=0;i<16;i+=4){pixels[i]=30;pixels[i+1]=240;pixels[i+2]=80;pixels[i+3]=255;}
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,tex[0]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,2,2,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
 d8_renderer_invalidate_bindings(&renderer);
}
static unsigned render(unsigned scene,int batching,unsigned char *pixels){
 setup(scene);external_texture(0);emitted=0;
 ThingBatch *b=batching?(benchmark_queue?benchmark_queue:thing_batch_create(scene==7?sizeof(struct Vertex)*8:1024*1024,65536,emit,NULL)):NULL;
 uint16_t ix16[3]={1,3,2};uint32_t ix32[3]={1,3,2};
 for(unsigned i=0;i<N;i++){
  if(benchmark_group)state.rs[60]=0xffffffffu-(i/benchmark_group);
  if(scene==4&&i==N/2){if(b)assert(!thing_batch_flush(b));external_texture(1);}
  if(scene==5&&i==N/2){if(b)assert(!thing_batch_flush(b));glClear(GL_DEPTH_BUFFER_BIT);d8_renderer_invalidate_raster(&renderer);}
  if(scene==6)state.rs[60]=i<6?0xffffffff:0xff60ffff;
  if(scene==6){state.tss[0][1]=4;state.tss[0][3]=3;}
  if(scene==9&&i==6)state.transform[256][12]=.13f;
  if(scene==10&&i==6)state.viewport[2]=W/2;
  if(scene==11&&i==6)state.light[0][1]=.2f;
  GLuint texture=scene==8&&i>=6?tex[1]:tex[0];
  ThingBatchDraw d={.state=&state,.vertices=vertices,.vertex_bytes=sizeof vertices,.indices=i%2?(void*)ix32:(void*)ix16,.index_bytes=i%2?12:6,.primitive=4,.count=1,.stride=sizeof vertices[0],.vcount=N*6-i*6,.base=i*6,.index_size=i%2?4:2,.vertex_identity=1,.target_identity=1,.target_height=H,.textures={texture,0}};
  assert(!(b?thing_batch_add(b,&d):emit(NULL,&d)));
  /* Source memory and index arrays may change immediately after the call. */
  vertices[i*6+1].p[0]=99;vertices[i*6+2].color=0;ix16[0]=5;ix32[0]=5;
  ix16[0]=ix32[0]=1;
 }
 if(b){assert(!thing_batch_flush(b));if(b!=benchmark_queue)thing_batch_destroy(b);}
 if(pixels)glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,pixels);assert(glGetError()==GL_NO_ERROR);return emitted;
}
static int fail_emit(void *p,const ThingBatchDraw*d){(void)p;(void)d;return -1;}
static void validation(void){
 setup(0);uint32_t ix[]={1,3,2};ThingBatchDraw d={.state=&state,.vertices=vertices,.vertex_bytes=sizeof vertices,.indices=ix,.index_bytes=12,.index_size=4,.primitive=4,.count=1,.stride=sizeof vertices[0],.vcount=N*6,.vertex_identity=1,.target_identity=1,.target_height=H,.textures={tex[0],0}};
 ThingBatch*b=thing_batch_create(4096,12,emit,NULL);assert(b);assert(!thing_batch_add(b,&d));unsigned before=emitted;
 ix[0]=UINT32_MAX;assert(thing_batch_add(b,&d)==-1&&emitted==before+1);ix[0]=1;
 d.index_bytes=4;assert(thing_batch_add(b,&d)==-1);d.index_bytes=12;
 d.vertex_bytes=4;assert(thing_batch_add(b,&d)==-1);d.vertex_bytes=sizeof vertices;
 d.base=UINT32_MAX;assert(thing_batch_add(b,&d)==-1);d.base=0;
 d.count=UINT32_MAX;assert(thing_batch_add(b,&d)==-1);d.count=1;
 state.rs[27]=1;assert(thing_batch_add(b,&d)==1);state.rs[27]=0;
 d.primitive=5;assert(thing_batch_add(b,&d)==1);d.primitive=4;
 thing_batch_destroy(b);b=thing_batch_create(1,3,emit,NULL);assert(thing_batch_add(b,&d)==1);thing_batch_destroy(b);
 b=thing_batch_create(4096,12,fail_emit,NULL);assert(!thing_batch_add(b,&d));assert(thing_batch_flush(b)==-1);assert(!thing_batch_flush(b));thing_batch_destroy(b);
}
static void benchmark(void){
 /* Persistent queue; allocation is excluded. End-to-end wall time includes
  * queue copies, submission and GPU completion, without readback per frame.
  * Tiny synthetic triangles are a call-overhead probe, NOT game FPS. */
 benchmark_queue=thing_batch_create(1024*1024,65536,emit,NULL);assert(benchmark_queue);
 printf("benchmark GL=%s renderer=%s resolution=%dx%d draws_per_frame=%d\n",glGetString(GL_VERSION),glGetString(GL_RENDERER),W,H,N);
 unsigned groups[]={1,2,4,12};unsigned order[]={0,1,1,0};
 for(unsigned g=0;g<4;g++){
  benchmark_group=groups[g];
  for(unsigned pass=0;pass<4;pass++){
   int batching=order[pass];for(unsigned warm=0;warm<20;warm++)render(1,batching,NULL);glFinish();
   uint64_t start=SDL_GetPerformanceCounter();unsigned draws=0;
   for(unsigned frame=0;frame<4000;frame++)draws+=render(1,batching,NULL);
   glFinish();double ms=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency()/4000;
   printf("benchmark group=%u pass=%u batching=%d ms_per_frame=%.6f emitted_per_frame=%.1f\n",benchmark_group,pass,batching,ms,draws/4000.0);fflush(stdout);
  }
 }
 thing_batch_destroy(benchmark_queue);benchmark_queue=NULL;benchmark_group=0;
}
int main(int argc,char **argv){
 setenv("THING_GPU_VERTEX","1",1);setenv("THING_DRAW_ERROR_CHECK_EVERY","16",1);
 assert(!SDL_Init(SDL_INIT_VIDEO));SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
 SDL_Window*w=SDL_CreateWindow("batch differential",0,0,W,H,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);SDL_GLContext c=SDL_GL_CreateContext(w);assert(c);
 GLuint fbo,color,depth;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,W,H,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
 glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,W,H);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
 glGenTextures(2,tex);for(unsigned i=0;i<2;i++){glBindTexture(GL_TEXTURE_2D,tex[i]);unsigned char rgba[16]={255,100,20,255,255,100,20,255,255,100,20,255,255,100,20,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);}
 assert(!d8_renderer_init(&renderer));unsigned char*a=malloc(W*H*4),*b=malloc(W*H*4);assert(a&&b);
 for(unsigned scene=0;scene<12;scene++){
  unsigned ordinary=render(scene,0,a),batched=render(scene,1,b);unsigned changed=0,covered=0;for(unsigned i=0;i<W*H*4;i++)changed+=a[i]!=b[i];for(unsigned i=0;i<W*H;i++)covered+=a[i*4]>30;
  printf("scene=%u ordinary=%u batched=%u different_bytes=%u covered_pixels=%u\n",scene,ordinary,batched,changed,covered);
  assert(!changed&&covered>1000&&batched<ordinary);
 }
 validation();puts("PASS: 12 exact640x480 GLES comparisons; source/index mutation,16/32bit base rebasing, lighting,fog,alpha-test,depth/texture barriers,state changes,capacity,invalid bounds and emit failures");
 if(argc==2&&!strcmp(argv[1],"--benchmark"))benchmark();
 d8_renderer_close(&renderer);free(a);free(b);SDL_GL_DeleteContext(c);SDL_DestroyWindow(w);SDL_Quit();return 0;
}
