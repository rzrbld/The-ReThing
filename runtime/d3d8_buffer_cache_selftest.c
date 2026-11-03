/* Exercise facade buffer uploads and real renderer binding bookkeeping. */
#include "d3d8.c"
#include <assert.h>
ptrdiff_t g_mem_base;
uint32_t g_cur_func;
static unsigned binds,allocations,updates,checks;
static unsigned char uploaded[64];
void GL_APIENTRY glGenBuffers(GLsizei n,GLuint *v){assert(n==1);*v=42;}
void GL_APIENTRY glBindBuffer(GLenum target,GLuint b){assert(target==GL_ARRAY_BUFFER||target==GL_ELEMENT_ARRAY_BUFFER);assert(b==42);binds++;}
void GL_APIENTRY glBufferData(GLenum target,GLsizeiptr n,const void *p,GLenum usage){(void)target;assert(n==64&&usage==GL_DYNAMIC_DRAW);memcpy(uploaded,p,n);allocations++;}
void GL_APIENTRY glBufferSubData(GLenum target,GLintptr offset,GLsizeiptr n,const void *p){(void)target;assert(offset==8&&n==4);memcpy(uploaded+offset,p,n);updates++;}
GLenum GL_APIENTRY glGetError(void){checks++;return GL_NO_ERROR;}
int main(void){
 unsigned char memory[128];for(unsigned i=0;i<sizeof memory;i++)memory[i]=(unsigned char)i;
 g_mem_base=(ptrdiff_t)memory;
 for(int index=0;index<2;index++){
  Object o={.length=64};o.memory[0]=16;
  assert(!upload_buffer(&o,index)&&o.buffer==42);
  assert(!memcmp(uploaded,memory+16,64));
  renderer.current_program=97;renderer.bound_tex[0]=98;renderer.bound_tex[1]=99;
  unsigned b=binds,c=checks;
  for(int i=0;i<100;i++)assert(!upload_buffer(&o,index));
  assert(binds==b&&checks==c);
  memory[24]^=0xff;o.buffer_dirty_start=8;o.buffer_dirty_end=12;
  assert(!upload_buffer(&o,index));
  assert(!memcmp(uploaded,memory+16,64)&&binds==b);
  assert(renderer.current_program==97&&renderer.bound_tex[0]==98&&renderer.bound_tex[1]==99);
  assert(!o.buffer_dirty_start&&!o.buffer_dirty_end);
 }
 assert(allocations==2&&updates==2&&binds==2);
 puts("PASS: clean vertex/index uploads issue no GL calls; dirty ranges preserve contents and unrelated bindings");
}
