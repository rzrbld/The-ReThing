#ifndef THING_FPU_ENV_H
#define THING_FPU_ENV_H
#include <stdint.h>
/* Restricted to the hash-checked CRT status-word edit sequences patched by
 * scripts/patch_fpu_env.py. This is not a general x87 environment emulator.
 * Tag/TOP/CW/pointer field changes between the paired instructions are rejected. */
void thing_fpu_env_store28(void *destination,const double st[8],int depth,uint16_t *cw,int comparison);
int thing_fpu_env_load28(const void *source,const double st[8],int depth,uint16_t *cw);
#endif
