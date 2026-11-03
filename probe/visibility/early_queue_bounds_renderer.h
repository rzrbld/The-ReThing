/* Included after D8State in an isolated experimental renderer build. */
#include "precomputed_bounds.h"
#include <stdlib.h>
unsigned thing_terrain_scope;
int thing_queue_bounds_outside(const float *bounds,const float *world){
 static int enabled=-1;
 static float previous[48];static int have_previous;
 static ThingBoundsPlanes planes;
 if(enabled<0){const char *e=getenv("THING_EARLY_QUEUE_BOUNDS");enabled=e&&atoi(e)==1;}
 if(!enabled)return 0;
 /* Cache only matrix values, never guest pointers or object bounds. Comparing
  * current matrices handles camera, world, projection and resource changes. */
 if(!have_previous||memcmp(previous,world,64)||memcmp(previous+16,state.transform[2],64)||memcmp(previous+32,state.transform[3],64)){
  memcpy(previous,world,64);memcpy(previous+16,state.transform[2],64);memcpy(previous+32,state.transform[3],64);
  thing_bounds_prepare(&planes,previous,previous+16,previous+32);have_previous=1;
 }
 return thing_bounds_outside(&planes,bounds,bounds+3);
}
