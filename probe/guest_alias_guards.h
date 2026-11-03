#ifndef THING_GUEST_ALIAS_GUARDS_H
#define THING_GUEST_ALIAS_GUARDS_H
/* Integer-only checks: do not change host FP exception state. Fallback retains
 * original NaN payload behavior and sequential overlapping-buffer semantics. */
static inline int thing_alias_finite(uint32_t a,unsigned n){
 if((uint64_t)a+(uint64_t)n*4>UINT64_C(0x100000000))return 0;
 for(unsigned i=0;i<n;i++)if((MEM32(a+i*4)&0x7f800000u)==0x7f800000u)return 0;
 return 1;
}
static inline int thing_alias_disjoint(uint32_t out,unsigned n,uint32_t a,uint32_t b){
 uint64_t bytes=(uint64_t)n*4,end=(uint64_t)out+bytes;
 if(end>UINT64_C(0x100000000))return 0;
 return (end<=a||(uint64_t)a+bytes<=out)&&(end<=b||(uint64_t)b+bytes<=out);
}
static inline int thing_alias_eligible(unsigned index){
 uint32_t a=g_edx,b=0;unsigned n=index==4?16:3;
 if(index==2)return thing_alias_finite(g_ecx,3)&&thing_alias_finite(a,3);
 b=MEM32(g_esp+4);
 if(index==1){
  if((b&0x7f800000u)==0x7f800000u)return 0;
  /* Stack scalar is an input too; reject any output/argument overlap. */
  return thing_alias_disjoint(g_ecx,3,a,g_esp+4)&&thing_alias_finite(a,3);
 }
 return thing_alias_disjoint(g_ecx,n,a,b)&&thing_alias_finite(a,n)&&thing_alias_finite(b,n);
}
#endif
