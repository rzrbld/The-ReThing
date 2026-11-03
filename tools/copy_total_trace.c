/* LD_PRELOAD diagnostic: total libc copy bytes/calls for the whole process,
 * exported to the game's SIGUSR1 profile flush as thing_copy_totals().
 * Diagnostic only (adds a counter per call); never part of the shipped build. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *(*real_memmove)(void *, const void *, size_t);
static void *(*real_memcpy)(void *, const void *, size_t);
static void *(*real_memset)(void *, int, size_t);
static uint64_t mm_calls, mm_bytes, mc_calls, mc_bytes, ms_calls, ms_bytes;

__attribute__((constructor)) static void init(void) {
    real_memmove = dlsym(RTLD_NEXT, "memmove");
    real_memcpy = dlsym(RTLD_NEXT, "memcpy");
    real_memset = dlsym(RTLD_NEXT, "memset");
}
static void *slow_copy(void *d, const void *s, size_t n) {
    unsigned char *dp = d; const unsigned char *sp = s;
    while (n--) *dp++ = *sp++;
    return d;
}
void *memmove(void *d, const void *s, size_t n) {
    if (!real_memmove) real_memmove = dlsym(RTLD_NEXT, "memmove");
    mm_calls++; mm_bytes += n;
    return real_memmove ? real_memmove(d, s, n) : slow_copy(d, s, n);
}
void *memcpy(void *d, const void *s, size_t n) {
    if (!real_memcpy) real_memcpy = dlsym(RTLD_NEXT, "memcpy");
    mc_calls++; mc_bytes += n;
    return real_memcpy ? real_memcpy(d, s, n) : slow_copy(d, s, n);
}
void *memset(void *d, int c, size_t n) {
    if (!real_memset) real_memset = dlsym(RTLD_NEXT, "memset");
    ms_calls++; ms_bytes += n;
    return real_memset ? real_memset(d, c, n) : d;
}
__attribute__((destructor)) static void dump(void) {
    if (!getenv("THING_COPY_TRACE_DUMP")) return;
    fprintf(stderr, "[copy-preload] memmove calls=%llu bytes=%llu memcpy calls=%llu bytes=%llu memset calls=%llu bytes=%llu\n",
        (unsigned long long)mm_calls, (unsigned long long)mm_bytes,
        (unsigned long long)mc_calls, (unsigned long long)mc_bytes,
        (unsigned long long)ms_calls, (unsigned long long)ms_bytes);
}
/* Called by runtime/d3d8.c when present (LD_PRELOAD active). */
void thing_copy_totals(uint64_t *mmc, uint64_t *mmb, uint64_t *mcc, uint64_t *mcb, uint64_t *msc, uint64_t *msb) {
    *mmc = mm_calls; *mmb = mm_bytes; *mcc = mc_calls; *mcb = mc_bytes; *msc = ms_calls; *msb = ms_bytes;
}
