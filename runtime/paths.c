#define _XOPEN_SOURCE 700
#include "paths.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int directory(const char *path, int create) {
    struct stat st;
    if (create && mkdir(path, 0755) && errno != EEXIST) return -1;
    if (stat(path, &st)) return -1;
    if (!S_ISDIR(st.st_mode)) { errno=ENOTDIR; return -1; }
    return 0;
}
static int join(char *out, size_t size, const char *root, const char *relative) {
    int n=snprintf(out,size,"%s%s%s",root,*relative?"/":"",relative);
    if(n<0 || (size_t)n>=size) {errno=ENAMETOOLONG;return -1;}
    return 0;
}
int ds_paths_init(void) {
    char root[PATH_MAX],candidate[PATH_MAX],path[PATH_MAX];
    const char *requested=getenv("THING_ROOT");
    if(requested && *requested) {
        if(!realpath(requested,root))return -1;
    } else {
        ssize_t n=readlink("/proc/self/exe",candidate,sizeof candidate-1);
        if(n<0)return -1;
        if((size_t)n>=sizeof candidate-1){errno=ENAMETOOLONG;return -1;}
        candidate[n]=0;
        char *slash=strrchr(candidate,'/');
        if(!slash){errno=EINVAL;return -1;}
        if(slash==candidate)slash[1]=0;else *slash=0;
        if(!realpath(candidate,root))return -1;
    }
    const char *required[]={"bin","data"};
    for(size_t i=0;i<sizeof required/sizeof *required;i++)
        if(join(path,sizeof path,root,required[i]) || directory(path,0))return -1;
    const char *writable[]={"saves","config","userdata","cache","tmp"};
    for(size_t i=0;i<sizeof writable/sizeof *writable;i++)
        if(join(path,sizeof path,root,writable[i]) || directory(path,1))return -1;
    const struct {const char *key,*relative;} values[]={
        {"THING_ROOT",""},{"DS_ROOT",""},{"THING_SAVE_DIR","saves"},
        {"DS_SAVE_DIR","saves"},{"XDG_CONFIG_HOME","config"},
        {"XDG_DATA_HOME","userdata"},{"XDG_STATE_HOME","userdata"},
        {"XDG_CACHE_HOME","cache"},{"TMPDIR","tmp"}
    };
    for(size_t i=0;i<sizeof values/sizeof *values;i++) {
        if(join(path,sizeof path,root,values[i].relative) || setenv(values[i].key,path,1))return -1;
    }
    /* Keep configurable mapping overrides supplied by a launcher. */
    if(!getenv("DS_PAD_CONFIG")) {
        if(join(path,sizeof path,root,"controller-buttons.cfg") || setenv("DS_PAD_CONFIG",path,1))return -1;
    }
    return 0;
}
