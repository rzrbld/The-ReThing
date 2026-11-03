#define _GNU_SOURCE
#include <fenv.h>
#define RECOMP_GENERATED_CODE
#define RECOMP_LOCAL_REGS
#include "../../third_party/pcrecomp/runtime/recomp32/recomp_types.h"
uint32_t g_eax,g_ecx,g_edx,g_ebx,g_esp,g_esi,g_edi,g_ebp,g_cur_func;
double g_st[8];int g_fp_top;uint16_t g_fpu_cw;ptrdiff_t g_mem_base;
static uint32_t end_flags[4];
void recomp_thething_007B3480(void) {
    int _fpu_cmp = 0;
    uint32_t _cf = 0;  /* carry flag */
    int _df = 1;  /* direction flag (1=forward, -1=backward) */
    uint32_t _flag_a = 0, _flag_b = 0;  /* flag-operand snapshots */
    uint32_t _flag_k = FK_NONE;  /* which instruction wrote them */
    /* _st[8]/_fp_top/_fpu_cw are GLOBAL (shared x87 stack) */
    RECOMP_REGS_LOCALS
    RECOMP_ENTER(0x007B3480u);

L_007B3480:
    PUSH32(esp, ebp); /* 0x007B3480: push ebp */
    ebp = esp; /* 0x007B3481: mov ebp, esp */
    _flag_a = (uint32_t)(esp); _flag_b = (uint32_t)(0xFFFFFFF4u);
    _cf = (uint32_t)((uint32_t)(_flag_a + _flag_b) < (uint32_t)_flag_a);
    esp = esp + 0xFFFFFFF4u; /* 0x007B3483: add esp, -0xc */
    _flag_k = FK_ADD;
    /* fwait */ /* 0x007B3486: wait  */
    MEM16(ebp + (-0x2)) = _fpu_cw; /* 0x007B3487: fnstcw word ptr [ebp - 2] */
    /* fwait */ /* 0x007B348A: wait  */
    SET_LO16(eax, MEM16(ebp + (-0x2))); /* 0x007B348B: mov ax, word ptr [ebp - 2] */
    _flag_a = (uint32_t)((HI8(eax) | 0xCu)) << 24; _flag_b = (uint32_t)((HI8(eax) | 0xCu)) << 24;
    SET_HI8(eax, HI8(eax) | 0xCu); /* 0x007B348F: or ah, 0xc */
    _cf = 0;
    _flag_k = FK_NARROW(FK_TEST, 24);
    MEM16(ebp + (-0x4)) = (uint16_t)(LO16(eax)); /* 0x007B3492: mov word ptr [ebp - 4], ax */
    _fpu_cw = MEM16(ebp + (-0x4)); /* 0x007B3496: fldcw word ptr [ebp - 4] */
    { int64_t _q = fp_st0_to_i64(FP_ST(0), FP_SI(0), _fpu_cw); (void)fp_pop(); MEM64(ebp + (-0xC)) = (uint64_t)_q; } /* 0x007B3499: fistp qword ptr [ebp - 0xc] */
    _fpu_cw = MEM16(ebp + (-0x2)); /* 0x007B349C: fldcw word ptr [ebp - 2] */
    eax = MEM32(ebp + (-0xC)); /* 0x007B349F: mov eax, dword ptr [ebp - 0xc] */
    edx = MEM32(ebp + (-0x8)); /* 0x007B34A2: mov edx, dword ptr [ebp - 8] */
    esp = ebp; ebp = POP32_VAL(esp); /* 0x007B34A5: leave  */
    RECOMP_FLAGS_OUT(); esp += 4; { RECOMP_REGS_OUT(); return; } /* 0x007B34A6: ret  */

    RECOMP_REGS_OUT(); /* checked fallthrough flush */
}void recomp_thething_005B78B0(void) {
    int _fpu_cmp = 0;
    uint32_t _cf = 0;  /* carry flag */
    int _df = 1;  /* direction flag (1=forward, -1=backward) */
    uint32_t _flag_a = 0, _flag_b = 0;  /* flag-operand snapshots */
    uint32_t _flag_k = FK_NONE;  /* which instruction wrote them */
    /* _st[8]/_fp_top/_fpu_cw are GLOBAL (shared x87 stack) */
    RECOMP_REGS_LOCALS
    RECOMP_ENTER(0x005B78B0u);

L_005B78B0:
    /* test ecx, ecx */ /* 0x005B78B0: test ecx, ecx */
    _flag_a = (uint32_t)(ecx); _flag_b = (uint32_t)(ecx);
    _cf = 0;
    _flag_k = FK_TEST;
    eax = edx; /* 0x005B78B2: mov eax, edx */
    if (TEST_S(_flag_a, _flag_b)) goto L_005B78BD; /* 0x005B78B4: jl 0x5b78bd */

L_005B78B6:
    /* cmp ecx, eax */ /* 0x005B78B6: cmp ecx, eax */
    _flag_a = (uint32_t)(ecx); _flag_b = (uint32_t)(eax);
    _cf = (uint32_t)CMP_B(_flag_a, _flag_b);
    _flag_k = FK_CMP;
    if (CMP_G(_flag_a, _flag_b)) goto L_005B78BF; /* 0x005B78B8: jg 0x5b78bf */

L_005B78BA:
    eax = ecx; /* 0x005B78BA: mov eax, ecx */
    RECOMP_FLAGS_OUT(); esp += 4; { RECOMP_REGS_OUT(); return; } /* 0x005B78BC: ret  */

L_005B78BD:
    _flag_a = (uint32_t)(0); _flag_b = (uint32_t)(0);
    eax = 0; /* 0x005B78BD: xor eax, eax */
    _cf = 0;
    _flag_k = FK_TEST;

L_005B78BF:
    RECOMP_FLAGS_OUT(); esp += 4; { RECOMP_REGS_OUT(); return; } /* 0x005B78BF: ret  */

    RECOMP_REGS_OUT(); /* checked fallthrough flush */
}static void original(void){
 uint32_t _cf=0,_flag_a=0,_flag_b=0,_flag_k=FK_NONE;
 RECOMP_REGS_LOCALS
 RECOMP_FLAGS_IN();
L_0075D04D:
    eax = MEM32(edi + 0x44); /* 0x0075D04D: mov eax, dword ptr [edi + 0x44] */
    edx = eax; /* 0x0075D050: mov edx, eax */
    if (16u) _cf = (((edx) >> 15) & 1u); /* 0x0075D052: shr edx, 0x10 */
    if (16u) { _flag_a = recomp_flags_pack((edx >> 16u), _cf, 0, 0); _flag_b = 0; _flag_k = FK_EFLAGS; }
    edx = edx >> 16u; /* 0x0075D052: shr edx, 0x10 */
    _flag_a = (uint32_t)(edx); _flag_b = (uint32_t)(0xFFu);
    edx = edx & 0xFFu; /* 0x0075D055: and edx, 0xff */
    _cf = 0;
    _flag_k = FK_TEST;
    MEM32(esp + 0x20) = edx; /* 0x0075D05B: mov dword ptr [esp + 0x20], edx */
    _flag_a = (uint32_t)(0); _flag_b = (uint32_t)(0);
    ecx = 0; /* 0x0075D05F: xor ecx, ecx */
    _cf = 0;
    _flag_k = FK_TEST;
    edx = eax; /* 0x0075D061: mov edx, eax */
    MEM32(esp + 0x24) = ecx; /* 0x0075D063: mov dword ptr [esp + 0x24], ecx */
    fp_push_i64((int64_t)MEM64(esp + 0x20)); /* 0x0075D067: fild qword ptr [esp + 0x20] */
    if (8u) _cf = (((edx) >> 7) & 1u); /* 0x0075D06B: shr edx, 8 */
    if (8u) { _flag_a = recomp_flags_pack((edx >> 8u), _cf, 0, 0); _flag_b = 0; _flag_k = FK_EFLAGS; }
    edx = edx >> 8u; /* 0x0075D06B: shr edx, 8 */
    _flag_a = (uint32_t)(edx); _flag_b = (uint32_t)(0xFFu);
    edx = edx & 0xFFu; /* 0x0075D06E: and edx, 0xff */
    _cf = 0;
    _flag_k = FK_TEST;
    MEM32(esp + 0x40) = edx; /* 0x0075D074: mov dword ptr [esp + 0x40], edx */
    FP_ST(0) *= (double)*(float*)ADDR(0x844AA0); /* 0x0075D078: fmul dword ptr [0x844aa0] */
    MEM32(esp + 0x44) = ecx; /* 0x0075D07E: mov dword ptr [esp + 0x44], ecx */
    fp_push_i64((int64_t)MEM64(esp + 0x40)); /* 0x0075D082: fild qword ptr [esp + 0x40] */
    _flag_a = (uint32_t)(eax); _flag_b = (uint32_t)(0xFFu);
    eax = eax & 0xFFu; /* 0x0075D086: and eax, 0xff */
    _cf = 0;
    _flag_k = FK_TEST;
    MEM32(esp + 0x28) = eax; /* 0x0075D08B: mov dword ptr [esp + 0x28], eax */
    MEM32(esp + 0x2C) = ecx; /* 0x0075D08F: mov dword ptr [esp + 0x2c], ecx */
    FP_ST(0) *= (double)*(float*)ADDR(0x844AA0); /* 0x0075D093: fmul dword ptr [0x844aa0] */
    fp_push_i64((int64_t)MEM64(esp + 0x28)); /* 0x0075D099: fild qword ptr [esp + 0x28] */
    FP_ST(0) *= (double)*(float*)ADDR(0x844AA0); /* 0x0075D09D: fmul dword ptr [0x844aa0] */
    FP_ST(0) *= (double)*(float*)ADDR(0x84177C); /* 0x0075D0A3: fmul dword ptr [0x84177c] */
    { float _v = (float)fp_pop(); *(float*)ADDR(esp + 0x14) = _v; } /* 0x0075D0A9: fstp dword ptr [esp + 0x14] */
    FP_ST(0) *= (double)*(float*)ADDR(0x84177C); /* 0x0075D0AD: fmul dword ptr [0x84177c] */
    FP_ST(0) = sqrt(FP_ST(0)); /* 0x0075D0B3: fsqrt  */
    fp_xch(1); /* 0x0075D0B5: fxch st(1) */
    FP_ST(0) *= (double)*(float*)ADDR(0x84177C); /* 0x0075D0B7: fmul dword ptr [0x84177c] */
    FP_ST(0) = sqrt(FP_ST(0)); /* 0x0075D0BD: fsqrt  */
    { float _v = (float)fp_pop(); *(float*)ADDR(esp + 0x10) = _v; } /* 0x0075D0BF: fstp dword ptr [esp + 0x10] */
    fp_push(*(float*)&MEM32(esp + 0x14)); /* 0x0075D0C3: fld dword ptr [esp + 0x14] */
    FP_ST(0) = sqrt(FP_ST(0)); /* 0x0075D0C7: fsqrt  */
    { float _v = (float)fp_pop(); *(float*)ADDR(esp + 0x14) = _v; } /* 0x0075D0C9: fstp dword ptr [esp + 0x14] */
    FP_ST(0) *= (double)*(float*)ADDR(0x851DB8); /* 0x0075D0CD: fmul dword ptr [0x851db8] */
    RECOMP_CALL(recomp_thething_007B3480); /* 0x0075D0D3: call 0x7b3480 */
    RECOMP_FLAGS_IN();
    edx = 0xFFu; /* 0x0075D0D8: mov edx, 0xff */
    ecx = eax; /* 0x0075D0DD: mov ecx, eax */
    RECOMP_CALL(recomp_thething_005B78B0); /* 0x0075D0DF: call 0x5b78b0 */
    RECOMP_FLAGS_IN();
    fp_push(*(float*)&MEM32(esp + 0x14)); /* 0x0075D0E4: fld dword ptr [esp + 0x14] */
    FP_ST(0) *= (double)*(float*)ADDR(0x851DB8); /* 0x0075D0E8: fmul dword ptr [0x851db8] */
    MEM32(esp + 0x1C) = eax; /* 0x0075D0EE: mov dword ptr [esp + 0x1c], eax */
    RECOMP_CALL(recomp_thething_007B3480); /* 0x0075D0F2: call 0x7b3480 */
    RECOMP_FLAGS_IN();
    edx = 0xFFu; /* 0x0075D0F7: mov edx, 0xff */
    ecx = eax; /* 0x0075D0FC: mov ecx, eax */
    RECOMP_CALL(recomp_thething_005B78B0); /* 0x0075D0FE: call 0x5b78b0 */
    RECOMP_FLAGS_IN();
    fp_push(*(float*)&MEM32(esp + 0x10)); /* 0x0075D103: fld dword ptr [esp + 0x10] */
    FP_ST(0) *= (double)*(float*)ADDR(0x851DB8); /* 0x0075D107: fmul dword ptr [0x851db8] */
    MEM32(esp + 0x14) = eax; /* 0x0075D10D: mov dword ptr [esp + 0x14], eax */
    RECOMP_CALL(recomp_thething_007B3480); /* 0x0075D111: call 0x7b3480 */
    RECOMP_FLAGS_IN();
    edx = 0xFFu; /* 0x0075D116: mov edx, 0xff */
    ecx = eax; /* 0x0075D11B: mov ecx, eax */
    RECOMP_CALL(recomp_thething_005B78B0); /* 0x0075D11D: call 0x5b78b0 */
    RECOMP_FLAGS_IN();
    ecx = MEM32(esp + 0x1C); /* 0x0075D122: mov ecx, dword ptr [esp + 0x1c] */
    _flag_a = (uint32_t)((eax | 0xFFFFFF00u)); _flag_b = (uint32_t)((eax | 0xFFFFFF00u));
    eax = eax | 0xFFFFFF00u; /* 0x0075D126: or eax, 0xffffff00 */
    _cf = 0;
    _flag_k = FK_TEST;
    if (8u) _cf = (((eax) >> 24) & 1u); /* 0x0075D12B: shl eax, 8 */
    if (8u) { _flag_a = recomp_flags_pack((eax << 8u), _cf, 0, 0); _flag_b = 0; _flag_k = FK_EFLAGS; }
    eax = eax << 8u; /* 0x0075D12B: shl eax, 8 */
    _flag_a = (uint32_t)((eax | ecx)); _flag_b = (uint32_t)((eax | ecx));
    eax = eax | ecx; /* 0x0075D12E: or eax, ecx */
    _cf = 0;
    _flag_k = FK_TEST;
    ecx = MEM32(esp + 0x14); /* 0x0075D130: mov ecx, dword ptr [esp + 0x14] */
    if (8u) _cf = (((eax) >> 24) & 1u); /* 0x0075D134: shl eax, 8 */
    if (8u) { _flag_a = recomp_flags_pack((eax << 8u), _cf, 0, 0); _flag_b = 0; _flag_k = FK_EFLAGS; }
    eax = eax << 8u; /* 0x0075D134: shl eax, 8 */
    _flag_a = (uint32_t)((eax | ecx)); _flag_b = (uint32_t)((eax | ecx));
    eax = eax | ecx; /* 0x0075D137: or eax, ecx */
    _cf = 0;
    _flag_k = FK_TEST;
    MEM32(edi + 0x44) = eax; /* 0x0075D139: mov dword ptr [edi + 0x44], eax */


 end_flags[0]=_flag_k;end_flags[1]=_flag_a;end_flags[2]=_flag_b;end_flags[3]=_cf;
 RECOMP_REGS_OUT();
}
