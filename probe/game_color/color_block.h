/* Exact translated-state replacement for original75D04D..75D13C.
 * Caller flushes local regs, calls this, reloads regs and restores the returned
 * LOCAL flag tuple when it returns1; on0 execute unchanged original block.
 * Global returned-callee flags are intentionally distinct from local flags.
 * No guest FP arithmetic is changed outside this finite-byte color algorithm.
 */
#ifndef THING_GAME_COLOR_BLOCK_H
#define THING_GAME_COLOR_BLOCK_H
#include <fenv.h>
#include "color_lut.h"
#if defined(__GLIBC__)
extern int fegetexcept(void);
#endif
static inline int thing_game_color_block(uint32_t out_flags[4]) {
#if !defined(__GLIBC__)
 (void)out_flags;return 0; /* host exception-mask inspection unverified */
#else
 if((g_fpu_cw&63)!=63 || g_fp_top<0 || g_fp_top>5 || fegetround()!=FE_TONEAREST || fegetexcept())return 0;
 unsigned c=MEM32(g_edi+0x44),r=(c>>16)&255,g=(c>>8)&255,b=c&255;
 unsigned rr=color_byte[r],gg=color_byte[g],bb=color_byte[b];
 MEM32(g_esp-4)=RECOMP_RETADDR;
 MEM32(g_esp-8)=g_ebp;
 MEM16(g_esp-10)=g_fpu_cw;
 MEM16(g_esp-12)=g_fpu_cw|0xc00;
 MEM64(g_esp-20)=rr;
 MEM32(g_esp+0x10)=red_root_bits[r];
 MEM32(g_esp+0x14)=bb;
 MEM32(g_esp+0x1c)=gg;
 MEM32(g_esp+0x20)=r;MEM32(g_esp+0x24)=0;
 MEM32(g_esp+0x28)=b;MEM32(g_esp+0x2c)=0;
 MEM32(g_esp+0x40)=g;MEM32(g_esp+0x44)=0;
 for(unsigned i=1;i<=3;i++){unsigned slot=(8u-(g_fp_top+i))&7u;g_st[slot]=0.;g_st_i64[slot]=0;}
 g_eax=0xff000000|(rr<<16)|(gg<<8)|bb;g_ecx=bb;g_edx=255;
 MEM32(g_edi+0x44)=g_eax;
 /* Calls publish last clamp's lazy flags; caller's final OR stays local. */
 g_flag_k=FK_CMP;g_flag_a=rr;g_flag_b=255;g_flag_cf=1;
 out_flags[0]=FK_TEST;out_flags[1]=g_eax;out_flags[2]=g_eax;out_flags[3]=0;
 if(r|g|b)feraiseexcept(FE_INEXACT);
 return 1;
#endif
}
#endif
