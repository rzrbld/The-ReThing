/* Conservative trivial clip rejection for XYZ triangle draws only.
 * An AABB encloses every indexed vertex; only reject if its eight transformed
 * corners lie strictly outside one homogeneous clip plane. No temporal reuse. */
#ifndef THING_CPU_CLIP_H
#define THING_CPU_CLIP_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
static int thing_cpu_clip(const float world[16],const float view[16],const float proj[16],
 const void *data,size_t stride,unsigned vcount,const void *indices,unsigned isize,unsigned base,unsigned n){
 if(!data||stride<12||!n||!vcount||(indices&&isize!=2&&isize!=4))return 0;
 float lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};
 for(unsigned i=0;i<n;i++){
  uint32_t ix=i;if(indices){if(isize==2){uint16_t v;memcpy(&v,(const char*)indices+i*2,2);ix=v;}else memcpy(&ix,(const char*)indices+i*4,4);}
  if(ix>=vcount||ix>UINT32_MAX-base||(size_t)(base+ix)>(SIZE_MAX-12)/stride)return 0;
  float p[3];memcpy(p,(const char*)data+(size_t)(base+ix)*stride,12);
  for(int j=0;j<3;j++){if(!isfinite(p[j]))return 0;if(p[j]<lo[j])lo[j]=p[j];if(p[j]>hi[j])hi[j]=p[j];}
 }
 unsigned outside=63;const float *ms[3]={world,view,proj};
 /* Propagate absolute magnitude/error bounds through all three matrices.
  * This remains conservative under cancellation and amplified prior error. */
 double mag[4]={fmax(fabs(lo[0]),fabs(hi[0])),fmax(fabs(lo[1]),fabs(hi[1])),fmax(fabs(lo[2]),fabs(hi[2])),1},err[4]={0};
 for(int m=0;m<3;m++){
  double next[4]={0},error[4]={0};
  for(int j=0;j<4;j++)for(int k=0;k<4;k++){double a=fabs(ms[m][k*4+j]);next[j]+=a*mag[k];error[j]+=a*err[k];}
  for(int j=0;j<4;j++){error[j]+=1e-5*next[j];if(!isfinite(error[j])||!isfinite(next[j])||next[j]>1e20)return 0;}
  memcpy(mag,next,sizeof mag);memcpy(err,error,sizeof err);
 }
 double epsilon=1e-5;for(int j=0;j<4;j++)epsilon+=err[j]+1e-5*mag[j];
 for(unsigned c=0;c<8;c++){
  double p[4]={c&1?hi[0]:lo[0],c&2?hi[1]:lo[1],c&4?hi[2]:lo[2],1},error_scale=1;
  for(int m=0;m<3;m++){
   double q[4]={0},scale=0;
   for(int j=0;j<4;j++)for(int k=0;k<4;k++){
    double term=p[k]*ms[m][k*4+j];q[j]+=term;scale+=fabs(term);
   }
   error_scale=fmax(error_scale,scale);memcpy(p,q,sizeof p);
  }
  if(!isfinite(error_scale)||error_scale>1e12||!isfinite(p[3])||p[3]<=0)return 0;

  unsigned flags=0;double distances[6]={p[0]+p[3],p[3]-p[0],p[1]+p[3],p[3]-p[1],p[2],p[3]-p[2]};
  for(int j=0;j<6;j++){if(!isfinite(distances[j]))return 0;if(distances[j]<-epsilon)flags|=1u<<j;}
  outside&=flags;if(!outside)return 0;
 }
 return outside!=0;
}
#endif
