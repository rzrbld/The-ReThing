#define _GNU_SOURCE
#include "rigid.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <fenv.h>
#include <stdio.h>
extern ptrdiff_t g_mem_base;
int thing_game_rigid_skin_enabled;
static uint64_t calls,processed;
static int activation_reported;
__attribute__((constructor)) static void rigid_init(void){const char *s=getenv("THING_NATIVE_RIGID_SKIN");thing_game_rigid_skin_enabled=s&&s[0]=='1';}
__attribute__((destructor)) static void rigid_report(void){fprintf(stderr,"game-rigid-skin enabled=%d calls=%llu processed_vertices=%llu\n",thing_game_rigid_skin_enabled,(unsigned long long)calls,(unsigned long long)processed);}

#define PTR(a) ((unsigned char *)(g_mem_base+(uintptr_t)(uint32_t)(a)))
static int overlap(uint32_t a,uint32_t e,uint32_t b,uint32_t f){return a<f&&b<e;}
static int finite32(const float *p){uint32_t u;memcpy(&u,p,4);return (u&0x7f800000u)!=0x7f800000u;}
/* Build this native object with -ffp-contract=off -fno-fast-math. Every sum
 * below follows original7AED58..7AEE55 order using current translator double
 * intermediates. It is not a generic matrix multiply or render API change.
 */
uint32_t thing_game_rigid_skin(uint32_t source,uint32_t source_end,uint32_t output,uint32_t output_end,uint32_t palette,uint32_t palette_end,uint32_t n,uint32_t guest_cw,int guest_top){
 if(!thing_game_rigid_skin_enabled)return 0;calls++;
 if(calls==1)fprintf(stderr,"[game-rigid-skin] entered n=%u source_bytes=%u output_bytes=%u palette_bytes=%u\n",n,source_end-source,output_end-output,palette_end-palette);
#if !defined(__GLIBC__)
 return 0;
#else
 if((guest_cw&63)!=63||guest_top<0||guest_top>4||fegetround()!=FE_TONEAREST||fegetexcept())return 0;
 if(n<2||n>1000000||!source||!output||!palette||source_end<source||output_end<output||palette_end<palette)return 0;
 if((source_end-source)%72||(output_end-output)%32||(palette_end-palette)%64)return 0;
 if(n>(source_end-source)/72||n>(output_end-output)/32)return 0;
 if(overlap(source,source_end,output,output_end)||overlap(palette,palette_end,output,output_end))return 0;
 uint32_t bones=(palette_end-palette)/64,done=0;int32_t last_bone=-1;
 for(uint32_t i=n-1;i>0;i--){
  const unsigned char *v=PTR(source+i*72);float *d=(float *)PTR(output+i*32);int16_t bone;memcpy(&bone,v+0x2c,2);
  if(bone<0||(uint32_t)bone>=bones)break;
  const float *p=(const float *)(v+4),*normal=(const float *)(v+0x10),*m=(const float *)PTR(palette+(uint32_t)bone*64);
  int valid=1;for(unsigned k=0;k<3;k++)valid&=finite32(p+k)&&finite32(normal+k);
  if(bone!=last_bone){for(unsigned row=0;row<4;row++)for(unsigned k=0;k<3;k++)valid&=finite32(m+row*4+k);}
  if(valid)last_bone=bone;
  if(!valid)break;
  double px=p[0],py=p[1],pz=p[2],nx=normal[0],ny=normal[1],nz=normal[2];
  d[3]=(float)((nz*(double)m[8]+(double)m[4]*ny)+(double)m[0]*nx);
  d[4]=(float)(((double)m[1]*nx+nz*(double)m[9])+ny*(double)m[5]);
  d[5]=(float)(((double)m[2]*nx+nz*(double)m[10])+ny*(double)m[6]);
  double z=((px*(double)m[2]+pz*(double)m[10])+py*(double)m[6])+(double)m[14];
  double y=((px*(double)m[1]+pz*(double)m[9])+py*(double)m[5])+(double)m[13];
  double x=(((double)m[4]*py+pz*(double)m[8])+px*(double)m[0])+(double)m[12];
  d[0]=(float)x;d[1]=(float)y;d[2]=(float)z;
  memcpy((unsigned char *)d+24,v+0x24,8);done++;
 }
 uint64_t before=processed;processed+=done;
 if(done&&!activation_reported){activation_reported=1;fprintf(stderr,"[game-rigid-skin] active first_processed_vertices=%u\n",done);}
 if(processed/100000!=before/100000)fprintf(stderr,"[game-rigid-skin] calls=%llu processed_vertices=%llu\n",(unsigned long long)calls,(unsigned long long)processed);
 return done;
#endif
}
