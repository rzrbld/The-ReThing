#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
typedef void (*recomp_func_t)(void);
#define SHIM_BASE 0x7F000000u
static unsigned imports,lookups,guards,pending;
static void guest(void){} static void imported(void){}
__attribute__((noinline)) recomp_func_t recomp_lookup_import(uint32_t va){imports++;if(va>=SHIM_BASE&&va<SHIM_BASE+128&&(va-SHIM_BASE)%16==0){pending=(va-SHIM_BASE)/16+1;return imported;}return NULL;}
__attribute__((noinline)) recomp_func_t recomp_lookup(uint32_t va){lookups++;return (va&15)==0?guest:NULL;}
void ds_guard_dispatch(uint32_t va){guards++;pending^=va;}
recomp_func_t reference(uint32_t va) {
    extern void ds_guard_dispatch(uint32_t) __attribute__((weak));
    if(ds_guard_dispatch) {
        /* Resolve once on strict hosts. Imports occupy a direct-indexed range;
         * avoid two full guest-table searches and a linear shim-name scan on
         * every D3D/DirectInput call. Unknown targets still use the guard. */
        recomp_func_t fn=recomp_lookup_import(va);
        if(!fn)fn=recomp_lookup(va);
        if(fn)return fn;
        ds_guard_dispatch(va);
    }
    return NULL;
}
recomp_func_t candidate(uint32_t va) {
    extern void ds_guard_dispatch(uint32_t) __attribute__((weak));
    if(ds_guard_dispatch) {
        /* Resolve once on strict hosts. Imports occupy a direct-indexed range;
         * avoid two full guest-table searches and a linear shim-name scan on
         * every D3D/DirectInput call. Unknown targets still use the guard. */
        /* Ordinary guest VAs cannot be synthetic imports. Avoid an out-of-line
         * failed import lookup on every virtual guest call; still use the full
         * import validator for all upper-range addresses and preserve fallback. */
        recomp_func_t fn=va>=SHIM_BASE?recomp_lookup_import(va):NULL;
        if(!fn)fn=recomp_lookup(va);
        if(fn)return fn;
        ds_guard_dispatch(va);
    }
    return NULL;
}

int main(void){
 uint32_t values[]={0,1,0x400000,0x402800,0x7ae710,0x10000000,0x11000000,0x7effffff,SHIM_BASE,SHIM_BASE+1,SHIM_BASE+16,SHIM_BASE+127,SHIM_BASE+128,0xffffffff};
 unsigned avoided=0;
 for(unsigned j=0;j<100000;j++){
  uint32_t va=j<sizeof(values)/sizeof(*values)?values[j]:(j*2654435761u);
  imports=lookups=guards=0;pending=123;
  recomp_func_t expected=reference(va);unsigned ri=imports,rl=lookups,rg=guards,rp=pending;
  imports=lookups=guards=0;pending=123;
  recomp_func_t actual=candidate(va);
  assert(expected==actual&&rl==lookups&&rg==guards&&rp==pending);
  assert(imports==(va<SHIM_BASE?0:ri));avoided+=ri-imports;
 }
 printf("PASS 100000 dispatch equivalence cases: targets, fallback, guard and shim state; %u unnecessary import lookups removed\n",avoided);
}
