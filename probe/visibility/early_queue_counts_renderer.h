/* Included after D8State in an isolated experimental renderer build. */
#include "precomputed_bounds.h"
#include <stdlib.h>
#include <stdio.h>
unsigned thing_terrain_scope;
static unsigned long long qb_checks,qb_rejects,qb_prepares,qb_frames;
void thing_queue_bounds_report(void){
 if(++qb_frames%600==0){
  fprintf(stderr,"[queue-bounds] frames=600 checks=%llu rejects=%llu matrix_prepares=%llu\n",qb_checks,qb_rejects,qb_prepares);
  qb_checks=qb_rejects=qb_prepares=0;
 }
}
int thing_queue_bounds_outside(const float *bounds,const float *world){
 static int enabled=-1;
 static float previous[48];static int have_previous;
 static ThingBoundsPlanes planes;
 if(enabled<0){const char *e=getenv("THING_EARLY_QUEUE_BOUNDS");enabled=e&&atoi(e)==1;}
 if(!enabled)return 0;
 qb_checks++;
 /* Cache only matrix values, never guest pointers or object bounds. Comparing
  * current matrices handles camera, world, projection and resource changes. */
 if(!have_previous||memcmp(previous,world,64)||memcmp(previous+16,state.transform[2],64)||memcmp(previous+32,state.transform[3],64)){
  memcpy(previous,world,64);memcpy(previous+16,state.transform[2],64);memcpy(previous+32,state.transform[3],64);
  qb_prepares++;thing_bounds_prepare(&planes,previous,previous+16,previous+32);have_previous=1;
 }
 int outside=thing_bounds_outside(&planes,bounds,bounds+3);qb_rejects+=outside!=0;return outside;
}
