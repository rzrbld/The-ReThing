#ifndef THING_LOCAL_FP_TOP_H
#define THING_LOCAL_FP_TOP_H
/* Opt-in generated bodies only. Values/shadows stay shared; top is published
 * before every guest transfer and explicit return, and reloaded after calls. */
#define THING_FP_ST(i) (g_st[((8u-(unsigned)_thing_fp_top)+(unsigned)(i))&7u])
#define THING_FP_SI(i) (g_st_i64[((8u-(unsigned)_thing_fp_top)+(unsigned)(i))&7u])
static inline void thing_fp_push(int *top,double value) {
 unsigned next=(unsigned)*top+1u;*top=(int)next;
 unsigned slot=(8u-next)&7u;g_st[slot]=value;g_st_i64[slot]=0;
}
static inline double thing_fp_pop(int *top) {
 unsigned old=(unsigned)*top,slot=(8u-old)&7u;double value=g_st[slot];
 g_st[slot]=0;g_st_i64[slot]=0;*top=(int)(old-1u);return value;
}
#define THING_FP_XCH(i) do { double _t=THING_FP_ST(0); \
 THING_FP_ST(0)=THING_FP_ST(i);THING_FP_ST(i)=_t; \
 int64_t _ti=THING_FP_SI(0);THING_FP_SI(0)=THING_FP_SI(i);THING_FP_SI(i)=_ti; } while(0)
#define THING_FP_CALL(fn) do { g_fp_top=_thing_fp_top; RECOMP_CALL(fn); _thing_fp_top=g_fp_top; } while(0)
#define THING_FP_ICALL(va) do { g_fp_top=_thing_fp_top; RECOMP_ICALL(va); _thing_fp_top=g_fp_top; } while(0)
#endif
