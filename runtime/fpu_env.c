#include "fpu_env.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const void *address;
    uint8_t environment[28];
    double values[8];
    int depth,valid;
} EnvironmentPair;
static _Thread_local EnvironmentPair saved;
static _Noreturn void unsupported(const char *why) {
    fprintf(stderr,"[x87-env] unsupported environment operation: %s\n",why);
    abort();
}
static uint16_t read16(const void *p) {uint16_t v;memcpy(&v,p,2);return v;}
static void write16(void *p,uint16_t v) {memcpy(p,&v,2);}
static uint16_t condition_bits(int comparison) {
    if(comparison>=0x10000)return (uint16_t)comparison&0x4700u;
    return comparison==2?0x4500u:comparison<0?0x0100u:comparison==0?0x4000u:0;
}
static uint16_t tags(const double st[8],int depth) {
    if(depth<0 || depth>8)unsupported("fixed-window stack depth outside 0..8");
    uint16_t word=0xffffu;
    unsigned top=(unsigned)(-depth)&7u;
    for(int i=0;i<depth;i++) {
        /* A loaded binary64 subnormal is normal in x87 extended precision. */
        unsigned tag=st[i]==0.0?1u:!isfinite(st[i])?2u:0u;
        unsigned physical=(top+(unsigned)i)&7u,shift=physical*2;
        word=(uint16_t)((word&~(3u<<shift))|(tag<<shift));
    }
    return word;
}
void thing_fpu_env_store28(void *destination,const double st[8],int depth,uint16_t *cw,int comparison) {
    if(saved.valid)unsupported("nested unpaired FNSTENV");
    uint16_t tag=tags(st,depth),status=condition_bits(comparison)|((unsigned)(-depth)&7u)<<11;
    uint8_t *out=destination;
    memset(out,0,28);
    write16(out,*cw);write16(out+2,0xffffu);
    write16(out+4,status);write16(out+6,0xffffu);
    write16(out+8,tag);write16(out+10,0xffffu);
    /* Pointer/opcode/exception fields are absent from the fixed-window model.
     * The checked CRT sequences never inspect or alter those fields. */
    saved.address=destination;saved.depth=depth;
    memcpy(saved.environment,out,28);memcpy(saved.values,st,sizeof saved.values);
    saved.valid=1;
    *cw|=0x003fu; /* FNSTENV masks all current exceptions after storing. */
}
int thing_fpu_env_load28(const void *source,const double st[8],int depth,uint16_t *cw) {
    if(!saved.valid || saved.address!=source)unsupported("FLDENV is not paired with the checked FNSTENV");
    const uint8_t *in=source;
    uint16_t status=read16(in+4),prior=read16(saved.environment+4);
    if(depth!=saved.depth || memcmp(st,saved.values,sizeof saved.values))unsupported("FP registers changed inside checked status edit");
    if(read16(in)!=read16(saved.environment))unsupported("control-word edit outside checked sequence");
    if(read16(in+8)!=read16(saved.environment+8))unsupported("tag-word edit outside checked sequence");
    if((status&~0x4700u)!=(prior&~0x4700u))unsupported("TOP/exception/status edit outside checked sequence");
    if(memcmp(in+12,saved.environment+12,16))unsupported("pointer/opcode edit outside checked sequence");
    *cw=read16(in);
    saved.valid=0;
    return 0x10000|(status&0x4700u);
}
