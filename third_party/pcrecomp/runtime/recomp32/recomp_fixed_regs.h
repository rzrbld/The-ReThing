/*
 * recomp_fixed_regs.h - C1: the guest GPR file in fixed AArch64 registers.
 *
 * Opt-in build mode for The ReThing (aarch64 only), selected with
 * scripts/build_game.py --fixed-regs (RECOMP_FIXED_REGS):
 *
 *   the eight guest general-purpose registers live in AArch64 callee-saved
 *   registers x19..x26 for the whole duration of lifted code, instead of the
 *   RECOMP_LOCAL_REGS model that copies them to/from the g_eax..g_ebp globals
 *   before every call and after every return. This removes the register half
 *   of the call-boundary cost.
 *
 * Requirements, enforced by scripts/build_game.py --fixed-regs:
 *   - every translation unit of the executable is compiled with
 *     -ffixed-x19 ... -ffixed-x26, so the host C compiler never allocates
 *     these registers for its own code;
 *   - this header is visible in every TU that names the registers. It is
 *     included from recomp_types.h, which win32hle.h and every lifted function
 *     pull in, so all TUs agree on where the register file lives;
 *   - RECOMP_LOCAL_REGS is NOT defined.
 *
 * The machine lock still snapshots/restores the registers at thread boundaries
 * (regs_save/regs_load in host_lite.c) and mach_enter/mach_leave additionally
 * preserve the host's full 64-bit x19..x26 (win32hle/host_regs.h) for native
 * callers outside the port, so libc/SDL/pthread code that re-enters the guest
 * sees its registers unchanged when the call returns. Libraries that call back
 * into the guest from inside a guest-initiated call (libthing-audio stream
 * callbacks) swap registers explicitly; see probe/audio/guest_stream_impl.h.
 *
 * Why global register variables are safe here:
 *   - no TU takes the address of g_eax..g_ebp (checked: no &g_eax style use in
 *     the tree), which C requires for a register variable;
 *   - writes still happen through the C variable, so all 8/16-bit
 *     read-modify-write sub-register macros keep working;
 *   - the compiler treats them as globals for call clobbering, so a value read
 *     after a call comes from the register the callee legitimately updated;
 *   - a call from native code back into the guest re-asserts its own x19..x26
 *     at mach_leave, satisfying the host ABI for external callers.
 */
#ifndef RECOMP_FIXED_REGS_H
#define RECOMP_FIXED_REGS_H

#if defined(RECOMP_FIXED_REGS) && defined(RECOMP_LOCAL_REGS)
#error "RECOMP_FIXED_REGS and RECOMP_LOCAL_REGS are mutually exclusive"
#endif

#if defined(RECOMP_FIXED_REGS) && defined(__aarch64__)
#include <stdint.h>

/* One C variable per guest GPR, bound to its AArch64 register. GCC inlines
 * every access to the W-register form, so `eax += k` becomes `add w19,w19,k`
 * with no load or store. */
register uint32_t g_eax asm("x19");
register uint32_t g_ecx asm("x20");
register uint32_t g_edx asm("x21");
register uint32_t g_esp asm("x22");
register uint32_t g_ebx asm("x23");
register uint32_t g_esi asm("x24");
register uint32_t g_edi asm("x25");
register uint32_t g_ebp asm("x26");

#define RECOMP_FIXED_REGS_ACTIVE 1
#endif

#endif /* RECOMP_FIXED_REGS_H */
