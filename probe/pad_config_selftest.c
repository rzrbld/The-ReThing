#include "desert_pad.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void){
    DS_PadConfig config;DS_PadState state;DS_PadRaw raw={{0},0};
    ds_pad_defaults(&config);assert(ds_pad_selftest());
    ds_pad_encode(&raw,&config,&state);
    assert(state.slider[0]==-128&&state.slider[1]==-128);
    raw.axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT]=16384;
    raw.axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT]=32767;
    ds_pad_encode(&raw,&config,&state);
    assert(state.slider[0]>=0&&state.slider[0]<=1&&state.slider[1]==128);
    assert(state.buttons[4]==0x80&&state.buttons[5]==0x80);
    raw.axes[SDL_CONTROLLER_AXIS_LEFTX]=1000;
    ds_pad_encode(&raw,&config,&state);assert(state.x==0);
    char path[]="/tmp/thing-pad-XXXXXX";int fd=mkstemp(path);assert(fd>=0);
    FILE *f=fdopen(fd,"w");assert(f);fputs("axis2=none\naxis3=rightx\n0=a\ndeadzone=0.15\n",f);fclose(f);
    assert(ds_pad_load(&config,path)==0);assert(config.axis[2]==-1&&config.axis[3]==SDL_CONTROLLER_AXIS_RIGHTX);
    raw.axes[SDL_CONTROLLER_AXIS_RIGHTX]=32767;raw.buttons=1u<<SDL_CONTROLLER_BUTTON_A;
    ds_pad_encode(&raw,&config,&state);assert(state.z==0&&state.rx==128&&state.buttons[0]==0x80);
    DS_PadConfig before=config;f=fopen(path,"w");assert(f);fputs("axis2=notanaxis\n",f);fclose(f);
    assert(ds_pad_load(&config,path)==-1);assert(!memcmp(&config,&before,sizeof config));unlink(path);
    puts("Pad configurable axes/buttons, analog triggers, deadzone and atomic invalid-config rejection: PASS");
}
