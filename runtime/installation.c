/* Installation-path registry defaults recovered from the original 1.2 game.
 * This replaces the installer-created paths for a relocated native install;
 * it does not rewrite game code or alter stored user choices. */
#include "win32hle.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

static const char key[]="SOFTWARE\\Computer Artworks\\The Thing\\1.0";
static const char resources[]="C:\\data;C:\\data\\mainui.pak";
/* Query through the same Win32 ABI as the game; no private registry tree reads. */
static int read_path(uint32_t root,const char *name,char value[1024],uint32_t *type) {
    uint32_t mark=hle_scratch_mark(),path=hle_scratch_copy(key,sizeof key),
        label=hle_scratch_copy(name,(uint32_t)strlen(name)+1),out=hle_scratch(4),
        kind=hle_scratch(4),length=hle_scratch(4),data=hle_scratch(1024);
    uint32_t args[5]={root,path,0,1,out};int status=(int)hle_call_guest(hle_resolve("RegOpenKeyExA"),5,args);
    if(!status) {
        uint32_t handle=MEM32(out),query[6]={handle,label,0,kind,data,length};MEM32(length)=1024;
        status=(int)hle_call_guest(hle_resolve("RegQueryValueExA"),6,query);
        if(!status){*type=MEM32(kind);memcpy(value,hle_ptr(data),1024);value[1023]=0;}
        uint32_t close[1]={handle};hle_call_guest(hle_resolve("RegCloseKey"),1,close);
    }
    hle_scratch_release(mark);return status;
}
static int legacy_resources(const char *value) {
    char normalized[1024];snprintf(normalized,sizeof normalized,"%s",value);
    for(char *p=normalized;*p;p++)if(*p=='/')*p='\\';
    size_t n=strlen(normalized);while(n&&normalized[n-1]=='\\')normalized[--n]=0;
    return !strcasecmp(normalized,"C:\\data");
}
static int present(const char *name) {
    const uint32_t roots[]={0x80000001u,0x80000002u}; /* game's HKCU then HKLM lookup */
    uint32_t open=hle_resolve("RegOpenKeyExA"),query=hle_resolve("RegQueryValueExA"),close=hle_resolve("RegCloseKey");
    if(!open || !query || !close)hle_fatal("installation path defaults need registry shims");
    uint32_t mark=hle_scratch_mark(),path=hle_scratch_copy(key,sizeof key),
        value=hle_scratch_copy(name,(uint32_t)strlen(name)+1),out=hle_scratch(4);
    int exists=0;
    for(unsigned i=0;i<2;i++) {
        uint32_t args[5]={roots[i],path,0,1,out};
        if(hle_call_guest(open,5,args))continue;
        uint32_t handle=MEM32(out),q[6]={handle,value,0,0,0,0};
        if(!hle_call_guest(query,6,q))exists=1;
        uint32_t c[1]={handle};hle_call_guest(close,1,c);
        if(exists)break;
    }
    hle_scratch_release(mark);return exists;
}
void thing_seed_installation_paths(void) {
    /* Original InstallShield data1.hdr stores
     * <TARGETDIR>\\data;<TARGETDIR>\\data\\mainui.pak. The permanent MainUI
     * archive entry is needed after the menu's temporary search base closes. */
    const struct {const char *name,*value;} defaults[]={
        {"ResourceDir",resources},
        {"LevelDir","C:\\data\\Levels"},
        {"FMVDir","C:\\data\\FMV"}
    };
    for(unsigned i=0;i<sizeof defaults/sizeof *defaults;i++) {
        if(present(defaults[i].name))continue;
        hle_reg_set_string(0x80000002u,key,defaults[i].name,defaults[i].value);
        fprintf(stderr,"[installation] default %s=%s\n",defaults[i].name,defaults[i].value);
    }
    /* Correct only the exact incomplete default distributed by earlier port
     * builds. Custom search lists and all other preferences remain untouched.
     * Both hives matter: an old HKCU value shadows a corrected HKLM value. */
    const uint32_t roots[]={0x80000001u,0x80000002u};
    for(unsigned i=0;i<2;i++) {
        char value[1024];uint32_t type=0;
        if(!read_path(roots[i],"ResourceDir",value,&type)&&type==1&&legacy_resources(value)) {
            hle_reg_set_string(roots[i],key,"ResourceDir",resources);
            fprintf(stderr,"[installation] corrected legacy %s ResourceDir=%s -> %s\n",i?"HKLM":"HKCU",value,resources);
        }
    }
    for(unsigned i=0;i<2;i++) {
        char value[1024];uint32_t type=0;int status=read_path(roots[i],"ResourceDir",value,&type);
        if(status==2)continue;
        if(!status)fprintf(stderr,"[installation] effective ResourceDir hive=%s type=%u value=%s\n",i?"HKLM":"HKCU",type,value);
        else fprintf(stderr,"[installation] ResourceDir lookup hive=%s status=%d\n",i?"HKLM":"HKCU",status);
        break;
    }
}
