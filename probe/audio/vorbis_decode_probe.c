/* Offline comparison only. Native Vorbisfile via its published ABI. */
#include <vorbis/vorbisfile.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
typedef struct { const unsigned char *data; size_t size,pos; } Memory;
static size_t rd(void *dst,size_t size,size_t count,void *opaque){
 Memory*m=opaque;if(!size)return 0;
 size_t n=(m->size-m->pos)/size;if(n>count)n=count;
 memcpy(dst,m->data+m->pos,n*size);m->pos+=n*size;return n;
}
static int sk(void *opaque,ogg_int64_t offset,int whence){
 Memory*m=opaque;ogg_int64_t base;
 if(whence==SEEK_SET)base=0;else if(whence==SEEK_CUR)base=m->pos;else if(whence==SEEK_END)base=m->size;else return -1;
 if(offset < -base || offset > (ogg_int64_t)m->size-base)return -1;
 m->pos=(size_t)(base+offset);return 0;
}
static long tl(void *opaque){return (long)((Memory*)opaque)->pos;}
int probe_decode(const unsigned char *input,size_t size,unsigned char **output,
 size_t *bytes,long *rate,int *channels,long long *frames){
 if(!output||!bytes||!rate||!channels||!frames||size>LONG_MAX)return -1;
 *output=NULL;*bytes=0;Memory m={input,size,0};OggVorbis_File vf;
 ov_callbacks cb={rd,sk,NULL,tl};int rc=ov_open_callbacks(&m,&vf,NULL,0,cb);if(rc)return rc;
 vorbis_info *info=ov_info(&vf,-1);ogg_int64_t n=ov_pcm_total(&vf,-1);
 if(!info||ov_streams(&vf)!=1||info->channels<1||info->channels>2||n<0||n>16000000){ov_clear(&vf);return -2;}
 *channels=info->channels;*rate=info->rate;*frames=n;
 size_t capacity=(size_t)n*info->channels*2;unsigned char *out=malloc(capacity?capacity:1);
 if(!out){ov_clear(&vf);return -3;}
 size_t used=0;char chunk[16384];int section=0;
 for(;;){long got=ov_read(&vf,chunk,sizeof chunk,0,2,1,&section);
  if(!got)break;
  if(got<0||section!=0||used+(size_t)got>capacity){free(out);ov_clear(&vf);return -4;}
  memcpy(out+used,chunk,(size_t)got);used+=(size_t)got;
 }
 ov_clear(&vf);*output=out;*bytes=used;return 0;
}
void probe_free(void *p){free(p);}
