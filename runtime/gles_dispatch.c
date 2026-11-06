/* Resolve GLES through the current SDL context instead of linking a vendor
 * library (libmali.so.0 / libGLESv2.so.2). Every GL entry point the port calls
 * is defined here as a small trampoline that looks the real function up with
 * SDL_GL_GetProcAddress on first use and caches it per thread, so the
 * synchronous renderer and the render worker each resolve in their own
 * current context. The executable therefore needs no GPU library at link time
 * and runs on whatever GLES driver the firmware's SDL2 loads.
 *
 * The call list is shared with the render-thread proxy and GL cost probe
 * (probe/rt/*.def); a GL function used by the port but missing from it is a
 * link error, never a silent fallback. Same approach as the Desert Storm port. */
#include <SDL.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
static void *resolve_gl(const char *name){
    void *entry=SDL_GL_GetCurrentContext()?SDL_GL_GetProcAddress(name):NULL;
    if(!entry){fprintf(stderr,"[gles-loader] cannot resolve %s: %s\n",name,SDL_GetError());abort();}
    return entry;
}
/* Hidden: with -rdynamic these must not interpose GL calls made inside SDL or
 * the driver itself. */
#pragma GCC visibility push(hidden)
#define R(ret,name,params,args) ret GL_APIENTRY name params { \
    typedef ret (GL_APIENTRYP Proc) params; \
    static _Thread_local Proc entry; \
    if(!entry)entry=(Proc)resolve_gl(#name); \
    return entry args; }
#define V(name,params,args) R(void,name,params,args)
#include "rt/gl_cost_calls.def"
#include "rt/render_thread_extra.def"
#undef V
#undef R
#pragma GCC visibility pop
