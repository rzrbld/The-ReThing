#ifndef THING_GUEST_ALIAS_GUARDS_INPLACE_H
#define THING_GUEST_ALIAS_GUARDS_INPLACE_H
/* Keep the prior conservative implementation for matrix/dot and reuse checks.
 * Exact vector in-place writes do not overwrite a later input component.
 * Partial overlaps still use the original sequential GCC fallback. */
#define thing_alias_eligible thing_alias_original_eligible
#include "guest_alias_guards.h"
#undef thing_alias_eligible
static inline int thing_alias_vector_input_safe(uint32_t out,uint32_t input){
 uint64_t end=(uint64_t)out+12,input_end=(uint64_t)input+12;
 if(end>UINT64_C(0x100000000)||input_end>UINT64_C(0x100000000))return 0;
 return out==input||end<=input||input_end<=out;
}
static inline int thing_alias_eligible(unsigned index){
 if(index==2||index==4)return thing_alias_original_eligible(index);
 uint32_t out=g_ecx,a=g_edx,b=MEM32(g_esp+4);
 if(!thing_alias_vector_input_safe(out,a))return 0;
 if(index==1){
  if((b&0x7f800000u)==0x7f800000u)return 0;
  /* Never permit overlap with the stack scalar, including exact equality. */
  if(!thing_alias_disjoint(out,3,g_esp+4,g_esp+4))return 0;
  return thing_alias_finite(a,3);
 }
 return thing_alias_vector_input_safe(out,b)&&thing_alias_finite(a,3)&&thing_alias_finite(b,3);
}
#endif
