#ifndef GAME_SPATIAL_SEARCH_INDEX_H
#define GAME_SPATIAL_SEARCH_INDEX_H
#include <stdint.h>
#include <math.h>

/* Isolated game algorithm, not a guest ABI replacement. Reader must return the
 * immutable entry+0x14 binary32 key. Both versions intentionally expose read
 * counts/order through reader; binary search changes read order by design. */
typedef float (*game_spatial_key_reader)(void *context, uint32_t index);
struct game_spatial_search_counts { uint32_t probes, tail; };

static uint32_t game_spatial_original_index(uint32_t n, float t,
        game_spatial_key_reader read_key, void *ctx,
        struct game_spatial_search_counts *counts) {
    if (!n) return 0;
    uint32_t d=n/2, c;
    ++counts->probes;
    if (read_key(ctx,d)>=t) {
        d=n/4; c=n/8;
        ++counts->probes;
        if (read_key(ctx,d)>=t) {
            ++counts->probes;
            if (read_key(ctx,c)>=t) c=0;
        } else {
            c+=d;
            ++counts->probes;
            if (read_key(ctx,c)>=t) c=d;
        }
    } else {
        uint32_t f=n/4, b=f+d;
        c=n/8;
        ++counts->probes;
        if (read_key(ctx,b)>=t) {
            c+=d;
            ++counts->probes;
            if (read_key(ctx,c)>=t) c=d;
        } else {
            c+=f+d;
            ++counts->probes;
            if (read_key(ctx,c)>=t) c=b;
        }
    }
    if (c) --c;
    while (c<n) {
        ++counts->tail;
        /* x87 TEST AH,41 advances on less, equal AND unordered. */
        if (read_key(ctx,c)>t) break;
        ++c;
    }
    return c;
}

/* valid_generation MUST establish ascending keys, no NaN keys, and no writes
 * until this search returns. It is not inferred from pointers/counts. Until
 * maintenance/load invalidation is implemented, pass zero (exact fallback).
 * Inputs n must fit the original positive signed pointer-difference count. */
static uint32_t game_spatial_candidate_index(uint32_t n, float t,
        game_spatial_key_reader read_key, void *ctx,
        int valid_generation, struct game_spatial_search_counts *counts) {
    if (!valid_generation)
        return game_spatial_original_index(n,t,read_key,ctx,counts);
    if (isnan(t)) return n;
    uint32_t lo=0, hi=n;
    while (lo<hi) {
        uint32_t m=lo+(hi-lo)/2;
        ++counts->probes;
        if (read_key(ctx,m)<=t) lo=m+1;
        else hi=m;
    }
    return lo;
}
#endif
