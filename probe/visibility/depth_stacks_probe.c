/* Diagnostic only: replay opaque depth-tested draws without depth/color writes.
 * Extra draws and synchronous queries invalidate performance measurements. */
#define _GNU_SOURCE
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <execinfo.h>
#include <stdint.h>
static void (*real_elements)(GLenum,GLsizei,GLenum,const void*);
static void (*real_arrays)(GLenum,GLint,GLsizei);
static unsigned active,window,frame,ordinal;
static FILE *out;
static unsigned *node,*mesh,*lod,*lod_count;
__attribute__((constructor)) static void init(void){
 *(void**)&real_elements=dlsym(RTLD_NEXT,"glDrawElements");
 *(void**)&real_arrays=dlsym(RTLD_NEXT,"glDrawArrays");
}
int ds_gl_cost_begin(unsigned ignored){
 (void)ignored;node=dlsym(RTLD_DEFAULT,"thing_probe_node");mesh=dlsym(RTLD_DEFAULT,"thing_probe_mesh");lod=dlsym(RTLD_DEFAULT,"thing_probe_lod");lod_count=dlsym(RTLD_DEFAULT,"thing_probe_lod_count");int (*threaded)(void)=dlsym(RTLD_DEFAULT,"ds_rt_active");
 if(threaded&&threaded())return 0;
 if(!out){const char *p=getenv("THING_DEPTH_FILE");if(!p)return 0;out=fopen(p,"w");if(!out)return 0;
 fputs("window,frame,draw,indexed,count,program,depth,blend,depth_write,normal_pass,no_depth_pass,node,mesh,lod,lod_count\n",out);}
 active=1;window++;ordinal=0;fprintf(stderr,"[depth-probe] window=%u one frame, diagnostic replay\n",window);return 1;
}
void ds_gl_cost_present(void){frame++;if(active){active=0;fflush(out);}}
void ds_gl_cost_flush(void){if(out)fflush(out);}
static void draw(int indexed,GLenum mode,GLint first,GLsizei count,GLenum type,const void *indices){
 if(!active){if(indexed)real_elements(mode,count,type,indices);else real_arrays(mode,first,count);return;}
 int saved=errno;GLint program;GLboolean depth=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),mask;
 glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);
 GLuint q[2],pass=0,without=2;glGenQueries(2,q);glBeginQuery(GL_ANY_SAMPLES_PASSED,q[0]);
 if(indexed)real_elements(mode,count,type,indices);else real_arrays(mode,first,count);
 glEndQuery(GL_ANY_SAMPLES_PASSED);
 if(depth&&!blend&&mask){
  GLboolean color[4];GLint sf,sb;glGetBooleanv(GL_COLOR_WRITEMASK,color);
  glGetIntegerv(GL_STENCIL_WRITEMASK,&sf);glGetIntegerv(GL_STENCIL_BACK_WRITEMASK,&sb);
  glColorMask(0,0,0,0);glDepthMask(0);glStencilMaskSeparate(GL_FRONT,0);glStencilMaskSeparate(GL_BACK,0);glDisable(GL_DEPTH_TEST);
  glBeginQuery(GL_ANY_SAMPLES_PASSED,q[1]);
  if(indexed)real_elements(mode,count,type,indices);else real_arrays(mode,first,count);
  glEndQuery(GL_ANY_SAMPLES_PASSED);
  glEnable(GL_DEPTH_TEST);glDepthMask(mask);glColorMask(color[0],color[1],color[2],color[3]);
  glStencilMaskSeparate(GL_FRONT,(GLuint)sf);glStencilMaskSeparate(GL_BACK,(GLuint)sb);
  glGetQueryObjectuiv(q[1],GL_QUERY_RESULT,&without);
 }
 glGetQueryObjectuiv(q[0],GL_QUERY_RESULT,&pass);glDeleteQueries(2,q);
 if(pass==0&&without==1){
 static uint64_t seen[64];static unsigned n;static FILE *st;
 void *bt[48];int len=backtrace(bt,48);uint64_t hash=1469598103934665603ULL;
 for(int i=2;i<len;i++){hash^=(uintptr_t)bt[i];hash*=1099511628211ULL;}
 unsigned i;for(i=0;i<n;i++)if(seen[i]==hash)break;
 if(i==n&&n<64){seen[n++]=hash;const char *path=getenv("THING_DEPTH_STACKS");if(!st&&path)st=fopen(path,"w");
 if(st){fprintf(st,"window=%u draw=%u count=%d node=%u stack=%u\n",window,ordinal+1,count,node?*node:0,n);fflush(st);backtrace_symbols_fd(bt,len,fileno(st));}}
 }
 fprintf(out,"%u,%u,%u,%d,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",window,frame+1,++ordinal,indexed,count,program,depth,blend,mask,pass,without,node?*node:0,mesh?*mesh:0,lod?*lod:0,lod_count?*lod_count:0);errno=saved;
}
void glDrawElements(GLenum m,GLsizei c,GLenum t,const void *i){draw(1,m,0,c,t,i);}
void glDrawArrays(GLenum m,GLint f,GLsizei c){draw(0,m,f,c,0,NULL);}
