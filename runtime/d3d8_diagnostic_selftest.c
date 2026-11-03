/* Trace helpers only; linker discards all GL/guest facade entry points. */
#include "d3d8.c"
#include <assert.h>
ptrdiff_t g_mem_base;
uint32_t g_cur_func;
static int test_heap_fail=1,test_heap_calls;
void *hle_alloc(uint32_t bytes){++test_heap_calls;assert(bytes<=16);return test_heap_fail?NULL:hle_ptr(32);}
uint32_t hle_va_bad(const void *pointer){(void)pointer;abort();}
static void test_allocation_diagnostics(void){
 FILE *capture=tmpfile();assert(capture);int saved=dup(STDERR_FILENO);assert(saved>=0);assert(dup2(fileno(capture),STDERR_FILENO)>=0);
 Object o={.kind=TEXTURE,.width=1,.height=1,.format=21,.levels=1,.usage=17,.pool=2};g_cur_func=0x78ed00;
 resource_bytes=TEX_BUDGET;assert(allocate_pixels(&o)==-1&&test_heap_calls==0);
 resource_bytes=0;assert(allocate_pixels(&o)==-1&&test_heap_calls==1);
 test_heap_fail=0;resource_bytes=TEX_BUDGET-4;assert(allocate_pixels(&o)==-1&&resource_bytes==TEX_BUDGET&&test_heap_calls==2);
 resource_bytes=0;o.format=0;assert(allocate_pixels(&o)==-1&&test_heap_calls==2);
 o.format=21;unsigned errors=texture_allocation_errors;assert(!allocate_pixels(&o)&&resource_bytes==8&&texture_allocation_errors==errors);
 for(unsigned i=0;i<40;i++)texture_allocation_failure("test-limit",1,1,21,1,17,2,0,4);
 assert(texture_allocation_errors==32);fflush(stderr);assert(dup2(saved,STDERR_FILENO)>=0);close(saved);
 rewind(capture);char log[16000];size_t n=fread(log,1,sizeof(log)-1,capture);log[n]=0;fclose(capture);
 assert(strstr(log,"cpu-resource-budget")&&strstr(log,"guest-heap")&&strstr(log,"gpu-resource-budget")&&strstr(log,"zero-mip-bytes"));
 assert(strstr(log,"size=1x1 format=21 levels=1 usage=0x11 pool=2")&&strstr(log,"guest=0x78ed00")&&strstr(log,"further failures suppressed"));
 unsigned lines=0;for(char *p=log;(p=strstr(p,"reason="));++p)++lines;assert(lines==32);
 resource_bytes=0;
}
int main(void){
 uint8_t memory[64]={0};g_mem_base=(ptrdiff_t)memory;
 const uint8_t bgra[8]={3,2,1,0,6,5,4,128};memcpy(memory+4,bgra,8);
 setenv("THING_ROOT","build/d3d8-diagnostic-check",1);
 Object obj={.self=0x1234,.kind=TEXTURE,.width=2,.height=1,.format=21,.levels=1};obj.memory[0]=4;obj.pitch[0]=8;
 diagnostic_frame=fopen("build/d3d8-diagnostic-check/trace.txt","w");assert(diagnostic_frame);
 diagnostic_texture(&obj);diagnostic_texture(&obj);assert(diagnostic_texture_count==1);
 state.fvf=0x1c2;float words[7]={427,408,0,0,0,.25f,.625f};uint32_t white=0xffffffff;memcpy(words+3,&white,4);
 diagnostic_vertex((const unsigned char*)words,sizeof words,0);
 obj.self=0x5678;obj.gpu_dirty=1;diagnostic_texture(&obj);assert(diagnostic_texture_count==2);
 fclose(diagnostic_frame);diagnostic_frame=NULL;
 FILE*f=fopen("build/d3d8-diagnostic-check/render-texture-1-00001234.rgba","rb");assert(f);uint8_t got[9];assert(fread(got,1,9,f)==8);fclose(f);
 const uint8_t expected[8]={1,2,3,0,4,5,6,128};assert(!memcmp(got,expected,8));
 f=fopen("build/d3d8-diagnostic-check/render-texture-1-00005678.rgba","rb");assert(!f);
 test_allocation_diagnostics();
 puts("PASS: bounded allocation failure reasons/budget/heap/success; atlas RGBA/alpha, duplicate suppression, stale backing rejection, decoded UV trace");return 0;
}
