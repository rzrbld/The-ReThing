#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../../runtime/d3d8_renderer.h"
static D8State state;
void thing_geometry_identity(uint32_t out[5]){for(unsigned i=0;i<5;i++)out[i]=i+10;}
#include "geometry_capture.h"
int main(int argc,char**argv){if(argc!=2)return 1;for(unsigned j=0;j<4;j++){state.transform[256][j*5]=1;state.transform[2][j*5]=1;state.transform[3][j*5]=1;}state.fvf=2;state.rs[22]=2;
 float xyz[9]={-.5f,-.5f,.5f,.5f,-.5f,.5f,0,.5f,.5f};uint16_t ix[3]={0,1,2};gc_begin(argv[1],7);gc_capture(4,1,xyz,12,3,ix,2,0,1,2);state.rs[22]=3;gc_capture(4,1,xyz,12,3,ix,2,0,1,2);ix[2]=3;gc_capture(4,1,xyz,12,3,ix,2,0,1,2);gc_end();return 0;}
