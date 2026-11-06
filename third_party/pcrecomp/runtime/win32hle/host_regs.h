/*
 * host_regs.h - C1 (fixed guest registers): full 64-bit save/restore of
 * x19..x26 for callers that are NOT lifted/runtime code (libc, SDL, pthread,
 * dlopen'ed libraries). Their callee-saved values are host pointers, so going
 * through the 32-bit g_eax..g_ebp register variables would truncate them.
 * p points to uint64_t[8]. Only meaningful when RECOMP_FIXED_REGS_ACTIVE.
 */
#ifndef HOST_REGS_H
#define HOST_REGS_H
#include <stdint.h>
#define RECOMP_HOST_REGS_SAVE(p) __asm__ volatile( \
    "stp x19, x20, [%0]\n\tstp x21, x22, [%0, #16]\n\t" \
    "stp x23, x24, [%0, #32]\n\tstp x25, x26, [%0, #48]" :: "r"(p) : "memory")
#define RECOMP_HOST_REGS_LOAD(p) __asm__ volatile( \
    "ldp x19, x20, [%0]\n\tldp x21, x22, [%0, #16]\n\t" \
    "ldp x23, x24, [%0, #32]\n\tldp x25, x26, [%0, #48]" :: "r"(p) : "memory")
#endif
