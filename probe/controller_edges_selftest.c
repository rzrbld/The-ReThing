/* Controller overlay through the actual DirectInput guest ABI. No game code. */
#include <SDL.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "win32hle.h"
#include "guest_test_harness.h"
extern const win32hle_shim ds_dinput8_shims[];
extern void ds_input_pump(void);
static int attach(void){
    int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,16,1);assert(index>=0);
    char guid[64],mapping[512];SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index),guid,sizeof guid);
    snprintf(mapping,sizeof mapping,"%s,The Thing virtual controller,a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,leftstick:b8,rightstick:b9,dpup:b10,dpright:b11,dpdown:b12,dpleft:b13,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,platform:Linux,",guid);
    assert(SDL_GameControllerAddMapping(mapping)>=0);return index;
}
static uint32_t create(uint32_t di,uint32_t out,uint32_t first){
    uint32_t guid=hle_scratch(16);MEM32(guid)=first;uint32_t args[3]={guid,out,0};assert(!invoke(di,3,3,args));uint32_t d=MEM32(out);assert(d);return d;
}
int main(void){
    assert(!setenv("THING_PAD_MODE","keyboard",1));SDL_setenv("SDL_VIDEODRIVER","dummy",1);
    assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER));g_mem_base=(ptrdiff_t)calloc(1,0x1000000);assert(g_mem_base);
    int index=attach();SDL_Joystick *joystick=SDL_JoystickOpen(index);assert(joystick);
    SDL_JoystickSetVirtualAxis(joystick,4,-32768);SDL_JoystickSetVirtualAxis(joystick,5,-32768);
    win32hle_register(ds_dinput8_shims);uint32_t iid=hle_scratch(16),out=hle_scratch(4);
    const uint8_t bytes[]={0x30,0x80,0x79,0xbf,0x3a,0x48,0xa2,0x4d,0xaa,0x99,0x5d,0x64,0xed,0x36,0x97,0};memcpy(hle_ptr(iid),bytes,16);
    uint32_t args[5]={0,0x800,iid,out,0};assert(!hle_call_guest(hle_resolve("DirectInput8Create"),5,args));uint32_t di=MEM32(out);
    uint32_t keyboard=create(di,out,0x6f1d2b61),mouse=create(di,out,0x6f1d2b60);
    uint32_t prop=hle_scratch(20);MEM32(prop)=20;MEM32(prop+4)=16;MEM32(prop+16)=64;
    uint32_t pargs[2]={1,prop};assert(!invoke(keyboard,6,2,pargs));assert(!invoke(mouse,6,2,pargs));assert(!invoke(keyboard,7,0,NULL)&&!invoke(mouse,7,0,NULL));
    uint32_t events=hle_scratch(64*20),n=hle_scratch(4);uint32_t get[4]={20,events,n,0};
    ds_input_pump();MEM32(n)=64;assert(!invoke(keyboard,10,4,get));
    /* SDL has already observed BOTH hardware transitions, while the game has
     * not polled. Current-state-only sampling sees neutral and loses the tap. */
    assert(!SDL_JoystickSetVirtualButton(joystick,7,1));SDL_JoystickUpdate();
    assert(!SDL_JoystickSetVirtualButton(joystick,7,0));SDL_JoystickUpdate();
    MEM32(n)=64;assert(!invoke(keyboard,10,4,get));
    printf("short Start tap buffered events=%u (expected Esc press+release)\n",MEM32(n));
    assert(MEM32(n)==2);assert(MEM32(events)==1&&MEM32(events+4)==0x80);assert(MEM32(events+20)==1&&MEM32(events+24)==0);
    assert(!SDL_JoystickSetVirtualButton(joystick,0,1));SDL_JoystickUpdate();assert(!SDL_JoystickSetVirtualButton(joystick,0,0));SDL_JoystickUpdate();
    MEM32(n)=64;assert(!invoke(keyboard,10,4,get));assert(MEM32(n)==4);
    /* Trigger press/release between game polls must reach mouse buffer too. */
    assert(!SDL_JoystickSetVirtualAxis(joystick,5,32767));SDL_JoystickUpdate();assert(!SDL_JoystickSetVirtualAxis(joystick,5,-32768));SDL_JoystickUpdate();
    MEM32(n)=64;assert(!invoke(mouse,10,4,get));assert(MEM32(n)==2);assert(MEM32(events)==12&&MEM32(events+4)==0x80);assert(MEM32(events+20)==12&&MEM32(events+24)==0);
    SDL_JoystickClose(joystick);assert(!SDL_JoystickDetachVirtual(index));ds_input_pump();
    SDL_Quit();free((void*)g_mem_base);puts("Physical SDL event replay: short Start/A/trigger taps through DirectInput buffers: PASS");
}
