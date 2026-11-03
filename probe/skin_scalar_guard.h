#ifndef THING_SKIN_SCALAR_GUARD_H
#define THING_SKIN_SCALAR_GUARD_H
/* Integer checks do not raise floating-point exceptions. This guard is only
 * for the straight-line 7AEBBA..7AECD7 block; not a general skinning API. */
static inline int skin_range_ok(uint32_t a,unsigned n){return (uint64_t)a+n<=UINT64_C(0x100000000);}
static inline int skin_separate(uint32_t a,unsigned an,uint32_t b,unsigned bn){return (uint64_t)a+an<=b||(uint64_t)b+bn<=a;}
static inline int skin_bounded(uint32_t a,unsigned n){
 for(unsigned i=0;i<n;i++)if((MEM32(a+4*i)&0x7fffffffu)>0x4f800000u)return 0;
 return 1;
}
static inline int thing_skin_scalar_eligible(uint32_t vertex,uint32_t bone,uint32_t sp,uint32_t output){
 if(sp<8||!skin_range_ok(sp,140)||!skin_range_ok(output,24)||!skin_range_ok(vertex,12))return 0;
 uint32_t object=MEM32(sp+0x58);
 if(!skin_range_ok(object,64))return 0;
 uint32_t matrix=MEM32(object+0x3c)+(bone<<6),normal=MEM32(sp+0x88),weight=MEM32(sp+0x28);
 if(!skin_range_ok(matrix,60)||!skin_range_ok(normal,12)||!skin_range_ok(weight,4))return 0;
 uint32_t a[]={vertex,matrix,normal,weight};unsigned n[]={12,60,12,4};
 /* Preserve all sequential read/write alias behavior with original fallback.
  * stack includes dummy return addresses and all temporary writes. */
 if(!skin_separate(output,24,sp-8,148))return 0;
 for(unsigned i=0;i<4;i++)if(!skin_separate(a[i],n[i],output,24)||!skin_separate(a[i],n[i],sp-8,148))return 0;
 if(!skin_bounded(vertex,3)||!skin_bounded(normal,3)||!skin_bounded(weight,1)||!skin_bounded(output,6))return 0;
 for(unsigned row=0;row<4;row++)if(!skin_bounded(matrix+16*row,3))return 0;
 /* Each magnitude <=2^32: intermediate double sums and weighted float
  * results remain finite. Original arithmetic order and stores stay intact. */
 return 1;
}
#endif
