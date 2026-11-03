#include "precomputed_bounds.h"
#include <assert.h>
#include <fenv.h>
#include <stdio.h>
#include <stdint.h>
static uint32_t seed=7781;
static uint32_t rng(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static float sample(void){return ((int32_t)(rng()%20001)-10000)*.001f;}
static unsigned oracle(const float m[3][16],const float lo[3],const float hi[3]){
 unsigned common=63;
 for(unsigned c=0;c<24;c++){
  float p[4];for(unsigned j=0;j<3;j++)p[j]=c<8?((c&(1u<<j))?hi[j]:lo[j]):lo[j]+(hi[j]-lo[j])*(rng()%1001)*.001f;p[3]=1;
  for(unsigned k=0;k<3;k++){float q[4]={0};for(unsigned j=0;j<4;j++)for(unsigned i=0;i<4;i++){volatile float product=p[i]*m[k][i*4+j];q[j]=q[j]+product;}memcpy(p,q,sizeof p);}
  if(!isfinite(p[0])||!isfinite(p[1])||!isfinite(p[2])||!isfinite(p[3]))return 0;
  float d[6]={p[3]+p[0],p[3]-p[0],p[3]+p[1],p[3]-p[1],p[2],p[3]-p[2]};unsigned mask=0;
  for(unsigned j=0;j<6;j++)if(d[j]<0)mask|=1u<<j;common&=mask;
 }
 return common;
}
int main(void){float identity[16]={0},lo[3]={2,0,.5},hi[3]={3,1,.8};for(unsigned j=0;j<4;j++)identity[j*5]=1;ThingBoundsPlanes p;thing_bounds_prepare(&p,identity,identity,identity);assert(thing_bounds_outside(&p,lo,hi));lo[0]=0;assert(!thing_bounds_outside(&p,lo,hi));lo[0]=NAN;assert(!thing_bounds_outside(&p,lo,hi));lo[0]=4;assert(!thing_bounds_outside(&p,lo,hi));identity[0]=INFINITY;thing_bounds_prepare(&p,identity,identity,identity);assert(!p.valid);
 int modes[4]={FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};unsigned rejected=0,cases=0;
 for(unsigned mode=0;mode<4;mode++){assert(!fesetround(modes[mode]));for(unsigned trial=0;trial<50000;trial++){
  float m[3][16];for(unsigned k=0;k<3;k++)for(unsigned i=0;i<16;i++)m[k][i]=sample();
  if(trial%2)for(unsigned k=0;k<3;k++){m[k][3]=m[k][7]=m[k][11]=0;m[k][15]=1;}
  for(unsigned j=0;j<3;j++){lo[j]=sample()*10;hi[j]=lo[j]+fabsf(sample());}
  if(trial%7==0){m[0][0]*=1e12f;m[1][0]*=1e-12f;}
  if(trial%11==0){m[0][1]=m[0][0];m[1][0]=-m[1][4];}
  thing_bounds_prepare(&p,m[0],m[1],m[2]);if(thing_bounds_outside(&p,lo,hi)){assert(oracle(m,lo,hi));rejected++;}cases++;
 }}assert(rejected>1000);printf("PASS %u random bounds tests across four rounding modes; %u rejections checked against float32 corner/interior projections\n",cases,rejected);return 0;}
