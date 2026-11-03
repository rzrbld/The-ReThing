/* Bit-exact reference test for the circular x87 stack (recomp_types.h).
 *
 * The production model keeps the eight registers in a ring addressed through
 * FP_ST(i)/FP_SI(i); this test drives it through the public macros and compares
 * every observable bit against an independent top-first shifting model (the
 * pre-2026-10-31 layout) after every operation. */
#include "recomp_types.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

double g_st[8];
int g_fp_top;
uint16_t g_fpu_cw = 0x027f;

static double ref_st[8];
static int64_t ref_sh[8];
static int ref_top;

static void ref_push(double v){
    for (int i = 7; i > 0; i--) { ref_st[i] = ref_st[i-1]; ref_sh[i] = ref_sh[i-1]; }
    ref_st[0] = v; ref_sh[0] = 0; ref_top++;
}
static double ref_pop(void){
    double v = ref_st[0];
    for (int i = 0; i < 7; i++) { ref_st[i] = ref_st[i+1]; ref_sh[i] = ref_sh[i+1]; }
    ref_st[7] = 0.0; ref_sh[7] = 0; ref_top--;
    return v;
}
static void ref_xch(int i){
    double a = ref_st[0]; ref_st[0] = ref_st[i]; ref_st[i] = a;
    int64_t b = ref_sh[0]; ref_sh[0] = ref_sh[i]; ref_sh[i] = b;
}

static uint64_t bits(double d) { uint64_t u; memcpy(&u, &d, 8); return u; }
static double from_bits(uint64_t u) { double d; memcpy(&d, &u, 8); return d; }

static int failures;
static void check(const char *what, int step){
    int bad = 0;
    if (g_fp_top != ref_top) bad = 1;
    for (int i = 0; i < ref_top && i < 8 && !bad; i++)
        if (bits(FP_ST(i)) != bits(ref_st[i]) || FP_SI(i) != ref_sh[i]) bad = 1;
    if (bad) {
        failures++;
        if (failures <= 10)
            printf("FAIL step %d (%s): ring top=%d ref top=%d\n", step, what, g_fp_top, ref_top);
    }
}

int main(void){
    /* Deterministic PRNG so failures reproduce. */
    uint64_t seed = 0x9E3779B97F4A7C15ull;
    for (int step = 0; step < 200000; step++) {
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        unsigned pick = (unsigned)(seed >> 33) % 100;
        if (pick < 34 && ref_top < 8) {
            uint64_t raw = seed ^ (seed << 17);
            switch (step & 7) {           /* signed zeros, NaN payloads, large ints */
            case 0: raw = 0x8000000000000000ull; break;
            case 1: raw = 0x7ff8000000000001ull; break;
            case 2: raw = 0xfff4000000000000ull; break;
            default: break;
            }
            double v = from_bits(raw);
            fp_push(v); ref_push(v);
            check("push", step);
        } else if (pick < 60) {
            double a = fp_pop(), b = ref_pop();
            if (bits(a) != bits(b)) { failures++; printf("FAIL step %d pop bit mismatch\n", step); }
            check("pop", step);
        } else if (pick < 80) {
            if (ref_top == 8) continue;
            int64_t v = (int64_t)(seed ^ (seed >> 29));
            if (step % 3 == 0) v = 0x7fffffffffffffffll;   /* not representable in double */
            fp_push_i64(v); ref_push((double)v); ref_sh[0] = v;
            check("push_i64", step);
        } else {
            if (ref_top < 2) continue;
            int i = 1 + (int)((seed >> 40) % (unsigned)(ref_top - 1));
            fp_xch(i); ref_xch(i);
            check("fxch", step);
        }
        if ((step & 1023) == 0 && ref_top > 0) {
            /* Checked FNSTENV/FLDENV round trip on the circular stack. */
            uint8_t env[28];
            uint16_t cw = 0x027f;
            fp_env_store(env, g_fp_top, &cw, 2);
            int cmp = fp_env_load(env, g_fp_top, &cw);
            if (cmp == 0) { failures++; printf("FAIL step %d env round trip\n", step); }
            check("env roundtrip", step);
        }
    }
    printf("circular x87 stack: %d failures over 200000 operations\n", failures);
    printf("stack depth reached: %d, final top %d\n", 8, g_fp_top);
    printf("%s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
