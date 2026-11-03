/* Batched decoder validation driver. Captured game assets remain private. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
int probe_decode(const unsigned char*,size_t,unsigned char**,size_t*,long*,int*,long long*);
void probe_free(void*);
static uint64_t ns(void){struct timespec t;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
int main(int argc,char**argv){
 if(argc!=3){fprintf(stderr,"usage: corpus INPUT_DIRECTORY OUTPUT_DIRECTORY\n");return 2;}
 uint64_t total=0;unsigned files=0;
 for(unsigned i=1;i<=282;i++){
  char path[4096];snprintf(path,sizeof path,"%s/decode-%04u.ogg",argv[1],i);
  FILE*f=fopen(path,"rb");if(!f){perror(path);return 3;}
  if(fseek(f,0,SEEK_END)){fclose(f);return 4;}long length=ftell(f);
  if(length<=0||length>16000000){fclose(f);return 5;}rewind(f);
  unsigned char*input=malloc((size_t)length);if(!input){fclose(f);return 6;}
  if(fread(input,1,(size_t)length,f)!=(size_t)length){free(input);fclose(f);return 7;}fclose(f);
  unsigned char*out=NULL;size_t bytes=0;long rate=0;int channels=0;long long frames=0;
  uint64_t start=ns();int rc=probe_decode(input,length,&out,&bytes,&rate,&channels,&frames);uint64_t elapsed=ns()-start;
  free(input);if(rc){fprintf(stderr,"decode %u failed: %d\n",i,rc);return 8;}
  snprintf(path,sizeof path,"%s/decode-%04u.pcm",argv[2],i);f=fopen(path,"wb");
  if(!f||fwrite(out,1,bytes,f)!=bytes){probe_free(out);if(f)fclose(f);return 9;}fclose(f);probe_free(out);
  printf("%u,%zu,%ld,%d,%lld,%llu\n",i,bytes,rate,channels,frames,(unsigned long long)elapsed);total+=elapsed;files++;
 }
 fprintf(stderr,"decoded=%u thread_cpu_ns=%llu (emulator timing is not hardware timing)\n",files,(unsigned long long)total);return 0;
}
