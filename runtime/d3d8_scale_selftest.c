/* Real GLES regression: 2D draws, backbuffer rebinding, and one final upscale. */
#include "d3d8.c"
#include <assert.h>
ptrdiff_t g_mem_base;
uint32_t g_cur_func;
void hle_fatal(const char *fmt,...){(void)fmt;abort();}
void *hle_alloc(uint32_t size){(void)size;abort();}
uint32_t hle_va_bad(const void *p){(void)p;abort();}
typedef struct {float x,y,z,rhw;uint32_t color;} TestVertex;
static void rect(float x,float y,float w,float h,uint32_t color){
 TestVertex v[]={{x,y,0,1,color},{x+w,y,0,1,color},{x,y+h,0,1,color},{x,y+h,0,1,color},{x+w,y,0,1,color},{x+w,y+h,0,1,color}};
 assert(!draw(4,2,v,sizeof v[0],6,NULL,0,0,NULL,0,NULL,0));
}
static void pixel(int x,int y,int green){
 unsigned char c[4];glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,c);
 fprintf(stderr,"pixel %d,%d: %u,%u,%u expected %s\n",x,y,c[0],c[1],c[2],green?"green":"red");
 assert(c[green?1:0]>240&&c[green?0:1]<10&&c[2]<10);
}
int main(void){
 static unsigned char memory[128+640*480*4]={0};g_mem_base=(ptrdiff_t)memory;
 setenv("THING_DRAW_BATCH","1",1);
 setenv("THING_RENDER_WIDTH","512",1);setenv("THING_RENDER_HEIGHT","384",1);
 setenv("THING_GPU_VERTEX","1",1);setenv("THING_BRIGHTNESS","1.0",1);
 assert(!SDL_Init(SDL_INIT_VIDEO));SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
 window=SDL_CreateWindow("scale-test",0,0,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);assert(window);context=SDL_GL_CreateContext(window);assert(context);
 assert(!d8_renderer_init(&renderer));assert(!create_scale_target(640,480));
 uint32_t index=1;memcpy(memory+24,&index,4);objects[1]=(Object){.self=16,.kind=SURFACE,.backbuffer=1,.width=640,.height=480,.format=21,.levels=1};objects[1].memory[0]=128;objects[1].pitch[0]=640*4;render_target=16;
 d8_state_defaults(&state,640,480);state.fvf=0x44;state.rs[7]=0;state.rs[22]=1;state.rs[137]=0;state.tss[0][1]=2;state.tss[0][2]=0;state.tss[0][4]=2;state.tss[0][5]=0;
 rect(0,0,640,480,0xffff0000);
 GLint bound;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&bound);assert((GLuint)bound==scale_fbo);
 assert(!bind_target(16,0));rect(512,384,128,96,0xff00ff00);
 assert(!sync_pixels(&objects[1]));
 assert(memory[128+2]==255&&memory[128+1]==0);
 assert(memory[128+(450*640+600)*4+1]==255&&memory[128+(450*640+600)*4+2]==0);
 scale_blit_now();pixel(10,10,0);pixel(639,479,0);pixel(600,30,1);pixel(639,0,1);
 SDL_SetWindowSize(window,800,600);SDL_PumpEvents();SDL_GL_SwapWindow(window);scale_blit_now();pixel(799,599,0);pixel(799,0,1);
 GLuint off;glGenFramebuffers(1,&off);index=2;memcpy(memory+40,&index,4);objects[2]=(Object){.self=32,.kind=SURFACE,.fbo=off,.width=128,.height=128};render_target=32;bind_game_target();glGetIntegerv(GL_FRAMEBUFFER_BINDING,&bound);assert((GLuint)bound==off&&renderer.output_scale==1.0f);
 render_target=16;bind_game_target();assert(renderer.output_scale==scale_factor);assert(glGetError()==GL_NO_ERROR);
 puts("PASS: 512x384 framebuffer retained across UI/rebind, upscaled once to full 640x480 and resized800x600; offscreen targets unscaled; guest readback includes current scaled frame");
 /* Exercise real facade queuing and readback/target barriers, not only the
  * standalone queue. UP draws above still take the ordinary fallback path. */
 typedef struct {float x,y,z;uint32_t color;} BatchVertex;
 BatchVertex triangle[]={{-1,-1,.5f,0xff00ff00},{1,-1,.5f,0xff00ff00},{0,1,.5f,0xff00ff00}};
 uint16_t indices[]={0,1,2};Object vb={.self=48,.length=sizeof triangle},ib={.self=64,.length=sizeof indices};
 state.fvf=0x42;
 assert(!draw(4,1,triangle,sizeof triangle[0],3,indices,2,0,&vb,0,&ib,0));
 assert(!draw(4,1,triangle,sizeof triangle[0],3,indices,2,0,&vb,0,&ib,0));
 assert(draw_batch&&batch_inputs==2&&batch_emits==0);
 memset(triangle,0,sizeof triangle); /* queued vertices must already be owned */
 assert(!sync_pixels(&objects[1]));assert(batch_emits==1);
 assert(memory[128+(240*640+320)*4+1]==255);
 triangle[0]=(BatchVertex){-1,-1,.5f,0xffff0000};triangle[1]=(BatchVertex){1,-1,.5f,0xffff0000};triangle[2]=(BatchVertex){0,1,.5f,0xffff0000};
 assert(!draw(4,1,triangle,sizeof triangle[0],3,indices,2,0,&vb,0,&ib,0));
 assert(batch_emits==1);assert(!bind_target(16,0));assert(batch_emits==2);
 thing_batch_destroy(draw_batch);draw_batch=NULL;
 puts("PASS: facade merges two owned draws; save readback and target binding flush pending geometry");
 SDL_Quit();return 0;
}
