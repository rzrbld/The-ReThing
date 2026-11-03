/* Experimental ordinary-terrain queue rejection. No persistent geometry cache:
 * bounds come from the currently validated resource on every invocation. */
#ifndef THING_EARLY_QUEUE_BOUNDS_H
#define THING_EARLY_QUEUE_BOUNDS_H
#include <stdint.h>
extern unsigned thing_terrain_scope;
int thing_queue_bounds_outside(const float *bounds,const float *world);
#endif
