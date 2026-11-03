#!/usr/bin/env python3
"""Stage a runtime copy with the optional render-worker hooks.

Copies runtime/*.c and *.h into build/render-thread-src/runtime and applies the
bounded hook set used by probe/rt/render_thread_proxy.c: start the GL worker
after context creation, move frame origin/sync around Present and captures,
defer the post-draw glGetError, stop the worker before context teardown, and let
device exit stop it via atexit. Every replacement is fail-closed: a missing or
ambiguous anchor aborts the staging instead of producing a silently wrong build.
"""
from pathlib import Path
import shutil, sys

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'runtime'
DST = ROOT / 'build/render-thread-src/runtime'


def replace(text, old, new, what):
    n = text.count(old)
    if n != 1:
        sys.exit(f'render-thread staging: anchor {what!r} occurs {n} times (want 1)')
    return text.replace(old, new, 1)


def main():
    if DST.exists():
        shutil.rmtree(DST)
    DST.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(SRC, DST)

    d = (DST / 'd3d8.c').read_text()
    d = replace(d, '#include "win32hle.h"',
                '#include "win32hle.h"\n#include "render_thread_proxy.h"\nstatic void rt_process_exit(void);', 'd3d8 include')
    d = replace(d, 'memcpy(hle_ptr(params),presentation,sizeof presentation);return 0;',
                'memcpy(hle_ptr(params),presentation,sizeof presentation);'
                '{int rt=ds_rt_start(window,context);if(rt<0)fprintf(stderr,"[render-thread] start failed: %s\\n",ds_rt_error()?ds_rt_error()->message:"?");}return 0;',
                'device start')
    d = replace(d, 'ds_input_pump();ds_fps_overlay_draw(window);',
                'ds_input_pump();ds_rt_origin(g_cur_func,frames+1);ds_fps_overlay_draw(window);',
                'present origin')
    d = replace(d, 'SDL_GL_SwapWindow(window);d8_renderer_begin_frame(&renderer);',
                'SDL_GL_SwapWindow(window);if(ds_rt_active()&&ds_rt_sync()<0)fprintf(stderr,"[render-thread] present sync: %s\\n",ds_rt_error()?ds_rt_error()->message:"?");d8_renderer_begin_frame(&renderer);',
                'present sync')
    d = replace(d, 'device_obj=0;destroy_scale_target();ds_fps_overlay_close();',
                'device_obj=0;if(ds_rt_active()&&ds_rt_stop())fprintf(stderr,"[render-thread] stop: %s\\n",ds_rt_error()?ds_rt_error()->message:"?");destroy_scale_target();ds_fps_overlay_close();',
                'device stop')
    d = replace(d, 'static void capture_frame(unsigned frame){',
                'static void capture_frame(unsigned frame){if(ds_rt_active())ds_rt_sync();',
                'capture sync')
    d = replace(d, 'static int draw(unsigned primitive,unsigned count,const void *vertices,unsigned stride,unsigned vcount,const void *indices,unsigned isize,unsigned base,Object *vb,size_t voff,Object *ib,size_t ioff){\n',
                'static int draw(unsigned primitive,unsigned count,const void *vertices,unsigned stride,unsigned vcount,const void *indices,unsigned isize,unsigned base,Object *vb,size_t voff,Object *ib,size_t ioff){\n    ds_rt_origin(g_cur_func,frames+1);\n',
                'draw origin')
    # Stops the worker if the guest exits without releasing the D3D8 device.
    # The movie path checks that the context is current on the calling thread and
    # draws directly; with the worker owning the context that check fails. Run the
    # whole movie draw as a blocking native job on the worker instead.
    d = replace(d, '''int thing_video_present_rgba(const uint8_t *rgba,int width,int height){
    batch_barrier();
    if(!window||!context||SDL_GL_GetCurrentContext()!=context)return -1;
    int rc=d8_video_draw_rgba(window,rgba,width,height,1);''',
                '''typedef struct {const uint8_t *rgba;int width,height,rc;} RtVideoPresent;
static void rt_video_present(void *p){RtVideoPresent *v=p;v->rc=d8_video_draw_rgba(window,v->rgba,v->width,v->height,1);}
int thing_video_present_rgba(const uint8_t *rgba,int width,int height){
    batch_barrier();
    int rc;
    if(ds_rt_active()){RtVideoPresent job={rgba,width,height,-1};ds_rt_call(rt_video_present,&job);rc=job.rc;}
    else{if(!window||!context||SDL_GL_GetCurrentContext()!=context)return -1;rc=d8_video_draw_rgba(window,rgba,width,height,1);}''', 'movie present')
    d = replace(d, 'static void present(void){',
                'static void rt_process_exit(void){if(ds_rt_active())ds_rt_stop();}\nstatic void present(void){',
                'process exit helper')
    d = replace(d, '{int rt=ds_rt_start(window,context);',
                'atexit(rt_process_exit);{int rt=ds_rt_start(window,context);',
                'atexit registration')
    (DST / 'd3d8.c').write_text(d)

    r = (DST / 'd3d8_renderer.c').read_text()
    r = replace(r, '#include "d3d8_renderer.h"',
                '#include "d3d8_renderer.h"\n#include "render_thread_proxy.h"', 'renderer include')
    r = replace(r, 'e=glGetError();', 'e=ds_rt_defer_draw_error();', 'deferred draw error')
    (DST / 'd3d8_renderer.c').write_text(r)
    print('render-thread staged into', DST)


if __name__ == '__main__':
    main()
