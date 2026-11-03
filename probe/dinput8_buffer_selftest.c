/* Real SDL events -> guest DirectInput buffered COM, not a game-state mock. */
#include <SDL.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "win32hle.h"
#include "guest_test_harness.h"
extern const win32hle_shim ds_dinput8_shims[];
extern void ds_input_pump(void);
#define NOTACQUIRED 0x8007000cu
#define NOTBUFFERED 0x80040207u
#define INPUTLOST 0x8007001eu
#define UNPLUGGED 0x80040209u
#define INVALIDPARAM 0x80070057u

static uint32_t count_pointer,events;
static uint32_t read_events(uint32_t device,uint32_t size,uint32_t output,uint32_t capacity,uint32_t flags){
    MEM32(count_pointer)=capacity;uint32_t args[]={size,output,count_pointer,flags};return invoke(device,10,4,args);
}
static uint32_t count(void){return MEM32(count_pointer);}
static uint32_t property(uint32_t device,uint32_t which,uint32_t value){
    uint32_t p=hle_scratch(20);MEM32(p)=20;MEM32(p+4)=16;MEM32(p+16)=value;
    uint32_t args[]={which,p};return invoke(device,6,2,args);
}
static uint32_t device(uint32_t di,uint32_t first){
    uint32_t guid=hle_scratch(16),out=hle_scratch(4);MEM32(guid)=first;
    uint32_t args[]={guid,out,0};assert(!invoke(di,3,3,args));assert(MEM32(out));return MEM32(out);
}
static void format(uint32_t device,int kind){
    unsigned number=kind==1?1:kind==2?6:2;
    uint32_t f=hle_scratch(24),o=hle_scratch(number*16),guid=hle_scratch(3*16);
    MEM32(f)=24;MEM32(f+4)=16;MEM32(f+8)=kind==3?1:2;MEM32(f+12)=kind==1?16:kind==2?28:8;
    MEM32(f+16)=number;MEM32(f+20)=o;
    if(kind==1){MEM32(o+4)=5;MEM32(o+8)=0x0c|(0x11<<8);}
    else if(kind==2){
        for(unsigned i=0;i<3;i++){MEM32(guid+i*16)=0xa36d02e0+i;MEM32(o+i*16)=guid+i*16;MEM32(o+i*16+4)=16+i*4;MEM32(o+i*16+8)=1|(i<<8);}
        for(unsigned i=0;i<3;i++){MEM32(o+(i+3)*16+4)=4+i;MEM32(o+(i+3)*16+8)=0x0c|(i<<8);}
    }else{
        MEM32(guid)=0xa36d02e0;MEM32(o)=guid;MEM32(o+8)=2;MEM32(o+16+4)=4;MEM32(o+16+8)=0x0c|(2<<8);
    }
    uint32_t args[]={f};assert(!invoke(device,11,1,args));
}
static void key(SDL_Scancode scan,int down){
    SDL_Event e={.type=down?SDL_KEYDOWN:SDL_KEYUP};e.key.keysym.scancode=scan;e.key.keysym.sym=SDL_GetKeyFromScancode(scan);
    e.key.state=down?SDL_PRESSED:SDL_RELEASED;e.key.timestamp=1;assert(SDL_PushEvent(&e)==1);
}
static void mouse(void){
    SDL_Event e={.type=SDL_MOUSEMOTION};e.motion.xrel=17;e.motion.yrel=-9;e.motion.x=100;e.motion.y=200;assert(SDL_PushEvent(&e)==1);
    memset(&e,0,sizeof e);e.type=SDL_MOUSEWHEEL;e.wheel.y=-1;assert(SDL_PushEvent(&e)==1);
    memset(&e,0,sizeof e);e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;assert(SDL_PushEvent(&e)==1);
    e.type=SDL_MOUSEBUTTONUP;assert(SDL_PushEvent(&e)==1);
}
static int attach(void){
    int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,16,1);assert(index>=0);
    char guid[64],mapping[512];SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index),guid,sizeof guid);
    snprintf(mapping,sizeof mapping,"%s,Buffered input controller,a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,leftstick:b8,rightstick:b9,dpup:b10,dpright:b11,dpdown:b12,dpleft:b13,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,platform:Linux,",guid);
    assert(SDL_GameControllerAddMapping(mapping)>=0);return index;
}
static void physical_tests(uint32_t di){
    uint32_t keyboard=device(di,0x6f1d2b61),pointer=device(di,0x6f1d2b60);format(keyboard,1);format(pointer,2);
    assert(!property(keyboard,1,256)&&!property(pointer,1,256)&&!property(pointer,2,1));
    assert(!invoke(keyboard,7,0,NULL)&&!invoke(pointer,7,0,NULL));
    key(SDL_SCANCODE_W,1);key(SDL_SCANCODE_W,0);key(SDL_SCANCODE_A,1); /* unmapped A is filtered */
    assert(!read_events(keyboard,20,events,1,1)&&count()==1);
    assert(MEM32(events)==5&&MEM32(events+4)==0x80&&MEM32(events+8)>=123456&&MEM32(events+16)==0);
    uint8_t first[20];memcpy(first,hle_ptr(events),20);
    assert(!read_events(keyboard,20,events,1,1)&&count()==1&&!memcmp(first,hle_ptr(events),20));
    assert(!read_events(keyboard,20,0,UINT32_MAX,1)&&count()==2);
    memset(hle_ptr(events),0xa5,64);assert(!read_events(keyboard,16,events,2,0)&&count()==2);
    assert(MEM32(events)==5&&MEM32(events+4)==0x80&&MEM32(events+16)==5&&MEM32(events+20)==0);
    assert(MEM32(events+28)>MEM32(events+12)&&MEM32(events+24)>=MEM32(events+8)&&MEM32(events+32)==0xa5a5a5a5u);
    assert(!read_events(keyboard,20,0,UINT32_MAX,1)&&!count());
    mouse();assert(!read_events(pointer,20,events,8,1)&&count()==5);
    const uint32_t offsets[]={16,20,24,4,4},values[]={17,(uint32_t)-9,(uint32_t)-120,0x80,0};
    for(unsigned i=0;i<5;i++){assert(MEM32(events+i*20)==offsets[i]&&MEM32(events+i*20+4)==values[i]);assert(MEM32(events+i*20+8)>=123456);if(i)assert(MEM32(events+i*20+12)>MEM32(events+(i-1)*20+12));}
    assert(!read_events(pointer,20,0,2,0)&&count()==2);assert(!read_events(pointer,20,events,8,0)&&count()==3&&MEM32(events)==24);
    assert(read_events(pointer,24,events,1,0)==INVALIDPARAM&&!count());assert(read_events(pointer,20,events,1,2)==INVALIDPARAM&&!count());
    assert(!invoke(keyboard,8,0,NULL));assert(read_events(keyboard,20,events,1,0)==NOTACQUIRED&&!count());
    assert(!property(keyboard,1,0));assert(!invoke(keyboard,7,0,NULL));assert(read_events(keyboard,20,events,1,0)==NOTBUFFERED&&!count());
    assert(!invoke(keyboard,8,0,NULL));assert(!property(keyboard,1,4));assert(!invoke(keyboard,7,0,NULL));
    key(SDL_SCANCODE_W,1);key(SDL_SCANCODE_W,0);key(SDL_SCANCODE_W,1);key(SDL_SCANCODE_W,0);
    assert(read_events(keyboard,20,0,UINT32_MAX,1)==1&&count()==3);
    assert(read_events(keyboard,20,0,0,1)==1&&!count()); /* peek does not acknowledge overflow */
    assert(read_events(keyboard,20,events,1,0)==1&&count()==1&&MEM32(events+4)==0x80);
    assert(!read_events(keyboard,20,events,8,0)&&count()==2&&MEM32(events+4)==0&&MEM32(events+24)==0x80);
    uint32_t state=hle_scratch(16),get[]={16,state};assert(!invoke(keyboard,9,2,get)&&!MEM8(state+5));
    assert(property(keyboard,1,256)==0x800700aau); /* acquired configuration is rejected */
    assert(!invoke(keyboard,8,0,NULL));uint32_t coop[]={0,6};assert(!invoke(keyboard,13,2,coop));assert(!property(keyboard,1,256));assert(!invoke(keyboard,7,0,NULL));
    key(SDL_SCANCODE_W,1);SDL_Event focus={.type=SDL_WINDOWEVENT};focus.window.event=SDL_WINDOWEVENT_FOCUS_LOST;assert(SDL_PushEvent(&focus)==1);
    assert(read_events(keyboard,20,events,8,0)==INPUTLOST&&!count());assert(invoke(keyboard,7,0,NULL)==0x80070005u);
    focus.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;assert(SDL_PushEvent(&focus)==1);ds_input_pump();assert(!invoke(keyboard,7,0,NULL));
    assert(!read_events(keyboard,20,events,8,0)&&!count());assert(!invoke(keyboard,8,0,NULL));
    assert(!setenv("THING_PAD_MODE","native",1));int index=attach();SDL_Joystick *j=SDL_JoystickOpen(index);assert(j);
    SDL_JoystickSetVirtualAxis(j,4,-32768);SDL_JoystickSetVirtualAxis(j,5,-32768);SDL_JoystickUpdate();
    uint32_t pad=device(di,0x44530000);format(pad,3);assert(!property(pad,1,32));assert(!invoke(pad,7,0,NULL));
    SDL_JoystickSetVirtualAxis(j,0,32767);SDL_JoystickSetVirtualButton(j,0,1);
    assert(!read_events(pad,20,events,8,0)&&count()==2&&MEM32(events)==0&&MEM32(events+4)==128&&MEM32(events+20)==4&&MEM32(events+24)==0x80);
    SDL_JoystickClose(j);assert(!SDL_JoystickDetachVirtual(index));assert(read_events(pad,20,events,8,0)==UNPLUGGED&&!count());
    index=attach();j=SDL_JoystickOpen(index);assert(j);SDL_JoystickSetVirtualAxis(j,4,-32768);SDL_JoystickSetVirtualAxis(j,5,-32768);
    assert(!read_events(pad,20,events,8,0)&&!count());SDL_JoystickSetVirtualButton(j,0,1);
    assert(!read_events(pad,20,events,8,0)&&count()==1&&MEM32(events)==4&&MEM32(events+4)==0x80);
    assert(invoke(pad,2,0,NULL)==0);SDL_JoystickClose(j);assert(!SDL_JoystickDetachVirtual(index));
    assert(invoke(pointer,2,0,NULL)==0&&invoke(keyboard,2,0,NULL)==0);
    puts("Buffered DirectInput:16/20-byte ABI, peek/count/flush/consume, custom offsets, timestamp/sequence, mouse deltas, overflow, focus/acquire and native hotplug: PASS");
}
static void append(const char *path,const char *text){FILE*f=fopen(path,"ab");assert(f);assert(fputs(text,f)>=0);assert(!fclose(f));}
static void automation_tests(uint32_t di,const char *path){
    uint32_t keyboard=device(di,0x6f1d2b61),pointer=device(di,0x6f1d2b60);
    assert(!property(keyboard,1,256)&&!property(pointer,1,256));assert(!invoke(keyboard,7,0,NULL)&&!invoke(pointer,7,0,NULL));
    ds_input_pump();append(path,"tap start\n");for(unsigned i=0;i<20;i++)ds_input_pump();
    assert(!read_events(keyboard,20,events,1,1)&&count()==1&&MEM32(events)==1&&MEM32(events+4)==0x80);
    uint8_t first[20];memcpy(first,hle_ptr(events),20);assert(!read_events(keyboard,20,events,1,1)&&count()==1&&!memcmp(first,hle_ptr(events),20));
    assert(!read_events(keyboard,20,events,1,0)&&count()==1&&MEM32(events+4)==0x80);
    assert(!read_events(keyboard,20,events,1,0)&&count()==1&&MEM32(events)==1&&MEM32(events+4)==0);
    append(path,"axis righttrigger 32767\n");assert(!read_events(keyboard,20,events,256,0)&&!count());
    assert(!read_events(pointer,20,events,256,0)&&count()==1&&MEM32(events)==12&&MEM32(events+4)==0x80);
    append(path,"axis righttrigger 0\n");assert(!read_events(keyboard,20,events,256,0)&&!count());
    assert(!read_events(pointer,20,events,256,0)&&count()==1&&MEM32(events)==12&&MEM32(events+4)==0);
    append(path,"axis rightx 32767\n");SDL_Delay(25);assert(!read_events(keyboard,20,events,256,0)&&!count());
    assert(!read_events(pointer,20,events,256,0)&&count()>0&&MEM32(events)==0&&(int32_t)MEM32(events+4)>0);
    assert(invoke(pointer,2,0,NULL)==0&&invoke(keyboard,2,0,NULL)==0);
    puts("Buffered controller automation: loading-pump tap retention, peek-stable press/consume/release, trigger edges and analog relative camera: PASS");
}
int main(int argc,char **argv){
    int commands=argc==2&&!strcmp(argv[1],"--commands");char root[]="/tmp/thing-buffer-XXXXXX",path[256];
    assert(!setenv("THING_PAD_MODE","keyboard",1));
    if(commands){assert(mkdtemp(root));assert(!setenv("THING_ROOT",root,1));assert(!setenv("THING_TEST_INPUT","1",1));snprintf(path,sizeof path,"%s/input.commands",root);}
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER));g_mem_base=(ptrdiff_t)calloc(1,0x1000000);assert(g_mem_base);
    win32hle_register(ds_dinput8_shims);uint32_t iid=hle_scratch(16),out=hle_scratch(4);
    const uint8_t bytes[]={0x30,0x80,0x79,0xbf,0x3a,0x48,0xa2,0x4d,0xaa,0x99,0x5d,0x64,0xed,0x36,0x97,0};memcpy(hle_ptr(iid),bytes,16);
    uint32_t args[]={0,0x800,iid,out,0};assert(!hle_call_guest(hle_resolve("DirectInput8Create"),5,args));uint32_t di=MEM32(out);
    count_pointer=hle_scratch(4);events=hle_scratch(256*20+64);
    if(commands)automation_tests(di,path);else physical_tests(di);
    SDL_Quit();free((void*)g_mem_base);if(commands){assert(!unlink(path)&&!rmdir(root));}
}
