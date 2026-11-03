#ifndef THING_D3D8_INDEX_RANGE_H
#define THING_D3D8_INDEX_RANGE_H
#include <stdint.h>
#include <string.h>
#if defined(__aarch64__)
#include <arm_neon.h>
#endif
/* Read exactly count indices, including unaligned guest storage. The caller
 * validates the maximum before vertex access; malformed draws retain their
 * original diagnostic slow path. This holds no persistent buffer cache. */
static inline void d8_index_range(const void *data,unsigned count,unsigned size,
                                  unsigned *minimum,unsigned *maximum){
    unsigned lo=UINT32_MAX,hi=0,i=0;
    const unsigned char *bytes=data;
#if defined(__aarch64__)
    if(size==2&&count>=8){
        uint16x8_t mn=vdupq_n_u16(UINT16_MAX),mx=vdupq_n_u16(0);
        for(;count-i>=8;i+=8){uint16x8_t v;memcpy(&v,bytes+(size_t)i*2,sizeof v);mn=vminq_u16(mn,v);mx=vmaxq_u16(mx,v);}
        lo=vminvq_u16(mn);hi=vmaxvq_u16(mx);
    }else if(size==4&&count>=4){
        uint32x4_t mn=vdupq_n_u32(UINT32_MAX),mx=vdupq_n_u32(0);
        for(;count-i>=4;i+=4){uint32x4_t v;memcpy(&v,bytes+(size_t)i*4,sizeof v);mn=vminq_u32(mn,v);mx=vmaxq_u32(mx,v);}
        lo=vminvq_u32(mn);hi=vmaxvq_u32(mx);
    }
#endif
    if(size==2){for(;i<count;i++){uint16_t v;memcpy(&v,bytes+(size_t)i*2,2);if(v<lo)lo=v;if(v>hi)hi=v;}}
    else{for(;i<count;i++){uint32_t v;memcpy(&v,bytes+(size_t)i*4,4);if(v<lo)lo=v;if(v>hi)hi=v;}}
    *minimum=lo;*maximum=hi;
}
#endif
