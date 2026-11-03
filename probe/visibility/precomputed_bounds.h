/* Conservative D3D clip-space AABB test. Prepare once for a world/view/projection
 * combination; test cached object-space bounds without rescanning vertices.
 * Unknown/nonfinite inputs and bounds crossing the eye always remain visible. */
#ifndef THING_PRECOMPUTED_BOUNDS_H
#define THING_PRECOMPUTED_BOUNDS_H
#include <math.h>
#include <string.h>
typedef struct { double plane[6][4],w[4],magnitude[4];int valid; } ThingBoundsPlanes;
static void thing_bounds_prepare(ThingBoundsPlanes *out,const float world[16],const float view[16],const float projection[16]){
 double combined[16]={0},absolute[16]={0};const float *matrices[3]={world,view,projection};memset(out,0,sizeof *out);
 for(unsigned i=0;i<4;i++)combined[i*4+i]=absolute[i*4+i]=1;
 for(unsigned m=0;m<3;m++){
  double next[16]={0},bound[16]={0};
  for(unsigned i=0;i<16;i++)if(!isfinite(matrices[m][i]))return;
  for(unsigned i=0;i<4;i++)for(unsigned j=0;j<4;j++)for(unsigned k=0;k<4;k++){
   next[i*4+j]+=combined[i*4+k]*(double)matrices[m][k*4+j];
   bound[i*4+j]+=absolute[i*4+k]*fabs((double)matrices[m][k*4+j]);
  }
  for(unsigned i=0;i<16;i++)if(!isfinite(next[i])||!isfinite(bound[i])||bound[i]>1e20)return;
  memcpy(combined,next,sizeof next);memcpy(absolute,bound,sizeof bound);
 }
 for(unsigned i=0;i<4;i++){
  out->plane[0][i]=combined[i*4+3]+combined[i*4];out->plane[1][i]=combined[i*4+3]-combined[i*4];
  out->plane[2][i]=combined[i*4+3]+combined[i*4+1];out->plane[3][i]=combined[i*4+3]-combined[i*4+1];
  out->plane[4][i]=combined[i*4+2];out->plane[5][i]=combined[i*4+3]-combined[i*4+2];
  out->w[i]=combined[i*4+3];
  /* Sum of absolute product terms bounds cancellation and error amplification
   * through all three GPU float32 matrix stages. Margin deliberately exceeds
   * the rounding bound, including fused arithmetic and double preparation. */
  for(unsigned j=0;j<4;j++)out->magnitude[i]+=absolute[i*4+j];
 }
 out->valid=1;
}
static int thing_bounds_outside(const ThingBoundsPlanes *p,const float lo[3],const float hi[3]){
 if(!p->valid)return 0;
 double mag=p->magnitude[3],wmin=p->w[3];
 for(unsigned i=0;i<3;i++){
  if(!isfinite(lo[i])||!isfinite(hi[i])||lo[i]>hi[i])return 0;
  mag+=fmax(fabs((double)lo[i]),fabs((double)hi[i]))*p->magnitude[i];
  wmin+=p->w[i]*(p->w[i]>=0?lo[i]:hi[i]);
 }
 if(!isfinite(mag)||mag>1e20)return 0;
 double epsilon=1e-5+1e-4*mag;
 if(!isfinite(wmin)||wmin<=epsilon)return 0;
 for(unsigned j=0;j<6;j++){
  double maximum=p->plane[j][3];
  for(unsigned i=0;i<3;i++)maximum+=p->plane[j][i]*(p->plane[j][i]>=0?hi[i]:lo[i]);
  if(isfinite(maximum)&&maximum < -epsilon)return 1;
 }
 return 0;
}
#endif
