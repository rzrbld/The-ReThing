/* Isolated prototype. Only known The Thing file callbacks are intercepted. */
#include "win32hle.h"
#include "vorbis_stream.h"
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct {uint32_t key,source,info;void*native;} GuestStream;
static GuestStream stream_slots[8];
static int (*vs_open)(void*,ThingVorbisCallbacks,void**);
static int (*vs_info)(void*,int,int*,int64_t*);
static int64_t (*vs_total)(void*,int);
static int (*vs_seek)(void*,int64_t);
static long (*vs_read)(void*,void*,int,int,int,int,int*);
static int (*vs_clear)(void*);
static int stream_ready(void){static int checked;if(checked)return vs_open!=NULL;checked=1;
 const char*p=getenv("THING_NATIVE_STREAM_PROBE");if(!p||!*p)return 0;void*h=dlopen(p,RTLD_NOW|RTLD_LOCAL);if(!h)return 0;
 vs_open=dlsym(h,"thing_vorbis_open");vs_info=dlsym(h,"thing_vorbis_info");vs_total=dlsym(h,"thing_vorbis_total");vs_seek=dlsym(h,"thing_vorbis_seek");vs_read=dlsym(h,"thing_vorbis_read");vs_clear=dlsym(h,"thing_vorbis_clear");
 if(!vs_info||!vs_total||!vs_seek||!vs_read||!vs_clear)vs_open=NULL;return vs_open!=NULL;
}
static size_t gs_read(void*out,size_t size,size_t count,void*opaque){GuestStream*s=opaque;if(!size||count>UINT32_MAX/size)return 0;uint32_t bytes=size*count;
 uint32_t mark=hle_scratch_mark(),buffer=hle_scratch(bytes);if(!buffer)return 0;
 uint32_t args[]={buffer,1,bytes,s->source};uint32_t got=hle_call_guest(0x79E420,4,args);
 if(got>bytes)got=0;if(got)memcpy(out,hle_ptr(buffer),got);hle_scratch_release(mark);return got/size;
}
static int gs_seek(void*opaque,int64_t off,int whence){GuestStream*s=opaque;uint32_t args[]={s->source,(uint32_t)off,(uint32_t)((uint64_t)off>>32),(uint32_t)whence};return (int32_t)hle_call_guest(0x79E490,4,args);}
static int gs_close(void*opaque){GuestStream*s=opaque;uint32_t arg=s->source;return (int32_t)hle_call_guest(0x79E450,1,&arg);}
static int64_t gs_tell(void*opaque){GuestStream*s=opaque;uint32_t arg=s->source;return (int32_t)hle_call_guest(0x79E470,1,&arg);}
int thing_native_stream_dispatch(unsigned address){
 if(!stream_ready())return 0;
 uint32_t key=A32(address==0x7D0B20?1:0);GuestStream*s=NULL;
 for(unsigned i=0;i<8;i++)if(stream_slots[i].key==key&&key){s=&stream_slots[i];break;}
 if(address==0x7D0B20){
  if(s||!key||A32(2)||A32(3)||A32(4)!=0x79E420||A32(5)!=0x79E490||A32(6)!=0x79E450||A32(7)!=0x79E470)return 0;
  for(unsigned i=0;i<8;i++)if(!stream_slots[i].key){s=&stream_slots[i];break;}
  if(!s)return 0;
  uint32_t source=A32(0);void*info=hle_alloc(32);if(!info)return 0;
  /* Original ov_open/ov_clear zero 0xB4 dwords (0x2D0 bytes). */
  memset(hle_ptr(key),0,0x2D0);
  s->key=key;s->source=source;s->info=hle_va(info);ThingVorbisCallbacks cb={gs_read,gs_seek,gs_close,gs_tell};
  int rc=vs_open(s,cb,&s->native);
  if(rc){hle_free(info);memset(s,0,sizeof *s);}else fprintf(stderr,"[native-stream-probe] opened guest=%#x source=%#x\n",key,source);
  g_eax=(uint32_t)rc;g_esp+=4;return 1;
 }
 if(!s)return 0;
 switch(address){
 case 0x7D0A20:{int rc=vs_clear(s->native);memset(hle_ptr(key),0,0x2D0);hle_free(hle_ptr(s->info));memset(s,0,sizeof *s);g_eax=rc;break;}
 case 0x7D2070:{int ch=0;int64_t rate=0;int rc=vs_info(s->native,(int32_t)A32(1),&ch,&rate);if(rc||rate>INT32_MAX||ch<1){g_eax=0;break;}memset(hle_ptr(s->info),0,32);MEM32(s->info+4)=ch;MEM32(s->info+8)=rate;g_eax=s->info;break;}
 case 0x7D1400:{int64_t n=vs_total(s->native,(int32_t)A32(1));g_eax=(uint32_t)n;g_edx=(uint32_t)((uint64_t)n>>32);break;}
 case 0x7D1B40:{int64_t n=(int64_t)((uint64_t)A32(1)|((uint64_t)A32(2)<<32));g_eax=vs_seek(s->native,n);break;}
 case 0x7D20B0:{uint32_t output=A32(1),section_va=A32(6);int section=0;long n=vs_read(s->native,hle_ptr(output),(int)A32(2),(int)A32(3),(int)A32(4),(int)A32(5),&section);if(section_va&&n>=0)MEM32(section_va)=section;g_eax=(uint32_t)n;break;}
 default:return 0;
 }
 g_esp+=4;return 1;
}
