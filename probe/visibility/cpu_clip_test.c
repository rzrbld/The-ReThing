#include "cpu_clip.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
static unsigned seed=12345;
static float rnd(void){seed=1664525*seed+1013904223;return (float)(seed>>8)/16777216.f;}
static void id(float*m){memset(m,0,64);m[0]=m[5]=m[10]=m[15]=1;}
static void vec(float*p,const float*m){float q[4]={0};for(int j=0;j<4;j++)for(int k=0;k<4;k++)q[j]+=p[k]*m[k*4+j];memcpy(p,q,16);}
int main(void){float w[16],v[16],p[16];id(w);id(v);id(p);float xyz[9]={2,0,.5,3,0,.5,2,1,.5};assert(thing_cpu_clip(w,v,p,xyz,12,3,0,0,0,3));xyz[0]=0;assert(!thing_cpu_clip(w,v,p,xyz,12,3,0,0,0,3));xyz[0]=NAN;assert(!thing_cpu_clip(w,v,p,xyz,12,3,0,0,0,3));xyz[0]=2;uint16_t bad[]={0,1,3};assert(!thing_cpu_clip(w,v,p,xyz,12,3,bad,2,0,3));unsigned rejected=0;
 for(int t=0;t<100000;t++){id(w);id(v);id(p);float a=rnd()*6.28f;w[0]=cosf(a);w[2]=sinf(a);w[8]=-sinf(a);w[10]=cosf(a);for(int j=0;j<3;j++){w[12+j]=(rnd()-.5f)*20;v[12+j]=(rnd()-.5f)*20;}p[0]=p[5]=.1f;p[10]=.01f;p[14]=.5f;
 float pos[18];for(int j=0;j<18;j++)pos[j]=(rnd()-.5f)*8;uint16_t ix[3]={1,3,5};int result=thing_cpu_clip(w,v,p,pos,12,6,ix,2,0,3);
 if(result){rejected++;unsigned common=63;for(int i=0;i<3;i++){float q[4]={pos[ix[i]*3],pos[ix[i]*3+1],pos[ix[i]*3+2],1};vec(q,w);vec(q,v);vec(q,p);float d[6]={q[0]+q[3],q[3]-q[0],q[1]+q[3],q[3]-q[1],q[2],q[3]-q[2]};unsigned flags=0;for(int j=0;j<6;j++)if(d[j]<0)flags|=1u<<j;common&=flags;}assert(common);}}
 printf("PASS 100000 seeded matrix/index cases; %u conservative rejections; NaN and invalid indices fall back\n",rejected);
}
