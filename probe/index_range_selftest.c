#define _GNU_SOURCE
#include "d3d8_index_range.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
static uint32_t rng=1234567;
static uint32_t next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void reference(const void *p,unsigned n,unsigned size,unsigned *lo,unsigned *hi){
 *lo=UINT32_MAX;*hi=0;
 for(unsigned i=0;i<n;i++){unsigned v=0;memcpy(&v,(const char*)p+(size_t)i*size,size);if(v<*lo)*lo=v;if(v>*hi)*hi=v;}
}
static double clock_s(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec*1e-9;}
int main(void){
 unsigned char storage[4096*4+16];unsigned cases=0;
 for(unsigned size=2;size<=4;size+=2)for(unsigned alignment=0;alignment<16;alignment++)for(unsigned n=0;n<=4096;n++){
  for(unsigned j=0;j<n;j++){uint32_t v=next();if(n%3==0)v&=65535;memcpy(storage+alignment+(size_t)j*size,&v,size);}
  unsigned a,b,c,d;reference(storage+alignment,n,size,&a,&b);d8_index_range(storage+alignment,n,size,&c,&d);assert(a==c&&b==d);cases++;
 }
 size_t page=(size_t)sysconf(_SC_PAGESIZE);unsigned char *guard=mmap(NULL,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(guard!=MAP_FAILED);assert(!mprotect(guard+page,page,PROT_NONE));
 for(unsigned size=2;size<=4;size+=2)for(unsigned n=0;n<64;n++){
  unsigned char *p=guard+page-(size_t)n*size;memset(p,255,(size_t)n*size);unsigned a,b;d8_index_range(p,n,size,&a,&b);assert(a==(n?(size==2?65535:UINT32_MAX):UINT32_MAX));assert(b==(n?(size==2?65535:UINT32_MAX):0));cases++;
 }
 munmap(guard,page*2);printf("PASS %u range comparisons: both formats, all lengths 0..4096, 16 alignments, UINT32_MAX and protected tail\n",cases);
 for(unsigned i=0;i<sizeof storage;i++)storage[i]=(unsigned char)next();
 volatile unsigned checksum=0;
 for(unsigned mode=0;mode<4;mode++){double t=clock_s();for(unsigned i=0;i<100000;i++){unsigned a,b;if(mode==1||mode==2)d8_index_range(storage,3072,2,&a,&b);else reference(storage,3072,2,&a,&b);checksum+=a+b;}printf("%s %.3f ns/scan checksum=%u\n",mode==1||mode==2?"candidate":"reference",(clock_s()-t)*1e4,checksum);}
 return 0;
}
