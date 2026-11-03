/* More than 256 independently retained sound buffers, shared PCM duplication,
 * metadata growth, slot reuse and Notify ownership. Host dummy audio only. */
#include <SDL.h>
#include <stdio.h>
#include "guest_test_harness.h"
extern const win32hle_shim ds_dsound8_shims[];
static const uint8_t notify_iid[16]={0x83,0x07,0x21,0xb0,0xcd,0x89,0xd0,0x11,0xaf,0x08,0,0xa0,0xc9,0x25,0xcd,0x16};
static uint32_t make_buffer(uint32_t ds){
    uint32_t d=hle_scratch(36),f=hle_scratch(18),o=hle_scratch(4);
    MEM32(d)=36;MEM32(d+4)=0x10188;MEM32(d+8)=64;MEM32(d+16)=f;
    MEM16(f)=1;MEM16(f+2)=1;MEM32(f+4)=22050;MEM32(f+8)=44100;MEM16(f+12)=2;MEM16(f+14)=16;
    uint32_t a[]={d,o,0};assert(!invoke(ds,3,3,a));return MEM32(o);
}
int main(void){
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);g_mem_base=(ptrdiff_t)calloc(1,0x400000);assert(g_mem_base);
    win32hle_register(win32hle_dsound);win32hle_register(ds_dsound8_shims);
    uint32_t o=hle_scratch(4),a[]={0,o,0};assert(!hle_call_guest(hle_resolve("DirectSoundCreate8"),3,a));uint32_t ds=MEM32(o),b[600],s[5];
    for(unsigned i=0;i<256;i++)b[i]=make_buffer(ds);
    uint32_t volume[]={(uint32_t)-725};assert(!invoke(b[0],15,1,volume));
    /* Both tables must grow in DuplicateSoundBuffer; source metadata must
     * survive realloc, and duplicate PCM must outlive the original owner. */
    uint32_t duplicate[]={b[0],o};assert(!invoke(ds,5,2,duplicate));b[256]=MEM32(o);
    uint32_t get[]={o};assert(!invoke(b[256],6,1,get)&&(int32_t)MEM32(o)==-725);
    for(unsigned i=257;i<600;i++)b[i]=make_buffer(ds);
    hle_dsound_buffer_stats(s);assert(s[0]>=600&&s[1]==600&&s[2]==600&&s[3]-s[4]==600);
    uint32_t guid=hle_scratch(16);memcpy(hle_ptr(guid),notify_iid,16);uint32_t q[]={guid,o};assert(!invoke(b[599],0,2,q));uint32_t notify=MEM32(o);
    for(unsigned i=0;i<599;i++)assert(invoke(b[i],2,0,NULL)==0);
    assert(invoke(b[599],2,0,NULL)==1);hle_dsound_buffer_stats(s);assert(s[1]==1);
    assert(invoke(notify,2,0,NULL)==0);hle_dsound_buffer_stats(s);assert(s[1]==0&&s[3]==s[4]);uint32_t capacity=s[0];
    for(unsigned i=0;i<300;i++)b[i]=make_buffer(ds);
    hle_dsound_buffer_stats(s);assert(s[0]==capacity&&s[1]==300);
    for(unsigned i=0;i<300;i++)assert(invoke(b[i],2,0,NULL)==0);
    hle_dsound_buffer_stats(s);assert(s[1]==0&&s[3]==900&&s[4]==900);
    assert(!invoke(ds,2,0,NULL));SDL_Quit();free((void*)g_mem_base);
    puts("DirectSound dynamic pool: 600 retained buffers, duplicate across realloc, Notify final ownership, all 900 finalized and slots reused: PASS");
}
