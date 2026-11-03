/* Per-draw inverse reuse versus preserved reference arithmetic. No device or
 * GLES timing claim: this benchmarks native host CPU lighting only. */
#include "../runtime/d3d8_renderer.c"
#include "d3d8_light_reference.h"
#include <assert.h>
#include <time.h>
static uint32_t random_state=0x51e2f66d;
static uint32_t next(void){random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static float sample(float scale){return ((int32_t)(next()%20001)-10000)*scale/10000.f;}
static void matrix(float *m,unsigned variant){
 memset(m,0,64);m[0]=1.2f;m[5]=.85f;m[10]=1.05f;m[15]=1;
 m[1]=sample(.3f);m[4]=sample(.3f);m[6]=sample(.3f);m[9]=sample(.3f);m[12]=sample(40);m[13]=sample(40);m[14]=sample(40);
 if(variant%9==0)m[0]=m[1]=m[2]=0; /* singular: preserve normal/default eye */
 if(variant%9==1)m[0]=m[5]=m[10]=1e-10f; /* below determinant threshold */
 if(variant%9==2)m[0]=-1.2f;
}
static void scene(D8State *s,unsigned variant){
 memset(s,0,sizeof *s);s->fvf=0x1d2;s->rs[139]=next();s->rs[141]=variant&1;s->rs[142]=(variant>>1)&1;s->rs[143]=(variant>>2)&1;s->rs[29]=(variant>>3)&1;
 for(int i=145;i<=148;i++)s->rs[i]=next()%4;
 for(int i=0;i<16;i++)s->material[i]=sample(.6f)+.5f;s->material[16]=4+(next()%64);
 matrix(s->transform[256],variant);matrix(s->transform[2],variant/9);
 for(unsigned i=0;i<8;i++){
  float *l=s->light[i];uint32_t type=1+(next()%4);memcpy(l,&type,4);s->light_on[i]=next()%3!=0;
  for(unsigned j=1;j<13;j++)l[j]=sample(.5f)+.5f;
  for(unsigned j=13;j<16;j++)l[j]=sample(10);for(unsigned j=16;j<19;j++)l[j]=sample(1);
  if(variant%13==0)l[16]=l[17]=l[18]=0;
  l[19]=variant%5?30:1;l[20]=2;l[21]=1;l[22]=.2f;l[23]=.02f;l[24]=.6f;l[25]=1.3f;
 }
}
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static volatile float checksum;
static void bench(unsigned lights,int cached){
 random_state=0x51e2f66d;checksum=0;D8State s;scene(&s,123);s.rs[142]=1;s.rs[143]=1;s.rs[29]=0;
 for(unsigned i=0;i<8;i++){s.light_on[i]=i<lights;uint32_t type=3;memcpy(s.light[i],&type,4);}
 const unsigned vertices=30000,draws=60;D8CpuLighting context;
 double start=seconds();
 for(unsigned d=0;d<draws;d++){
  /* Model a different world matrix per draw; each has 30000 lit vertices. */
  s.transform[256][1]=(d&7)*.01f;if(cached)cpu_lighting_prepare(&s,&context);
  for(unsigned i=0;i<vertices;i++){
   float p[4]={(i%101)*.1f,(i%37)*.2f,(i%73)*.15f,1},n[3]={.2f,.8f,.3f},c[4]={.4f,.6f,.8f,1},sp[4]={.1f,.2f,.3f,0};
   if(cached)light_vertex(&s,&context,p,n,c,sp);else light_vertex_reference(&s,p,n,c,sp);
   checksum+=c[i%4]*.00001f+sp[(i+1)%4]*.00001f;
  }
 }
 double elapsed=seconds()-start;printf("benchmark lights=%u path=%s vertices_per_draw=%u draws=%u total_ms=%.3f mean_draw_ms=%.6f checksum=%.9g\n",lights,cached?"cached":"reference",vertices,draws,elapsed*1000,elapsed*1000/draws,(double)checksum);
}
int main(void){
 unsigned comparisons=0;
 for(unsigned v=0;v<900;v++){
  D8State s;scene(&s,v);D8CpuLighting cache;cpu_lighting_prepare(&s,&cache);
  for(unsigned i=0;i<32;i++){
   float p[4]={sample(20),sample(20),sample(20),1},n[3]={sample(1),sample(1),sample(1)},a[4],b[4],sa[4],sb[4];
   if(i%7==0)n[0]=n[1]=n[2]=0;
   for(unsigned k=0;k<4;k++){a[k]=b[k]=sample(.6f)+.5f;sa[k]=sb[k]=sample(.6f)+.5f;}
   light_vertex_reference(&s,p,n,a,sa);light_vertex(&s,&cache,p,n,b,sb);
   if(memcmp(a,b,sizeof a)||memcmp(sa,sb,sizeof sa)){fprintf(stderr,"lighting differs state=%u vertex=%u\n",v,i);return 1;}++comparisons;
  }
 }
 printf("PASS: %u bitwise-equal lighting results across singular/near-singular/sheared/reflected matrices, varied material sources, point/spot/directional/disabled lights, local viewer, normalization and specular\n",comparisons);
 for(unsigned lights=1;lights<=8;lights*=8){bench(lights,0);bench(lights,1);}
 return 0;
}
