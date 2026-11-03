/* Actual advapi32 registry/parser + guest ABI, with a minimal memory harness. */
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../probe/guest_test_harness.h"
extern const win32hle_shim win32hle_advapi32[];
void thing_seed_installation_paths(void);
void hle_set_last_error(uint32_t error){(void)error;}
uint32_t hle_scratch_copy(const void*p,uint32_t n){uint32_t a=hle_scratch(n);memcpy(hle_ptr(a),p,n);return a;}
static void *handles[128];
uint32_t hle_handle_alloc(int type,void*obj){assert(type==HLE_H_REGKEY);for(unsigned i=1;i<128;i++)if(!handles[i]){handles[i]=obj;return i;}abort();}
void *hle_handle_obj(uint32_t h,int type){return type==HLE_H_REGKEY&&h<128?handles[h]:NULL;}
int hle_handle_close(uint32_t h){if(h>=128)return 0;handles[h]=NULL;return 1;}
static const char key[]="SOFTWARE\\Computer Artworks\\The Thing\\1.0";
static const char expected[]="C:\\data;C:\\data\\mainui.pak";
static void expect(uint32_t root,const char*name,const char*expected_value){
 uint32_t mark=hle_scratch_mark(),path=hle_scratch_copy(key,sizeof key),value=hle_scratch_copy(name,strlen(name)+1),out=hle_scratch(4),data=hle_scratch(1024),length=hle_scratch(4),type=hle_scratch(4);
 uint32_t open[5]={root,path,0,1,out};assert(!hle_call_guest(hle_resolve("RegOpenKeyExA"),5,open));
 MEM32(length)=1024;uint32_t query[6]={MEM32(out),value,0,type,data,length};assert(!hle_call_guest(hle_resolve("RegQueryValueExA"),6,query));
 assert(MEM32(type)==1);assert(MEM32(length)==strlen(expected_value)+1);assert(!memcmp(hle_ptr(data),expected_value,strlen(expected_value)+1));
 uint32_t close[1]={MEM32(out)};assert(!hle_call_guest(hle_resolve("RegCloseKey"),1,close));hle_scratch_release(mark);
}
int main(int argc,char**argv){
 assert(argc==2);g_mem_base=(ptrdiff_t)calloc(1,0x1000000);assert(g_mem_base);win32hle_register(win32hle_advapi32);
 const char*mode=argv[1];const char*custom="D:\\mods;C:\\data";
 if(!strcmp(mode,"old-hklm"))hle_reg_set_string(0x80000002,key,"ResourceDir","C:\\data");
 else if(!strcmp(mode,"old-hkcu")){hle_reg_set_string(0x80000001,key,"ResourceDir","c:/DATA/");hle_reg_set_string(0x80000002,key,"ResourceDir",expected);}
 else if(!strcmp(mode,"custom")){hle_reg_set_string(0x80000001,key,"ResourceDir",custom);hle_reg_set_string(0x80000002,key,"ResourceDir","C:\\data");}
 else if(!strcmp(mode,"empty"))hle_reg_set_string(0x80000001,key,"ResourceDir","");
 else if(!strcmp(mode,"both")){hle_reg_set_string(0x80000001,key,"ResourceDir","C:\\data");hle_reg_set_string(0x80000002,key,"ResourceDir",custom);}
 else assert(!strcmp(mode,"fresh"));
 hle_reg_set_string(0x80000001,key,"UnrelatedPreference","keep-me");thing_seed_installation_paths();
 if(!strcmp(mode,"custom"))expect(0x80000001,"ResourceDir",custom);
 else if(!strcmp(mode,"empty"))expect(0x80000001,"ResourceDir","");
 else if(!strcmp(mode,"old-hkcu")||!strcmp(mode,"both"))expect(0x80000001,"ResourceDir",expected);
 if(strcmp(mode,"empty"))expect(0x80000002,"ResourceDir",!strcmp(mode,"both")?custom:expected);
 expect(0x80000001,"UnrelatedPreference","keep-me");expect(0x80000002,"LevelDir","C:\\data\\Levels");expect(0x80000002,"FMVDir","C:\\data\\FMV");
 thing_seed_installation_paths();puts("PASS installation ABI: seed/migrate, exact slash bytes, hive precedence and preservation");free((void*)g_mem_base);return 0;
}
