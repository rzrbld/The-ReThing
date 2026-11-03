#ifndef THING_PROVEN_FLAGS_CALL_H
#define THING_PROVEN_FLAGS_CALL_H
/* Only emitted for callees with a verified flag-killing entry prefix.
 * All register transfer, guest stack and returned flags remain unchanged. */
#define RECOMP_CALL_FLAGS_KILLED(func) do { \
    uint32_t _caller = g_cur_func; \
    PUSH32(esp, RECOMP_RETADDR); \
    RECOMP_REGS_OUT(); \
    func(); \
    RECOMP_REGS_IN(); \
    g_cur_func = _caller; \
} while(0)
#endif
