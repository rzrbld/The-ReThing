#include "vorbis_stream.h"
#include <vorbis/vorbisfile.h>
#include <stdlib.h>
#include <limits.h>
typedef struct {OggVorbis_File vf;void *source;ThingVorbisCallbacks cb;} Stream;
static size_t read_cb(void *p,size_t s,size_t n,void *opaque){Stream*t=opaque;return t->cb.read(p,s,n,t->source);}
static int seek_cb(void *opaque,ogg_int64_t n,int whence){Stream*t=opaque;return t->cb.seek(t->source,n,whence);}
static int close_cb(void *opaque){Stream*t=opaque;return t->cb.close?t->cb.close(t->source):0;}
static long tell_cb(void *opaque){Stream*t=opaque;int64_t n=t->cb.tell(t->source);return n<0||n>LONG_MAX?-1:(long)n;}
int thing_vorbis_open(void*source,ThingVorbisCallbacks cb,void**result){
 if(!result||!cb.read)return OV_EINVAL;
 *result=NULL;Stream*t=calloc(1,sizeof *t);if(!t)return OV_EFAULT;t->source=source;t->cb=cb;
 ov_callbacks native={read_cb,cb.seek?seek_cb:NULL,cb.close?close_cb:NULL,cb.tell?tell_cb:NULL};
 int rc=ov_open_callbacks(t,&t->vf,NULL,0,native);if(rc){free(t);return rc;}*result=t;return 0;
}
int thing_vorbis_info(void*s,int link,int*channels,int64_t*rate){if(!s||!channels||!rate)return OV_EINVAL;vorbis_info*v=ov_info(&((Stream*)s)->vf,link);if(!v)return OV_EINVAL;*channels=v->channels;*rate=v->rate;return 0;}
int64_t thing_vorbis_total(void*s,int link){return s?ov_pcm_total(&((Stream*)s)->vf,link):OV_EINVAL;}
int thing_vorbis_seek(void*s,int64_t frame){return s?ov_pcm_seek(&((Stream*)s)->vf,frame):OV_EINVAL;}
long thing_vorbis_read(void*s,void*out,int n,int be,int word,int sign,int*section){
 if(!s||!out||n<=0||(word!=1&&word!=2))return OV_EINVAL;
 return ov_read(&((Stream*)s)->vf,out,n,be,word,sign,section);
}
int thing_vorbis_clear(void*s){if(!s)return OV_EINVAL;Stream*t=s;int rc=ov_clear(&t->vf);free(t);return rc;}
