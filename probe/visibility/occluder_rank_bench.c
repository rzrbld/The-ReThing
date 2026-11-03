/* Isolated preparation component: current AABB projection and bounded ranking.
 * Excludes scene traversal, extraction, triangle preparation and rasterization. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
typedef struct {uint32_t id;double a[54],expected;} Bounds;
typedef struct {uint32_t id;double score;} Entry;
static double project_score(const Bounds *b){
 const double *a=b->a;double lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};
 for(unsigned c=0;c<8;c++){
  double p[4]={a[(c&1)?3:0],a[(c&2)?4:1],a[(c&4)?5:2],1};
  for(unsigned m=0;m<3;m++){
   const double *v=a+6+16*m;double q[4];
   for(unsigned j=0;j<4;j++)q[j]=p[0]*v[j]+p[1]*v[4+j]+p[2]*v[8+j]+p[3]*v[12+j];
   for(unsigned j=0;j<4;j++)p[j]=q[j];
  }
  if(!isfinite(p[3])||p[3]<=1e-4)return -1;
  for(unsigned j=0;j<3;j++){
   double v=p[j]/p[3];if(!isfinite(v))return -1;
   if(v<lo[j])lo[j]=v;
   if(v>hi[j])hi[j]=v;
  }
 }
 double x=fmax(0,fmin(1,hi[0])-fmax(-1,lo[0]));
 double y=fmax(0,fmin(1,hi[1])-fmax(-1,lo[1]));
 return x*y*fmax(1e-6,1-lo[2]);
}
static unsigned select_bounds(const Bounds *b,unsigned n,unsigned k,Entry *out,int verify){
 unsigned count=0;
 for(unsigned i=0;i<n;i++){
  double score=project_score(b+i);
  if(verify&&fabs(score-b[i].expected)>1e-9*fmax(1,fabs(b[i].expected))){fprintf(stderr,"score mismatch %u %.17g %.17g\n",b[i].id,score,b[i].expected);exit(2);}
  if(score<0)continue;
  unsigned at=0;while(at<count&&(out[at].score>score||(out[at].score==score&&out[at].id>b[i].id)))at++;
  if(at>=k)continue;
  if(count<k)count++;
  for(unsigned j=count-1;j>at;j--)out[j]=out[j-1];
  out[at]=(Entry){b[i].id,score};
 }
 return count;
}
static void rd(FILE *f,void *p,size_t n){if(fread(p,1,n,f)!=n)exit(2);}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc,char **argv){
 if(argc<2)return 2;
 unsigned iterations=argc>2?(unsigned)strtoul(argv[2],0,10):1000;if(!iterations)return 2;
 FILE *f=fopen(argv[1],"rb");if(!f)return 2;
 uint32_t h[4];rd(f,h,16);unsigned n=h[1],k=h[2];if(h[0]!=0x4b4e4152||n>10000||!k||k>64||h[3]>k)return 2;
 Bounds *b=calloc(n,sizeof *b);if(!b)return 2;Entry chosen[64];uint32_t expected[64];
 for(unsigned i=0;i<n;i++){rd(f,&b[i].id,4);rd(f,b[i].a,432);rd(f,&b[i].expected,8);for(unsigned j=0;j<54;j++)if(!isfinite(b[i].a[j]))return 2;}
 rd(f,expected,h[3]*4);if(fgetc(f)!=EOF)return 2;fclose(f);
 unsigned count=select_bounds(b,n,k,chosen,1);if(count!=h[3])return 2;
 for(unsigned i=0;i<count;i++)if(chosen[i].id!=expected[i]){fprintf(stderr,"ranking mismatch %u got %u expected %u\n",i,chosen[i].id,expected[i]);return 2;}
 volatile uint64_t checksum=0;
 for(unsigned j=0;j<20;j++){select_bounds(b,n,k,chosen,0);checksum+=chosen[0].id;}
 double start=now();for(unsigned j=0;j<iterations;j++){select_bounds(b,n,k,chosen,0);checksum+=chosen[0].id;}
 printf("PASS bounds=%u budget=%u rank_us=%.3f iterations=%u checksum=%llu\n",n,k,(now()-start)*1e6/iterations,iterations,(unsigned long long)checksum);
 free(b);return 0;
}
