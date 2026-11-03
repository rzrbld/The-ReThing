#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "win32hle.h"
uint32_t g_eax,g_edx,g_esp;ptrdiff_t g_mem_base;
static uint32_t bump=0x200000;static unsigned closes,frees;static unsigned char*compressed;static size_t compressed_size,position;
void *hle_alloc(uint32_t n){uint32_t a=bump;bump+=(n+15)&~15u;assert(bump<0x800000);memset(hle_ptr(a),0,n);return hle_ptr(a);}
void hle_free(void*p){assert(p);frees++;}
uint32_t hle_scratch_mark(void){return bump;}
uint32_t hle_scratch(uint32_t n){return hle_va(hle_alloc(n));}
void hle_scratch_release(uint32_t p){bump=p;}
uint32_t hle_va_bad(const void*p){(void)p;abort();}
uint32_t hle_call_guest(uint32_t addr,int count,const uint32_t*a){
 if(addr==0x79E420){assert(count==4&&a[1]==1&&a[3]==0x1234);size_t n=a[2];if(n>compressed_size-position)n=compressed_size-position;memcpy(hle_ptr(a[0]),compressed+position,n);position+=n;return n;}
 if(addr==0x79E490){assert(count==4&&a[0]==0x1234);int64_t offset=(int64_t)((uint64_t)a[1]|((uint64_t)a[2]<<32));int64_t base=a[3]==0?0:a[3]==1?(int64_t)position:(int64_t)compressed_size;if(offset< -base||offset>(int64_t)compressed_size-base)return -1;position=base+offset;return 0;}
 if(addr==0x79E470){assert(count==1&&a[0]==0x1234);return position;}
 if(addr==0x79E450){assert(count==1&&a[0]==0x1234);closes++;return 0;}
 abort();
}
#include "guest_stream_impl.h"
static uint32_t call(uint32_t va,int n,const uint32_t*a){g_esp=0x1000;MEM32(g_esp)=0xdead0000;for(int i=0;i<n;i++)MEM32(g_esp+4+4*i)=a[i];assert(thing_native_stream_dispatch(va));assert(g_esp==0x1004);return g_eax;}
int main(int argc,char**argv){assert(argc==3);assert(!setenv("THING_NATIVE_STREAM_PROBE",argv[1],1));unsigned char*mem=calloc(1,0x800000);assert(mem);g_mem_base=(ptrdiff_t)mem;
 FILE*f=fopen(argv[2],"rb");assert(f);assert(!fseek(f,0,SEEK_END));compressed_size=ftell(f);rewind(f);compressed=malloc(compressed_size);assert(compressed);assert(fread(compressed,1,compressed_size,f)==compressed_size);fclose(f);
 for(int repeat=0;repeat<3;repeat++){
  position=0;memset(hle_ptr(0x10000),0x90,0x2D0);uint32_t open[]={0x1234,0x10000,0,0,0x79E420,0x79E490,0x79E450,0x79E470};assert(call(0x7D0B20,8,open)==0);
  uint32_t info[]={0x10000,0xffffffff};uint32_t v=call(0x7D2070,2,info);assert(v&&MEM32(v+4)==2&&MEM32(v+8)==22050);
  uint32_t total=call(0x7D1400,2,info);assert(g_edx==0&&total>0);
  uint32_t read[]={0x10000,0x20000,4096,0,2,1,0x18000};size_t bytes=0;
  for(;;){int32_t n=call(0x7D20B0,7,read);assert(n>=0&&n<=4096);assert(MEM32(0x18000)==0);if(!n)break;bytes+=n;}
  assert(bytes==total*4);
  uint32_t seek[]={0x10000,0,0};assert(!call(0x7D1B40,3,seek));assert((int32_t)call(0x7D20B0,7,read)>0);
  uint32_t clear=0x10000;assert(!call(0x7D0A20,1,&clear));assert(closes==(unsigned)repeat+1&&frees==(unsigned)repeat+1);for(unsigned j=0;j<0x2D0;j++)assert(MEM8(0x10000+j)==0);
  g_esp=0x1000;MEM32(g_esp+4)=clear;assert(!thing_native_stream_dispatch(0x7D20B0));assert(g_esp==0x1000);
 }
 free(compressed);free(mem);puts("PASS guest stream ABI: cdecl stack, callbacks, metadata, 64-bit total, PCM size, seek, close-once, slot reuse, and inactive fallback");return 0;
}
