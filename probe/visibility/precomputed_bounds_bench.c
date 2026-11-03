#define _POSIX_C_SOURCE 200809L
#include "precomputed_bounds.h"
#include "cpu_clip.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static float matrices[3][16],boxes[256][6],vertices[256][24];static ThingBoundsPlanes planes;
__attribute__((noinline,noclone)) static int old(unsigned i){return thing_cpu_clip(matrices[0],matrices[1],matrices[2],vertices[i],12,8,NULL,0,0,8);}
__attribute__((noinline,noclone)) static int cached(unsigned i){return thing_bounds_outside(&planes,boxes[i],boxes[i]+3);}
__attribute__((noinline,noclone)) static int uncached(unsigned i){ThingBoundsPlanes p;thing_bounds_prepare(&p,matrices[0],matrices[1],matrices[2]);return thing_bounds_outside(&p,boxes[i],boxes[i]+3);}
int main(int argc,char **argv){if(argc!=3)return 1;unsigned variant=atoi(argv[1]),n=atoi(argv[2]);for(unsigned m=0;m<3;m++)for(unsigned i=0;i<4;i++)matrices[m][i*5]=1;matrices[1][12]=.123f;
 for(unsigned b=0;b<256;b++){boxes[b][0]=(int)(b%16)*.4f-3;boxes[b][1]=0;boxes[b][2]=.2f;for(unsigned j=0;j<3;j++)boxes[b][j+3]=boxes[b][j]+.1f;for(unsigned c=0;c<8;c++)for(unsigned j=0;j<3;j++)vertices[b][c*3+j]=boxes[b][j+((c&(1u<<j))?3:0)];}
 thing_bounds_prepare(&planes,matrices[0],matrices[1],matrices[2]);int(*fn)(unsigned)=variant==0?old:variant==1?cached:uncached;uint64_t checksum=0;struct timespec a,b;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&a);for(unsigned i=0;i<n;i++)checksum+=fn(i&255);clock_gettime(CLOCK_THREAD_CPUTIME_ID,&b);printf("variant=%u ns=%.4f checksum=%llu\n",variant,((b.tv_sec-a.tv_sec)*1e9+b.tv_nsec-a.tv_nsec)/n,(unsigned long long)checksum);return 0;}
