#define main unused_main
#include THING_TEST_RENDERER_SOURCE
#undef main
#include <assert.h>
static unsigned calls;
static size_t size;
static unsigned char uploaded[128];
void glBufferData(GLenum target,GLsizeiptr bytes,const void *data,GLenum usage){
 assert(target==GL_ELEMENT_ARRAY_BUFFER && usage==GL_STREAM_DRAW);
 assert(bytes>=0 && bytes<=128);++calls;size=bytes;
 if(bytes)memcpy(uploaded,data,bytes);
}
int main(void){
 D8Renderer r={0}; unsigned char data[128]={0};r.tiny_index_enabled=1;
 tiny_index_upload(&r,12,data);assert(calls==1);
 tiny_index_upload(&r,12,data);assert(calls==1 && r.tiny_index_hits==1);
 data[11]=1;tiny_index_upload(&r,12,data);assert(calls==2 && uploaded[11]==1);
 tiny_index_upload(&r,24,data);assert(calls==3 && size==24);
 tiny_index_upload(&r,64,data);assert(calls==4 && !r.tiny_index_bytes);
 tiny_index_upload(&r,24,data);assert(calls==5);
 r.tiny_index_enabled=0;tiny_index_upload(&r,24,data);assert(calls==6);
 r.tiny_index_enabled=1;tiny_index_upload(&r,24,data);assert(calls==7);
 tiny_index_upload(&r,0,NULL);assert(calls==8 && !r.tiny_index_bytes);
 tiny_index_upload(&r,24,data);assert(calls==9);
 tiny_index_upload(&r,24,data);assert(calls==9);
 D8Renderer other={0};other.tiny_index_enabled=1;
 tiny_index_upload(&other,24,data);assert(calls==10);
 puts("PASS: exact-byte reuse, mutation, size, large/disabled/empty invalidation, per-renderer isolation");
}
