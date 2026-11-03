#include "precomputed_bounds.h"
void bounds_prepare(ThingBoundsPlanes*p,const float*w,const float*v,const float*m){thing_bounds_prepare(p,w,v,m);}
int bounds_outside(const ThingBoundsPlanes*p,const float*lo,const float*hi){return thing_bounds_outside(p,lo,hi);}
