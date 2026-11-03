#ifndef THING_VORBIS_STREAM_H
#define THING_VORBIS_STREAM_H
#include <stdint.h>
#include <stddef.h>
/* Native ABI only: the guest bridge must marshal 32-bit callbacks separately. */
typedef struct {
 size_t (*read)(void*,size_t,size_t,void*);
 int (*seek)(void*,int64_t,int);
 int (*close)(void*);
 int64_t (*tell)(void*);
} ThingVorbisCallbacks;
int thing_vorbis_open(void *source,ThingVorbisCallbacks callbacks,void **result);
int thing_vorbis_info(void *stream,int link,int *channels,int64_t *rate);
int64_t thing_vorbis_total(void *stream,int link);
int thing_vorbis_seek(void *stream,int64_t frame);
long thing_vorbis_read(void *stream,void *output,int bytes,int bigendian,int word,int sign,int *section);
int thing_vorbis_clear(void *stream);
#endif
