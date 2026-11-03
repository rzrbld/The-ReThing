#include "search_index.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct source { const float *a; uint32_t n, reads; uint64_t trace; };
static float key(void *p, uint32_t i) {
    struct source *s=p;
    assert(i<s->n);
    ++s->reads;
    s->trace=s->trace*UINT64_C(1099511628211)+i+1;
    return s->a[i];
}
static uint64_t cases;
static void compare(const float *a,uint32_t n,float t,int valid) {
    struct source x={a,n,0,0}, y=x;
    struct game_spatial_search_counts oc={0},nc={0};
    uint32_t old=game_spatial_original_index(n,t,key,&x,&oc);
    uint32_t neu=game_spatial_candidate_index(n,t,key,&y,valid,&nc);
    assert(old==neu);
    if (!valid) {
        assert(x.reads==y.reads && x.trace==y.trace);
        assert(oc.probes==nc.probes && oc.tail==nc.tail);
    } else {
        uint32_t expected=0;
        while (expected<n && !(a[expected]>t)) ++expected;
        assert(neu==expected);
    }
    ++cases;
}
static uint32_t rng=0x714c1a0;
static uint32_t random_u32(void) {
    rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng;
}
static float bits_float(uint32_t b) { float f; memcpy(&f,&b,4); return f; }
static int cmp_float(const void *a,const void *b) {
    float x=*(const float *)a,y=*(const float *)b;
    return (x>y)-(x<y);
}
static void enumerate(float *a,uint32_t n,uint32_t i,int minimum) {
    if(i==n) {
        const float ts[]={-1,0,.5,1,1.5,2,2.5,3,4};
        for(unsigned j=0;j<sizeof(ts)/sizeof(ts[0]);++j) compare(a,n,ts[j],1);
        return;
    }
    for(int k=minimum;k<4;++k) { a[i]=(float)k; enumerate(a,n,i+1,k); }
}
int main(void) {
    float a[4096];
    for(uint32_t n=0;n<=17;++n) enumerate(a,n,0,0);
    const float edges[]={-INFINITY,-0x1p-149f,-0.0f,0.0f,0x1p-149f,INFINITY,NAN};
    const float sorted[]={-INFINITY,-1,-0x1p-149f,-0.0f,0.0f,0x1p-149f,1,INFINITY};
    for(unsigned j=0;j<7;++j) compare(sorted,8,edges[j],1);
    for(uint32_t n=0;n<4096;++n) {
        for(uint32_t i=0;i<n;++i) a[i]=(float)(random_u32()%13);
        for(unsigned j=0;j<7;++j) compare(a,n,edges[j],0);
        if(n) a[random_u32()%n]=NAN;
        compare(a,n,5,0); /* NaN and unsorted arrays retain original reads. */
        for(uint32_t i=0;i<n;++i) {
            do a[i]=bits_float(random_u32()); while(isnan(a[i]));
        }
        qsort(a,n,sizeof(float),cmp_float);
        for(unsigned j=0;j<7;++j) compare(a,n,edges[j],1);
        if(n) compare(a,n,a[random_u32()%n],1);
    }
    for(uint32_t i=0;i<4096;++i) a[i]=42;
    compare(a,4096,42,1);
    struct source x={a,4096,0,0},y=x;
    struct game_spatial_search_counts oc={0},nc={0};
    game_spatial_original_index(4096,42,key,&x,&oc);
    game_spatial_candidate_index(4096,42,key,&y,1,&nc);
    printf("PASS %llu index comparisons; duplicate N=4096: original=%u reads (%u tail), candidate=%u reads\n",
           (unsigned long long)cases,x.reads,oc.tail,y.reads);
    puts("Index model only; no guest ABI/state or runtime speed equivalence claimed.");
}
