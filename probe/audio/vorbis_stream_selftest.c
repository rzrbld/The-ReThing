#include "vorbis_stream.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {unsigned char*data;size_t size,pos;unsigned closes;} Input;
static size_t rd(void*out,size_t size,size_t count,void*arg){Input*i=arg;if(!size)return 0;size_t n=(i->size-i->pos)/size;if(n>count)n=count;memcpy(out,i->data+i->pos,n*size);i->pos+=n*size;return n;}
static int sk(void*arg,int64_t off,int w){Input*i=arg;int64_t base=w==SEEK_SET?0:w==SEEK_CUR?(int64_t)i->pos:w==SEEK_END?(int64_t)i->size:-1;if(base<0||off< -base||off>(int64_t)i->size-base)return -1;i->pos=base+off;return 0;}
static int cl(void*arg){((Input*)arg)->closes++;return 0;}
static int64_t tl(void*arg){return ((Input*)arg)->pos;}
static unsigned char*file(const char*path,size_t*n){FILE*f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long z=ftell(f);assert(z>0);rewind(f);unsigned char*p=malloc(z);assert(p);assert(fread(p,1,z,f)==(size_t)z);fclose(f);*n=z;return p;}
static void compare(const unsigned char*a,const unsigned char*b,size_t n){assert(!(n&1));for(size_t i=0;i<n;i+=2){int x=(int16_t)((unsigned)a[i]|((unsigned)a[i+1]<<8));int y=(int16_t)((unsigned)b[i]|((unsigned)b[i+1]<<8));assert(abs(x-y)<=1);}}
int main(int argc,char**argv){assert(argc==2);unsigned seeks=0;
 for(unsigned i=1;i<=282;i++){
  char path[4096];Input in={0};size_t bytes;
  snprintf(path,sizeof path,"%s/decode-%04u.ogg",argv[1],i);in.data=file(path,&in.size);
  snprintf(path,sizeof path,"%s/decode-%04u.pcm",argv[1],i);unsigned char*ref=file(path,&bytes);
  ThingVorbisCallbacks cb={rd,sk,cl,tl};void*s=NULL;assert(!thing_vorbis_open(&in,cb,&s));assert(s);
  int channels=0;int64_t rate=0;assert(!thing_vorbis_info(s,-1,&channels,&rate));assert(channels>0&&channels<=2&&rate>0);
  int64_t frames=thing_vorbis_total(s,-1);assert(frames*channels*2==(int64_t)bytes);
  size_t pos=0;unsigned step=0;int sizes[]={7,256,4096,16385};unsigned char buf[16385];int section=0;
  for(;;){long n=thing_vorbis_read(s,buf,sizes[step++%4],0,2,1,&section);assert(n>=0&&section==0);if(!n)break;assert(pos+(size_t)n<=bytes);compare(ref+pos,buf,n);pos+=n;}
  assert(pos==bytes);assert(thing_vorbis_read(s,buf,sizeof buf,0,2,1,&section)==0);
  int64_t offsets[]={0,frames/2,frames-1,frames,0};
  for(unsigned j=0;j<5;j++){assert(!thing_vorbis_seek(s,offsets[j]));long n=thing_vorbis_read(s,buf,sizeof buf,0,2,1,&section);assert(n>=0);size_t offset=offsets[j]*channels*2;assert(offset+n<=bytes);compare(ref+offset,buf,n);seeks++;}
  assert(thing_vorbis_seek(s,-1)<0);assert(thing_vorbis_seek(s,frames+1)<0);
  assert(!thing_vorbis_clear(s));assert(in.closes==1);free(ref);free(in.data);
 }
 unsigned char bad[16]={0};Input in={bad,sizeof bad,0,0};ThingVorbisCallbacks cb={rd,sk,cl,tl};void*s=(void*)1;
 assert(thing_vorbis_open(&in,cb,&s)<0&&!s&&in.closes==0);
 printf("PASS 282 original PCM streams, partial reads, EOF, %u seeks/restarts, invalid seeks, and single-close ownership\n",seeks);return 0;
}
