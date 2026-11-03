#define _GNU_SOURCE
#include "win32hle.h"
#include "font_diagnostic.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
uint32_t g_eax=1,g_ecx=2,g_edx=3,g_esp=4,g_ebx=5,g_esi=6,g_edi=7,g_ebp=8,g_fs_base=9,g_cur_func=10;
ptrdiff_t g_mem_base;
double g_st[8]={.5,1.5,2.5,3.5,4.5,5.5,6.5,7.5};int g_fp_top=5;uint16_t g_fpu_cw=0x027f;
struct Snapshot{uint32_t regs[14];double st[8];int64_t shadow[8];int top;uint16_t cw;};
static struct Snapshot snap(void){struct Snapshot s={0};uint32_t r[]={g_eax,g_ecx,g_edx,g_esp,g_ebx,g_esi,g_edi,g_ebp,g_fs_base,g_cur_func,g_flag_k,g_flag_a,g_flag_b,g_flag_cf};memcpy(s.regs,r,sizeof r);memcpy(s.st,g_st,sizeof g_st);memcpy(s.shadow,g_st_i64,sizeof g_st_i64);s.top=g_fp_top;s.cw=g_fpu_cw;return s;}
static void check(uint32_t site,uint32_t font,uint32_t name,uint32_t path,uint32_t result){struct Snapshot a=snap();uint8_t memory[0x10000];memcpy(memory,(void*)g_mem_base,sizeof memory);errno=EDOM;thing_font_diagnostic(site,font,name,path,result);assert(errno==EDOM);struct Snapshot b=snap();assert(!memcmp(&a,&b,sizeof a));assert(!memcmp(memory,(void*)g_mem_base,sizeof memory));}
int main(int argc,char**argv){
 assert(argc==2);int traced=!strcmp(argv[1],"on");setenv("THING_FONT_TRACE",traced?"1":"0",1);
 void*p=mmap(NULL,0x20000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED);g_mem_base=(ptrdiff_t)p;assert(!mprotect((char*)p+0x10000,0x10000,PROT_NONE));
 strcpy(hle_ptr(0x800),"fnt_handelgothic.3df");strcpy(hle_ptr(0x1000),"C:\\data\\mainui.pak\\Textures/fnt_handelgothic.3df");MEM8(0x100+0x88)=1;MEM32(0x100+0x40)=162;MEM32(0x100+0x50)=0x400;
 g_flag_k=4;g_flag_a=0xfeed1234;g_flag_b=0xbeef5678;g_flag_cf=1;for(int i=0;i<8;i++)g_st_i64[i]=i*17;
 FILE*f=tmpfile();assert(f);int saved=dup(2);assert(saved>=0&&dup2(fileno(f),2)==2);
 check(0x76887d,0x100,0x800,0x1000,1);
 /* Unmapped, overflowing and NULL pointers are diagnostic errors, not faults. */
 check(0x76887d,0x10000,0xfffffff0,0,1);
 check(0x768830,0,0x800,0x10000,0);check(0x76887d,0x10000,0x800,0xfffffff0,0);
 for(int i=0;i<64;i++)check(0x768830,0x100,0x800,0,0);
 fflush(stderr);assert(dup2(saved,2)>=0);close(saved);rewind(f);char line[4096];unsigned lines=0,errors=0,valid=0,unreadable=0;
 while(fgets(line,sizeof line,f)){++lines;if(strstr(line,"[font-error]"))++errors;if(strstr(line,"loaded=1 glyphs=162 metrics=0x400"))++valid;if(strstr(line,"<unreadable:"))++unreadable;}
 assert(errors==32&&unreadable>=2);assert(lines==32+(traced?2:0));assert(!traced||valid);fclose(f);munmap(p,0x20000);
 printf("PASS font diagnostic mode=%s: guest registers/flags/FP/shadow/memory/errno unchanged; invalid pointers safe; bounded logging (%u lines)\n",argv[1],lines);
}
