/* Exact render-state redundancy predicate for the original material prefix.
 * Does not modify device state or skip textures/resources. */
#ifndef THING_MATERIAL_STATE_REUSE_H
#define THING_MATERIAL_STATE_REUSE_H
#include <stdint.h>
static inline int thing_material_prefix_matches(const uint8_t *m,const uint32_t rs[256]){
 /* Keep the original final FILLMODE setter, which restores scratch registers. */
 if(m[0x13]>1)return 0;
 #define MG_EQ(k,v) do{if(rs[(k)]!=(unsigned)(v))return 0;}while(0)
 MG_EQ(7,m[0x12]!=0);
 if(m[0x12]){static const unsigned depth[]={1,8,4,2};MG_EQ(14,m[0x14]);if(m[0x17]<4)MG_EQ(23,depth[m[0x17]]);}
 MG_EQ(15,m[0x15]!=0);
 if(m[0x15]){static const unsigned alpha[]={1,8,2,4,3,7,5,6};MG_EQ(24,m[0x18]);if(m[0x19]<8)MG_EQ(25,alpha[m[0x19]]);}
 MG_EQ(27,m[0x1b]!=0);
 if(m[0x1b]&&m[0x1c]<10){static const unsigned src[]={5,2,1,5,1,2,1,2,1,2},dst[]={6,2,1,2,6,6,3,3,4,4};MG_EQ(19,src[m[0x1c]]);MG_EQ(20,dst[m[0x1c]]);}
 MG_EQ(28,m[0x1d]!=0);MG_EQ(140,m[0x1d]?3:0);MG_EQ(47,m[0x1e]);MG_EQ(137,m[0x1f]);
 if(m[0x1b]||m[0x16]==0)MG_EQ(22,1);else if(m[0x16]<=2)MG_EQ(22,m[0x16]+1);
 #undef MG_EQ
 return 1;
}
#endif
