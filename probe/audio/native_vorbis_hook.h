/* Isolated host experiment; decoder shared library selected explicitly. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "win32hle.h"
static int native_vorbis_whole(uint32_t data,uint32_t length,uint32_t object){
 typedef int (*decode_fn)(const unsigned char*,size_t,unsigned char**,size_t*,long*,int*,long long*);
 typedef void (*free_fn)(void*);
 static int checked;static decode_fn decode;static free_fn release;
 if(!checked){checked=1;const char*p=getenv("THING_NATIVE_VORBIS_PROBE");if(p&&*p){void*h=dlopen(p,RTLD_NOW|RTLD_LOCAL);if(h){decode=(decode_fn)dlsym(h,"probe_decode");release=(free_fn)dlsym(h,"probe_free");}if(!decode||!release)decode=NULL;}}
 if(!decode||!data||!object||!length||length>16000000)return 0;
 unsigned char*pcm=NULL;size_t bytes=0;long rate=0;int channels=0;long long frames=0;
 if(decode((const unsigned char*)hle_ptr(data),length,&pcm,&bytes,&rate,&channels,&frames))return 0;
 if(!bytes||bytes>32000000||rate<1||rate>192000||channels<1||channels>2||bytes!=(size_t)frames*channels*2){release(pcm);return 0;}
 uint32_t n=(uint32_t)bytes,fmt_n=18;
 uint32_t buffer=hle_call_guest(0x007B30DEu,1,&n);
 if(!buffer){release(pcm);return 0;}
 uint32_t fmt=hle_call_guest(0x007B30DEu,1,&fmt_n);
 if(!fmt){hle_call_guest(0x007B2180u,1,&buffer);release(pcm);return 0;}
 memcpy(hle_ptr(buffer),pcm,bytes);release(pcm);
 MEM16(fmt)=1;MEM16(fmt+2)=channels;MEM32(fmt+4)=rate;MEM32(fmt+8)=rate*channels*2;
 MEM16(fmt+12)=channels*2;MEM16(fmt+14)=16;MEM16(fmt+16)=0;
 MEM32(object+0x20)=fmt;MEM32(object+0x24)=buffer;MEM32(object+0x28)=n;
 static unsigned count;if(++count<=4)fprintf(stderr,"[native-vorbis-probe] decoded bytes=%u channels=%d rate=%ld\n",n,channels,rate);
 return 1;
}
