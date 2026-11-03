/* DirectSound Notify COM + real SDL mixer crossings. Dummy output is not
 * evidence of audible sound or H700 performance. No decoder/game code runs.
 */
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include "guest_test_harness.h"
extern const win32hle_shim ds_dsound8_shims[];
static const uint8_t iid_notify[]={0x83,0x07,0x21,0xb0,0xcd,0x89,0xd0,0x11,0xaf,0x08,0x00,0xa0,0xc9,0x25,0xcd,0x16};
static const uint8_t iid_buffer8[]={0x49,0xa4,0x25,0x68,0x24,0x75,0x82,0x4d,0x92,0x0f,0x50,0xe3,0x6a,0xb3,0xab,0x1e};
static const uint8_t iid_unknown[]={0,0,0,0,0,0,0,0,0xc0,0,0,0,0,0,0,0x46};
static unsigned previous_hook_calls;
static int previous_hook(uint32_t handle){(void)handle;++previous_hook_calls;return -1;}
static uint32_t query(uint32_t object,const uint8_t *bytes,uint32_t *result){
    uint32_t guid=hle_scratch(16),out=hle_scratch(4);memcpy(hle_ptr(guid),bytes,16);
    uint32_t args[]={guid,out};uint32_t hr=invoke(object,0,2,args);*result=MEM32(out);return hr;
}
static uint32_t make_buffer(uint32_t ds,uint32_t flags){
    uint32_t desc=hle_scratch(36),format=hle_scratch(18),out=hle_scratch(4);
    MEM32(desc)=36;MEM32(desc+4)=flags;MEM32(desc+8)=16384;MEM32(desc+16)=format;
    MEM16(format)=1;MEM16(format+2)=1;MEM32(format+4)=22050;MEM32(format+8)=44100;MEM16(format+12)=2;MEM16(format+14)=16;
    uint32_t args[]={desc,out,0};assert(!invoke(ds,3,3,args));assert(MEM32(out));return MEM32(out);
}
static unsigned signaled(uint32_t event){assert(hle_wait_hook);assert(hle_wait_hook(event)==-1);return test_event_signals[event-0x200];}
static void wait_next(uint32_t event,unsigned previous){
    Uint32 deadline=SDL_GetTicks()+1500;
    while(signaled(event)==previous&&SDL_GetTicks()<deadline)SDL_Delay(2);
    assert(signaled(event)>previous);
}
static uint32_t positions(uint32_t notify,unsigned count,uint32_t array){uint32_t args[]={count,array};return invoke(notify,3,2,args);}
int main(void){
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);g_mem_base=(ptrdiff_t)calloc(1,0x400000);assert(g_mem_base);
    hle_wait_hook=previous_hook;win32hle_register(win32hle_dsound);win32hle_register(ds_dsound8_shims);
    uint32_t out=hle_scratch(4),create[]={0,out,0};assert(!hle_call_guest(hle_resolve("dsound.dll#11"),3,create));uint32_t ds=MEM32(out);
    uint32_t owner=make_buffer(ds,0x10188),notify=0,back=0,unknown=0,again=0;
    assert(!query(owner,iid_notify,&notify)&&notify&&notify!=owner);
    assert(!query(notify,iid_buffer8,&back)&&back==owner);assert(invoke(back,2,0,NULL)==2);
    assert(!query(notify,iid_unknown,&unknown)&&unknown==owner);assert(invoke(unknown,2,0,NULL)==2);
    assert(!query(notify,iid_notify,&again)&&again==notify);assert(invoke(again,2,0,NULL)==1);
    uint32_t unsupported=make_buffer(ds,0x80),bad=123;
    assert(query(unsupported,iid_notify,&bad)==0x80004002u&&bad==0);assert(invoke(unsupported,2,0,NULL)==0);
    uint32_t array=hle_scratch(20*8);
    for(unsigned i=0;i<20;i++){MEM32(array+i*8)=(i+1)*16384/20-1;MEM32(array+i*8+4)=0x220+i;}
    assert(!positions(notify,20,array)); /* actual game registration count */
    MEM32(array)=4095;MEM32(array+4)=0x200;MEM32(array+8)=8191;MEM32(array+12)=0x201;
    MEM32(array+16)=UINT32_MAX;MEM32(array+20)=0x203;assert(!positions(notify,3,array));
    uint32_t play[]={0,0,0};assert(!invoke(owner,12,3,play));assert(positions(notify,0,0)==0x88780032u);
    wait_next(0x200,0);wait_next(0x201,0);wait_next(0x203,0);
    uint32_t status[]={out};assert(!invoke(owner,9,1,status)&&!(MEM32(out)&1));
    for(unsigned i=0;i<20;i++)assert(!test_event_signals[0x20+i]); /* replacement removed old registrations */
    MEM32(array)=2048;MEM32(array+4)=0x204;MEM32(array+8)=8192;MEM32(array+12)=0x205;
    MEM32(array+16)=UINT32_MAX;MEM32(array+20)=0x206;assert(!positions(notify,3,array));
    uint32_t seek[]={12288};assert(!invoke(owner,13,1,seek));play[2]=1;
    Uint32 start=SDL_GetTicks();assert(!invoke(owner,12,3,play));wait_next(0x204,0);
    assert(SDL_GetTicks()-start>=50); /* crossing must follow real cursor travel across wrap */
    wait_next(0x205,0);unsigned old=signaled(0x204);wait_next(0x204,old); /* another complete loop */
    assert(!invoke(owner,18,0,NULL)&&signaled(0x206)>0);assert(!invoke(owner,9,1,status)&&!(MEM32(out)&1));
    MEM32(array)=16384;MEM32(array+4)=0x207;assert(positions(notify,1,array)==0x80070057u);
    MEM32(array)=0;MEM32(array+4)=0;assert(positions(notify,1,array)==0x80070057u);
    assert(!positions(notify,0,0));
    assert(invoke(owner,2,0,NULL)==1); /* the notify proxy still owns the buffer */
    assert(!query(notify,iid_buffer8,&back)&&back==owner);assert(!invoke(back,9,1,status));assert(invoke(back,2,0,NULL)==1);
    assert(invoke(notify,2,0,NULL)==0);hle_dsound_poll_notifications();assert(previous_hook_calls>0);
    assert(invoke(ds,2,0,NULL)==0);SDL_Quit();free((void*)g_mem_base);
    puts("DirectSound Notify: exact IID/4-slot COM, buffer identity/lifetime,20positions, replacement, real cursor crossings/wrap/nonloop-end/Stop events and wait-hook delivery: PASS");
}
