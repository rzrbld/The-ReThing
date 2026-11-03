/* Isolated x87 mapping oracle: instruction order from active original75D04D.
 * Does not patch or link game. x86_64 host only. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static const union{uint32_t u;float f;} inv={.u=0x3b808081},half={.u=0x3f000000},scale={.u=0x437f0000};
static unsigned channel(unsigned x,unsigned lane,unsigned cw,unsigned *status){
 int64_t src=x,dst;float spill;uint16_t old,work=cw,chop=cw|0xc00,sw;
 __asm__ volatile("fnstcw %0":"=m"(old));
 __asm__ volatile("fnclex; fldcw %0; fildq %1; fmuls %2; fmuls %3"::"m"(work),"m"(src),"m"(inv.f),"m"(half.f):"st");
 if(lane==2)__asm__ volatile("fstps %0; flds %0":"=m"(spill)::"st");
 __asm__ volatile("fsqrt":::"st");
 if(lane!=1)__asm__ volatile("fstps %0; flds %0":"=m"(spill)::"st");
 __asm__ volatile("fmuls %2; fldcw %3; fistpq %0; fnstsw %1; fldcw %4":"=m"(dst),"=m"(sw):"m"(scale.f),"m"(chop),"m"(old):"st");
 *status=sw;return dst<0?0:dst>255?255:(unsigned)dst;
}
int main(void){
 unsigned base[3][256],diff=0,lanediff=0,exceptions[64]={0};
 for(unsigned lane=0;lane<3;lane++)for(unsigned x=0;x<256;x++){unsigned sw;base[lane][x]=channel(x,lane,0x37f,&sw);}
 for(unsigned pc=0;pc<3;pc++)for(unsigned rc=0;rc<4;rc++)for(unsigned lane=0;lane<3;lane++)for(unsigned x=0;x<256;x++){
 unsigned sw,cw=0x7f|((pc==0?0:pc==1?2:3)<<8)|(rc<<10);unsigned y=channel(x,lane,cw,&sw);exceptions[sw&63]++;
 if(y!=base[lane][x]){if(diff<16)printf("DIFF pc=%u rc=%u lane=%u x=%u base=%u actual=%u\n",pc,rc,lane,x,base[lane][x],y);diff++;}}
 for(unsigned x=0;x<256;x++)if(base[0][x]!=base[1][x]||base[1][x]!=base[2][x])lanediff++;
 printf("cases=9216 rounding_precision_differences=%u channel_differences=%u\n",diff,lanediff);
 for(unsigned i=0;i<64;i++)if(exceptions[i])printf("exception_bits=0x%02x count=%u\n",i,exceptions[i]);
 printf("table=");for(unsigned x=0;x<256;x++)printf("%s%u",x?",":"",base[0][x]);puts("");return diff!=0||lanediff!=0;
}
