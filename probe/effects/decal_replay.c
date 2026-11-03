/* Exact captured decal workload; isolates submission from NPC/weather updates.
 * Raw fixtures stay local: they contain user-owned game geometry/textures. */
#define _POSIX_C_SOURCE 200809L
#include "adjacent_batch.h"
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define W 640
#define H 480
typedef struct {D8State state;ThingBatchDraw draw;} Record;
typedef struct {uint32_t meta[4];size_t bytes;unsigned char *pixels;GLuint texture;} Texture;
static Texture textures[128];static unsigned texture_count;
static Record *records;static unsigned count,emits;
static D8Renderer renderer;
static void read_exact(FILE*f,void*p,size_t n){assert(fread(p,1,n,f)==n);}
static unsigned dim(unsigned x,unsigned l){x>>=l;return x?x:1;}
static GLuint texture(FILE*f){
 uint32_t meta[4];read_exact(f,meta,sizeof meta);if(!meta[0]){assert(!meta[1]&&!meta[2]&&!meta[3]);return 0;}
 assert(meta[1]&&meta[2]&&meta[1]<=2048&&meta[2]<=2048&&meta[3]&&meta[3]<=16);
 size_t bytes=0;for(unsigned l=0;l<meta[3];l++)bytes+=(size_t)dim(meta[1],l)*dim(meta[2],l)*4;
 unsigned char*p=malloc(bytes);assert(p);read_exact(f,p,bytes);
 for(unsigned i=0;i<texture_count;i++)if(textures[i].meta[0]==meta[0]){
  Texture*t=textures+i;assert(!memcmp(t->meta,meta,sizeof meta)&&t->bytes==bytes&&!memcmp(t->pixels,p,bytes));
  free(p);return t->texture;
 }
 assert(texture_count<128);Texture*t=textures+texture_count++;memcpy(t->meta,meta,sizeof meta);t->bytes=bytes;t->pixels=p;
 glGenTextures(1,&t->texture);glBindTexture(GL_TEXTURE_2D,t->texture);
 size_t at=0;for(unsigned l=0;l<meta[3];l++){
  unsigned w=dim(meta[1],l),h=dim(meta[2],l);glTexImage2D(GL_TEXTURE_2D,l,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,p+at);at+=(size_t)w*h*4;
 }
 return t->texture;
}
static void load(const char*path,Record*r){
 FILE*f=fopen(path,"rb");assert(f);uint32_t h[14];read_exact(f,h,sizeof h);
 assert(h[0]==0x31434454&&h[1]==1&&h[2]==sizeof(D8State)&&h[13]==2);
 assert(h[3]==4&&h[4]>0&&h[4]<=2&&h[5]&&h[10]==H&&h[11]&&(h[8]==2||h[8]==4));
 assert(h[11]<=(64u<<20)&&h[12]==h[4]*3*h[8]);
 read_exact(f,&r->state,sizeof r->state);
 assert(r->state.fvf==0x112&&r->state.rs[27]&&r->state.rs[19]==5&&r->state.rs[20]==6);
 void*v=malloc(h[11]),*ix=malloc(h[12]);assert(v&&ix);read_exact(f,v,h[11]);read_exact(f,ix,h[12]);
 r->draw=(ThingBatchDraw){.state=&r->state,.vertices=v,.indices=ix,.vertex_bytes=h[11],.index_bytes=h[12],
  .primitive=h[3],.count=h[4],.stride=h[5],.vcount=h[6],.base=h[7],.index_size=h[8],.vertex_identity=h[9],
  .target_identity=1,.target_height=h[10],.default_framebuffer=1};
 for(unsigned t=0;t<2;t++)r->draw.textures[t]=texture(f);
 assert(fgetc(f)==EOF);fclose(f);
}
static int emit(void*unused,const ThingBatchDraw*d){
 (void)unused;emits++;D8NativeBuffers native={.default_framebuffer=1};
 return d8_renderer_draw_native(&renderer,d->state,H,d->primitive,d->count,d->vertices,d->stride,d->vcount,d->indices,d->index_size,d->base,d->textures,&native);
}
static unsigned render(ThingBatch*b,unsigned char*pixels){
 emits=0;glDepthMask(1);glColorMask(1,1,1,1);glClearColor(.3f,.3f,.3f,1);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 d8_renderer_invalidate_raster(&renderer);d8_renderer_begin_frame(&renderer);
 for(unsigned i=0;i<count;i++)assert(!(b?thing_batch_add(b,&records[i].draw):emit(NULL,&records[i].draw)));
 if(b)assert(!thing_batch_flush(b));
 if(pixels){glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,pixels);assert(glGetError()==GL_NO_ERROR);}
 return emits;
}
static double cpu_ms(void){struct timespec t;assert(!clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t));return t.tv_sec*1000.+t.tv_nsec/1e6;}
int main(int argc,char**argv){
 assert(argc>1);count=argc-1;assert(count<=4096);records=calloc(count,sizeof *records);assert(records);
 setenv("THING_GPU_VERTEX","1",1);setenv("THING_DRAW_ERROR_CHECK_EVERY","16",1);
 assert(!SDL_Init(SDL_INIT_VIDEO));SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
 SDL_Window*w=SDL_CreateWindow("captured decal replay",0,0,W,H,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);
 SDL_GLContext context=SDL_GL_CreateContext(w);assert(context);
 GLuint fbo,color,depth;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,W,H,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
 glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,W,H);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
 assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
 for(unsigned i=0;i<count;i++)load(argv[i+1],records+i);
 assert(!d8_renderer_init(&renderer));ThingBatch*b=thing_batch_create(1024*1024,65536,emit,NULL);assert(b);
 unsigned char*a=malloc(W*H*4),*c=malloc(W*H*4);assert(a&&c);
 unsigned ordinary=render(NULL,a),batched=render(b,c),changed=0,covered=0;
 for(unsigned i=0;i<W*H*4;i++)changed+=a[i]!=c[i];
 for(unsigned i=0;i<W*H;i++)covered+=a[i*4]<70||a[i*4]>85||a[i*4+1]<70||a[i*4+1]>85||a[i*4+2]<70||a[i*4+2]>85;
 printf("records=%u textures=%u ordinary=%u batched=%u different_bytes=%u covered_pixels=%u\n",count,texture_count,ordinary,batched,changed,covered);
 assert(!changed&&covered>0&&batched<ordinary);
 printf("GL=%s renderer=%s\n",glGetString(GL_VERSION),glGetString(GL_RENDERER));
 unsigned order[]={0,1,1,0};for(unsigned pass=0;pass<4;pass++){
  ThingBatch*q=order[pass]?b:NULL;for(unsigned i=0;i<30;i++)render(q,NULL);glFinish();
  double begin=cpu_ms();uint64_t wall=SDL_GetPerformanceCounter();
  for(unsigned i=0;i<1000;i++)render(q,NULL);
  double cpu=cpu_ms()-begin;glFinish();double elapsed=(SDL_GetPerformanceCounter()-wall)*1000./SDL_GetPerformanceFrequency();
  printf("pass=%u batching=%u cpu_ms_per_frame=%.6f wall_ms_per_frame=%.6f\n",pass,order[pass],cpu/1000,elapsed/1000);fflush(stdout);
 }
 thing_batch_destroy(b);d8_renderer_close(&renderer);
 for(unsigned i=0;i<count;i++){free((void*)records[i].draw.vertices);free((void*)records[i].draw.indices);}free(records);free(a);free(c);
 for(unsigned i=0;i<texture_count;i++){glDeleteTextures(1,&textures[i].texture);free(textures[i].pixels);}
 SDL_GL_DeleteContext(context);SDL_DestroyWindow(w);SDL_Quit();return 0;
}
