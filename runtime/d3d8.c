/* 32-bit guest Direct3D8 COM facade, backed by SDL2 and native GLES.
 * API method order and structures follow the public Direct3D8 ABI. Host
 * pointers never enter guest objects: +8 holds a bounded host-table index.
 * Unimplemented entry points terminate with their synthetic method name. */
#define _POSIX_C_SOURCE 200809L
#include "win32hle.h"
#include "d3d8_renderer.h"
#include "adjacent_batch.h"
#include "d3d8_lights.h"
#include "d3d8_slots.h"
#include "fps_overlay.h"
#include "d3d8_video.h"
#include <GLES2/gl2ext.h>
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
extern void thing_cpu_profiler_frame(unsigned next_frame) __attribute__((weak));

static const char *setting(const char *name){
    if(!strncmp(name,"DS_",3)){char alias[96];snprintf(alias,sizeof alias,"THING_%s",name+3);const char *value=getenv(alias);if(value)return value;}
    return getenv(name);
}

#define OK 0u
#define INVALID 0x8876086cu
#define NOTAVAILABLE 0x8876086au
#define OUTOFMEMORY 0x8007000eu
#define MAX_OBJECTS DS_D3D8_SLOT_CAPACITY
#define MAX_LEVELS 16
#define TEX_BUDGET (192u<<20)
enum { ROOT=1, DEVICE, TEXTURE, SURFACE, VBUFFER, IBUFFER };
typedef struct {
    uint32_t self,kind,parent,device,width,height,format,pool,usage,length,fvf,levels,lod,priority,gpu_bytes;
    uint32_t memory[MAX_LEVELS],pitch[MAX_LEVELS],bytes[MAX_LEVELS],dirty[MAX_LEVELS];
    GLuint texture,depth,fbo,buffer;
    uint32_t buffer_dirty_start,buffer_dirty_end,buffer_locked;
    int backbuffer,gpu_dirty,gpu_origin,owns_memory;
} Object;
static Object objects[MAX_OBJECTS];
static D8Slots object_slots;
static uint32_t vtables[7],root_obj,device_obj,render_target,depth_target,backbuffer_obj;
static uint32_t bound_texture[8],stream_obj,stream_stride,index_obj,base_vertex;
static uint32_t presentation[13],clip_status[2],current_palette, palettes[16][256];
static uint64_t resource_bytes;
static SDL_Window *window;
static SDL_GLContext context;
static D8Renderer renderer;
/* Optional lower-than-guest render resolution: the game renders into this FBO
 * (scaled viewport/scissor, including UI) and Present blits it once to the window. Enabled with
 * DS_RENDER_WIDTH/DS_RENDER_HEIGHT; requires ES3 glBlitFramebuffer. */
static GLuint scale_fbo,scale_tex,scale_depth,scale_stencil;
static int scale_w,scale_h;
static float scale_factor=1.0f;
static GLuint scale_bound=(GLuint)-1;
static Object *object(uint32_t handle); /* defined below; used by target routing */
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif
#ifndef GL_DEPTH_STENCIL_ATTACHMENT
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#endif
#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 0x88F0
#endif
typedef void (*D8BlitFn)(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum);
static D8BlitFn blit_fn;
/* Optional render-worker hooks (strong symbols only in the threaded build). */
extern int ds_rt_active(void) __attribute__((weak));
extern int ds_rt_call(void (*fn)(void *),void *arg) __attribute__((weak));
/* Experimental owned-geometry queue. Disabled by default and with the render
 * worker/cache path; all queued GL work stays on the single context thread. */
static ThingBatch *draw_batch;
static uint64_t batch_inputs,batch_emits;
static int batch_emit(void *unused,const ThingBatchDraw *d){
    (void)unused;D8NativeBuffers native={.default_framebuffer=d->default_framebuffer};batch_emits++;
    return d8_renderer_draw_native(&renderer,d->state,d->target_height,d->primitive,d->count,d->vertices,d->stride,d->vcount,d->indices,d->index_size,d->base,d->textures,&native);
}
static void batch_barrier(void){
    if(draw_batch&&thing_batch_flush(draw_batch))hle_fatal("D3D8 experimental batch flush: %s",d8_renderer_error());
}
static int batch_requested(void){
    static int enabled=-1;if(enabled<0){const char *v=setting("DS_DRAW_BATCH");enabled=v&&!strcmp(v,"1");}
    return enabled&&!(ds_rt_active&&ds_rt_active());
}
static void destroy_scale_target(void){
    batch_barrier();
    if(scale_fbo)glDeleteFramebuffers(1,&scale_fbo);
    if(scale_tex)glDeleteTextures(1,&scale_tex);
    if(scale_depth)glDeleteRenderbuffers(1,&scale_depth);
    scale_fbo=scale_tex=scale_depth=0;scale_w=scale_h=0;scale_factor=1.0f;
}
static int create_scale_target(int gw,int gh){
    const char *vw=setting("DS_RENDER_WIDTH"),*vh=setting("DS_RENDER_HEIGHT");
    int w=vw?atoi(vw):0,h=vh?atoi(vh):0;
    if(w<160||h<120||w>=gw||h>=gh){d8_renderer_set_output_scale(&renderer,1.0f);return 0;}
    if(!blit_fn){blit_fn=(D8BlitFn)SDL_GL_GetProcAddress("glBlitFramebuffer");if(!blit_fn){fprintf(stderr,"[d3d8] glBlitFramebuffer unavailable; render scale disabled\n");return 0;}}
    glGenTextures(1,&scale_tex);glBindTexture(GL_TEXTURE_2D,scale_tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1,&scale_depth);glBindRenderbuffer(GL_RENDERBUFFER,scale_depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,w,h);
    glGenFramebuffers(1,&scale_fbo);glBindFramebuffer(GL_FRAMEBUFFER,scale_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,scale_tex,0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,scale_depth);
    int ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER,0);glBindRenderbuffer(GL_RENDERBUFFER,0);glBindTexture(GL_TEXTURE_2D,0);
    if(!ok){fprintf(stderr,"[d3d8] render scale %dx%d framebuffer incomplete; using guest resolution\n",w,h);destroy_scale_target();return 0;}
    scale_w=w;scale_h=h;scale_factor=(float)h/(float)gh;d8_renderer_set_output_scale(&renderer,scale_factor);
    glBindFramebuffer(GL_FRAMEBUFFER,scale_fbo);scale_bound=scale_fbo; /* game renders here until Present */
    fprintf(stderr,"[d3d8] render scale %dx%d (%.3f of %dx%d backbuffer)\n",w,h,(double)scale_factor,gw,gh);
    return 0;
}
static void scale_blit_now(void){
    if(!scale_fbo)return;
    GLint dw=0,dh=0;SDL_GL_GetDrawableSize(window,&dw,&dh);
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,scale_fbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
    blit_fn(0,0,scale_w,scale_h,0,0,dw,dh,GL_COLOR_BUFFER_BIT,GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,dw,dh);scale_bound=0;
}
static void scale_blit_job(void *unused){(void)unused;scale_blit_now();}
/* Render routing: game frames must land in the scaled FBO whenever the guest
 * target is the backbuffer, even if another pass (movie, overlay, readback)
 * changed the GL framebuffer binding outside bind_target(). */
static void bind_game_target(void){
    if(!scale_fbo)return;
    Object *t=object(render_target);
    if(!t)return;
    GLuint want=t->backbuffer?scale_fbo:t->fbo;
    float output=t->backbuffer?scale_factor:1.0f;
    if(renderer.output_scale!=output){d8_renderer_set_output_scale(&renderer,output);d8_renderer_invalidate_raster(&renderer);}
    if(want!=scale_bound){glBindFramebuffer(GL_FRAMEBUFFER,want);scale_bound=want;}
}
int thing_video_present_rgba(const uint8_t *rgba,int width,int height){
    batch_barrier();
    if(!window||!context||SDL_GL_GetCurrentContext()!=context)return -1;
    int rc=d8_video_draw_rgba(window,rgba,width,height,1);
    scale_bound=(GLuint)-1;
    d8_renderer_invalidate_raster(&renderer);
    d8_renderer_invalidate_attributes(&renderer);
    d8_renderer_invalidate_texture_parameters(&renderer);
    return rc;
}
static D8State state;
static D8LightBank light_bank;
static uint32_t active_light_ids[8];
static void pack_lights(void){ds_d8_lights_pack(&light_bank,state.light,state.light_on,active_light_ids);}
typedef struct {
    D8State value;
    D8LightBank lights;
    D8LightMask light_mask;
    uint8_t mask[sizeof(D8State)], bindings[10];
    uint32_t textures[8],stream,stride,index,base;
} StateBlock;
static StateBlock *blocks[128],*record_start;
static uint8_t record_mask[sizeof(D8State)],record_bindings[10];
static D8LightMask record_light_mask;
static void record_state(const void *value,size_t bytes){
    if(!record_start)return;
    size_t offset=(const uint8_t*)value-(const uint8_t*)&state;
    if(offset+bytes>sizeof state)hle_fatal("invalid D3D8 state record");
    memset(record_mask+offset,1,bytes);
}
static void record_binding(unsigned binding){if(record_start&&binding<10)record_bindings[binding]=1;}
static unsigned frames;
/* Opt-in live diagnostics keep the current mission intact while comparing
 * CPU/GPU vertex paths. Commands are consumed once at a Present boundary. */
static int diagnostic_cpu,diagnostic_finish,buffer_cache=-1;
static int cache_buffers(void){if(buffer_cache<0){const char *v=setting("DS_GPU_BUFFER_CACHE");buffer_cache=v&&!strcmp(v,"1");}return buffer_cache;}
static FILE *diagnostic_frame;
static uint32_t diagnostic_textures[8];
static unsigned diagnostic_texture_count,diagnostic_draw_count,diagnostic_draw_limit=64;
extern void ds_gl_cost_present(void) __attribute__((weak));
extern void ds_gl_cost_flush(void) __attribute__((weak));
extern int ds_gl_cost_begin(unsigned frames) __attribute__((weak));
static unsigned gl_cost_requested;
static void diagnostic_poll(void){
    if(diagnostic_frame){fclose(diagnostic_frame);diagnostic_frame=NULL;}
    const char *enabled=setting("DS_CAPTURE_REQUEST"),*root=setting("DS_ROOT");
    if(!enabled||strcmp(enabled,"1")||!root)return;
    char path[PATH_MAX],command[32];
    if(snprintf(path,sizeof path,"%s/render.control",root)>=(int)sizeof path)return;
    FILE *f=fopen(path,"r");if(!f)return;
    int got=fscanf(f,"%31s",command);fclose(f);unlink(path);if(got!=1)return;
    if(!strcmp(command,"cpu"))diagnostic_cpu=1;
    else if(!strcmp(command,"gpu"))diagnostic_cpu=0;
    else if(!strcmp(command,"buffers"))buffer_cache=1;
    else if(!strcmp(command,"nobuffers"))buffer_cache=0;
    else if(!strcmp(command,"glcost"))gl_cost_requested=120;
    else if(!strcmp(command,"glcoststop")&&ds_gl_cost_flush)ds_gl_cost_flush();
    else if(!strcmp(command,"trim"))renderer.trim_vertex_prefix=1;
    else if(!strcmp(command,"notrim"))renderer.trim_vertex_prefix=0;
    else if(!strcmp(command,"finish"))diagnostic_finish=1;
    else if(!strcmp(command,"nofinish"))diagnostic_finish=0;
    else if(!strcmp(command,"trace")){
        diagnostic_texture_count=diagnostic_draw_count=0;
        const char *limit=setting("DS_TRACE_DRAW_LIMIT");unsigned requested=limit?(unsigned)strtoul(limit,NULL,10):64;
        diagnostic_draw_limit=requested>=1&&requested<=4096?requested:64;
        if(snprintf(path,sizeof path,"%s/render-frame.txt",root)<(int)sizeof path)diagnostic_frame=fopen(path,"w");
    }
    fprintf(stderr,"[d3d8-diagnostic] frame=%u command=%s cpu=%d finish=%d trim=%d\n",frames,command,diagnostic_cpu,diagnostic_finish,renderer.trim_vertex_prefix);
}
static void diagnostic_draw(unsigned primitive,unsigned count){
    if(!diagnostic_frame)return;
    if(diagnostic_draw_count++>=diagnostic_draw_limit){fprintf(diagnostic_frame,"trace truncated after %u draws\n",diagnostic_draw_limit);fclose(diagnostic_frame);diagnostic_frame=NULL;return;}
    fprintf(diagnostic_frame,"draw frame=%u guest=%#x primitive=%u count=%u fvf=%#x rt=%#x depth=%#x textures=%#x,%#x\n",frames+1,g_cur_func,primitive,count,state.fvf,render_target,depth_target,bound_texture[0],bound_texture[1]);
    fputs("rs",diagnostic_frame);for(unsigned i=0;i<256;i++)fprintf(diagnostic_frame," %u:%08x",i,state.rs[i]);fputc('\n',diagnostic_frame);
    for(unsigned t=0;t<2;t++){fprintf(diagnostic_frame,"tss%u",t);for(unsigned i=0;i<32;i++)fprintf(diagnostic_frame," %u:%08x",i,state.tss[t][i]);fputc('\n',diagnostic_frame);}
    fputs("material",diagnostic_frame);for(unsigned i=0;i<17;i++)fprintf(diagnostic_frame," %.9g",state.material[i]);fputc('\n',diagnostic_frame);
    fprintf(diagnostic_frame,"viewport %u %u %u %u %08x %08x\n",state.viewport[0],state.viewport[1],state.viewport[2],state.viewport[3],state.viewport[4],state.viewport[5]);
    for(unsigned l=0;l<8;l++)if(state.light_on[l]){fprintf(diagnostic_frame,"light%u logical_id=%u",l,active_light_ids[l]);for(unsigned j=0;j<26;j++)fprintf(diagnostic_frame," %.9g",state.light[l][j]);fputc('\n',diagnostic_frame);}
    const unsigned matrices[]={2,3,16,17,256};for(unsigned m=0;m<5;m++){fprintf(diagnostic_frame,"matrix%u",matrices[m]);for(unsigned j=0;j<16;j++)fprintf(diagnostic_frame," %.9g",state.transform[matrices[m]][j]);fputc('\n',diagnostic_frame);}

}
typedef struct {
    uint64_t draws,accepted,rejected,primitives,vertices,clears,uploads,upload_levels,upload_bytes;
    double submit_ms,texture_decode_cpu_ms,texture_decode_wall_ms,dxt_decode_cpu_ms,dxt_decode_wall_ms,texture_upload_cpu_ms,texture_upload_wall_ms;
    uint64_t dxt_decode_levels;
    uint32_t last_guest,last_fvf;
} RenderCounters;
static RenderCounters render_frame;
static int render_trace=-1;
static FILE *render_csv;
static unsigned render_log_lines,render_error_lines;
static int render_seen_draw,render_seen_reject;
static int trace_render(void){
    if(render_trace<0){const char *s=setting("DS_TRACE_RENDER");render_trace=s&&!strcmp(s,"1");if(render_trace){const char *root=setting("DS_ROOT");char path[PATH_MAX];if(root&&snprintf(path,sizeof path,"%s/render.csv",root)<(int)sizeof path)render_csv=fopen(path,"w");if(render_csv){fputs("frame,draws,accepted,rejected,primitives,vertices,clears,upload_calls,uploaded_levels,uploaded_rgba_bytes,draw_cpu_submit_ms,swap_ms,last_draw_guest,last_fvf,render_target,backbuffer,texture_decode_cpu_ms,texture_decode_wall_ms,dxt_decode_cpu_ms,dxt_decode_wall_ms,texture_upload_cpu_ms,texture_upload_wall_ms,dxt_decode_levels\n",render_csv);fflush(render_csv);}}}
    return render_trace;
}
static void render_rejected(const char *method,const char *reason){
    if(render_error_lines++>=16)return;
    fprintf(stderr,"[d3d8-render-error] method=%s frame=%u guest=%#x reason=%s fvf=%#x viewport=%u,%u,%u,%u rt=%#x backbuffer=%#x textures=%#x,%#x RS(fill=%u z=%u zw=%u zfunc=%u cull=%u alpha=%u lighting=%u vblend=%u iblend=%u clip=%#x colorwrite=%#x)\n",method,frames+1,g_cur_func,reason,state.fvf,state.viewport[0],state.viewport[1],state.viewport[2],state.viewport[3],render_target,backbuffer_obj,bound_texture[0],bound_texture[1],state.rs[8],state.rs[7],state.rs[14],state.rs[23],state.rs[22],state.rs[27],state.rs[137],state.rs[151],state.rs[167],state.rs[152],state.rs[168]);
    for(int i=0;i<2;i++)fprintf(stderr,"[d3d8-render-error] stage=%d colorop=%u colorargs=%#x,%#x alphaop=%u alphaargs=%#x,%#x texcoordindex=%#x transform=%#x\n",i,state.tss[i][1],state.tss[i][2],state.tss[i][3],state.tss[i][4],state.tss[i][5],state.tss[i][6],state.tss[i][11],state.tss[i][24]);
}
static int draw_failed(const char *reason){if(trace_render())render_frame.rejected++;render_rejected("draw",reason);return -1;}
static void trace_draw_start(void){if(trace_render()){render_frame.draws++;render_frame.last_guest=g_cur_func;render_frame.last_fvf=state.fvf;}}
static void trace_present(double swap_ms){
    D8UploadStats buffers=d8_renderer_take_upload_stats(&renderer);
    if(!trace_render())return;
    if(frames%120==0)fprintf(stderr,"[d3d8-buffer] frame=%u vertex_bytes=%llu index_bytes=%llu original_prefix_bytes=%llu avoidable_prefix_bytes=%llu trimmed_draws=%llu native_vertex_draws=%llu native_index_draws=%llu\n",frames,(unsigned long long)buffers.vertex_bytes,(unsigned long long)buffers.index_bytes,(unsigned long long)buffers.prefix_vertex_bytes,(unsigned long long)buffers.avoidable_prefix_bytes,(unsigned long long)buffers.trimmed_draws,(unsigned long long)buffers.native_vertex_draws,(unsigned long long)buffers.native_index_draws);
    if(render_csv)fprintf(render_csv,"%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%.6f,%.6f,%#x,%#x,%#x,%#x,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%llu\n",frames,(unsigned long long)render_frame.draws,(unsigned long long)render_frame.accepted,(unsigned long long)render_frame.rejected,(unsigned long long)render_frame.primitives,(unsigned long long)render_frame.vertices,(unsigned long long)render_frame.clears,(unsigned long long)render_frame.uploads,(unsigned long long)render_frame.upload_levels,(unsigned long long)render_frame.upload_bytes,render_frame.submit_ms,swap_ms,render_frame.last_guest,render_frame.last_fvf,render_target,backbuffer_obj,render_frame.texture_decode_cpu_ms,render_frame.texture_decode_wall_ms,render_frame.dxt_decode_cpu_ms,render_frame.dxt_decode_wall_ms,render_frame.texture_upload_cpu_ms,render_frame.texture_upload_wall_ms,(unsigned long long)render_frame.dxt_decode_levels);
    if(render_log_lines<256&&(frames<=8||frames%120==0||(render_frame.draws&&!render_seen_draw)||(render_frame.rejected&&!render_seen_reject))){render_log_lines++;GLint fbo=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&fbo);fprintf(stderr,"[d3d8-render] frame=%u draws=%llu accepted=%llu rejected=%llu primitives=%llu vertices=%llu clears=%llu uploads=%llu upload_levels=%llu upload_bytes=%llu submit_ms=%.3f swap_ms=%.3f last_draw=%#x fvf=%#x fbo=%d rt=%#x backbuffer=%#x\n",frames,(unsigned long long)render_frame.draws,(unsigned long long)render_frame.accepted,(unsigned long long)render_frame.rejected,(unsigned long long)render_frame.primitives,(unsigned long long)render_frame.vertices,(unsigned long long)render_frame.clears,(unsigned long long)render_frame.uploads,(unsigned long long)render_frame.upload_levels,(unsigned long long)render_frame.upload_bytes,render_frame.submit_ms,swap_ms,render_frame.last_guest,render_frame.last_fvf,fbo,render_target,backbuffer_obj);}
    render_seen_draw|=render_frame.draws!=0;render_seen_reject|=render_frame.rejected!=0;if(render_csv&&(frames%30==0||render_frame.rejected))fflush(render_csv);memset(&render_frame,0,sizeof render_frame);
}
static uint64_t profile_start,profile_last;
static double *frame_ms;
static size_t frame_count,frame_capacity;
static FILE *profile_csv;
/* Explicit diagnostic snapshots flush only at an ordinary input-pump
 * boundary, never from a signal handler or on every gameplay frame. */
/* Optional copy accounting for the gameplay CPU investigation. Counters are
 * normally zero-cost (a branch on g_copy_diag); THING_COPY_STATS=1 enables them
 * and each SIGUSR1 flush prints a cumulative line so interval deltas can be
 * taken by the external profiler script. thing_copy_totals is supplied by the
 * optional LD_PRELOAD tracer in tools/copy_total_trace.c. */
static uint64_t copy_blit_calls,copy_blit_bytes,copy_sync_calls,copy_sync_bytes,copy_decode_calls,copy_decode_bytes;
__attribute__((constructor)) static void copy_diag_init(void){const char *v=getenv("THING_COPY_STATS");g_copy_diag=v&&!strcmp(v,"1");}
void ds_graphics_profile_flush(void){
    if(g_copy_diag){
        uint64_t mmc=0,mmb=0,mcc=0,mcb=0,msc=0,msb=0;
        void thing_copy_totals(uint64_t*,uint64_t*,uint64_t*,uint64_t*,uint64_t*,uint64_t*) __attribute__((weak));
        if(thing_copy_totals)thing_copy_totals(&mmc,&mmb,&mcc,&mcb,&msc,&msb);
        fprintf(stderr,"[copy-stats] rep_calls=%llu rep_bytes=%llu blit_calls=%llu blit_bytes=%llu sync_calls=%llu sync_bytes=%llu decode_calls=%llu decode_bytes=%llu libc_memmove_calls=%llu libc_memmove_bytes=%llu libc_memcpy_calls=%llu libc_memcpy_bytes=%llu libc_memset_calls=%llu libc_memset_bytes=%llu\n",
            (unsigned long long)g_copy_rep_calls,(unsigned long long)g_copy_rep_bytes,
            (unsigned long long)copy_blit_calls,(unsigned long long)copy_blit_bytes,
            (unsigned long long)copy_sync_calls,(unsigned long long)copy_sync_bytes,
            (unsigned long long)copy_decode_calls,(unsigned long long)copy_decode_bytes,
            (unsigned long long)mmc,(unsigned long long)mmb,(unsigned long long)mcc,(unsigned long long)mcb,(unsigned long long)msc,(unsigned long long)msb);
    }
    if(profile_csv)fflush(profile_csv);if(render_csv)fflush(render_csv);
}
static double profile_seconds;
static long profile_frames;
/* Optional frame cap: 0 = no cap, 30 or 60 = milliseconds per frame target. */
static double frame_limit_ms;
static uint64_t last_present_tick;
static int profile_reported;
static void (*previous_exit_hook)(int);
static int compare_double(const void *a,const void *b){double x=*(const double*)a,y=*(const double*)b;return (x>y)-(x<y);}
static void profile_report(void){
    if(!frame_count||profile_reported)return;
    profile_reported=1;
    double *sorted=malloc(frame_count*sizeof(double));if(!sorted)return;memcpy(sorted,frame_ms,frame_count*sizeof(double));qsort(sorted,frame_count,sizeof(double),compare_double);
    double sum=0,slow=0;unsigned s33=0,s50=0,s100=0;for(size_t i=0;i<frame_count;i++){sum+=frame_ms[i];s33+=frame_ms[i]>33.333333;s50+=frame_ms[i]>50;s100+=frame_ms[i]>100;}
    size_t tail=(frame_count+99)/100;for(size_t i=frame_count-tail;i<frame_count;i++)slow+=sorted[i];
    fprintf(stderr,"[d3d8-profile] frames=%zu seconds=%.3f avg_fps=%.3f low_1pct_fps=%.3f spikes_gt33ms=%u spikes_gt50ms=%u spikes_gt100ms=%u\n",frame_count,sum/1000,frame_count*1000.0/sum,tail*1000.0/slow,s33,s50,s100);
    d8_renderer_report_gl_stats(&renderer);
    free(sorted);if(profile_csv)fflush(profile_csv);
}
static void profile_exit(int code){profile_report();if(profile_csv){fclose(profile_csv);profile_csv=NULL;}if(render_csv){fclose(render_csv);render_csv=NULL;}if(previous_exit_hook)previous_exit_hook(code);}
static void profile_frame(void){
    uint64_t now=SDL_GetPerformanceCounter(),freq=SDL_GetPerformanceFrequency();
    if(!profile_start){profile_start=profile_last=now;const char *seconds=setting("DS_PROFILE_SECONDS");if(seconds){char *end;double s=strtod(seconds,&end);if(end!=seconds&&!*end&&isfinite(s)&&s>0)profile_seconds=s;}
        const char *frame_limit=setting("DS_PROFILE_FRAMES");if(frame_limit){char *end;long n=strtol(frame_limit,&end,10);if(end!=frame_limit&&!*end&&n>0)profile_frames=n;}
        const char *root=setting("DS_ROOT");if(root){char path[PATH_MAX];if(snprintf(path,sizeof path,"%s/profile.csv",root)<(int)sizeof path)profile_csv=fopen(path,"w");}
        if(profile_csv){fputs("frame,elapsed_seconds,frame_ms\n",profile_csv);fflush(profile_csv);}previous_exit_hook=hle_on_exit;hle_on_exit=profile_exit;atexit(profile_report);return;}
    double dt=(now-profile_last)*1000.0/freq;profile_last=now;double elapsed=(now-profile_start)/(double)freq;
    if(frame_count<2000000){if(frame_count==frame_capacity){size_t cap=frame_capacity?frame_capacity*2:4096;void *p=realloc(frame_ms,cap*sizeof(double));if(p){frame_ms=p;frame_capacity=cap;}}
        if(frame_count<frame_capacity)frame_ms[frame_count++]=dt;}
    if(profile_csv)fprintf(profile_csv,"%u,%.6f,%.6f\n",frames,elapsed,dt);
    if(frames%600==0){double sum=0;size_t n=frame_count<600?frame_count:600;for(size_t i=frame_count-n;i<frame_count;i++)sum+=frame_ms[i];fprintf(stderr,"[d3d8-profile] window_frames=%zu elapsed=%.3f avg_fps=%.3f\n",n,elapsed,n*1000.0/sum);if(profile_csv)fflush(profile_csv);}
    if(profile_seconds>0&&elapsed>=profile_seconds){profile_report();hle_exit(0);}
    if(profile_frames>0&&(long)frames>=profile_frames){profile_report();hle_exit(0);}
}
static void capture_frame(unsigned frame){
    static int initialized,request_enabled,request_errors;
    static unsigned wanted;
    static char request_path[PATH_MAX];
    if(!initialized){
        initialized=1;const char *value=setting("DS_CAPTURE_FRAME"),*requests=setting("DS_CAPTURE_REQUEST"),*root=setting("DS_ROOT");
        if(value){char *end;unsigned long n=strtoul(value,&end,10);if(end!=value&&!*end&&n>0&&n<=UINT32_MAX)wanted=(unsigned)n;}
        request_enabled=requests&&!strcmp(requests,"1")&&root&&snprintf(request_path,sizeof request_path,"%s/capture.request",root)<(int)sizeof request_path;
    }
    int requested=0;
    if(request_enabled){if(!unlink(request_path))requested=1;else if(errno!=ENOENT&&request_errors++<4)fprintf(stderr,"[d3d8] capture request could not be consumed: path=%s error=%s\n",request_path,strerror(errno));}
    if((!wanted||frame!=wanted)&&!requested)return;
    const char *root=setting("DS_ROOT");if(!root){fprintf(stderr,"[d3d8] capture skipped: DS_ROOT is unset\n");return;}
    char path[PATH_MAX];if(snprintf(path,sizeof path,"%s/capture.bmp",root)>=(int)sizeof path)return;
    int width,height;SDL_GL_GetDrawableSize(window,&width,&height);
    SDL_Surface *image=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);
    if(!image){fprintf(stderr,"[d3d8] capture allocation: %s\n",SDL_GetError());return;}
    GLint previous,pack;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);glGetIntegerv(GL_PACK_ALIGNMENT,&pack);glBindFramebuffer(GL_FRAMEBUFFER,0);
    glPixelStorei(GL_PACK_ALIGNMENT,4);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,image->pixels);GLenum error=glGetError();
    glPixelStorei(GL_PACK_ALIGNMENT,pack);glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous);
    if(!error){uint8_t *row=malloc(image->pitch);if(row){for(int y=0;y<height/2;y++){uint8_t *a=(uint8_t*)image->pixels+y*image->pitch,*b=(uint8_t*)image->pixels+(height-1-y)*image->pitch;memcpy(row,a,image->pitch);memcpy(a,b,image->pitch);memcpy(b,row,image->pitch);}free(row);if(SDL_SaveBMP(image,path))fprintf(stderr,"[d3d8] capture save failed: %s\n",SDL_GetError());else {
                fprintf(stderr,"[d3d8] captured game frame=%u source=%s path=%s size=%dx%d\n",frame,requested?"request":"frame-number",path,width,height);
                char metadata[PATH_MAX];
                if(snprintf(metadata,sizeof metadata,"%s/capture-frame.txt",root)<(int)sizeof metadata){
                    FILE *f=fopen(metadata,"w");if(f){fprintf(f,"frame=%u width=%d height=%d source=%s\n",frame,width,height,requested?"request":"frame-number");if(fclose(f))fprintf(stderr,"[d3d8] capture metadata write failed\n");}
                    else fprintf(stderr,"[d3d8] capture metadata open failed: %s\n",strerror(errno));
                }
            }}else fprintf(stderr,"[d3d8] capture row allocation failed\n");}
    else fprintf(stderr,"[d3d8] capture GL error %#x\n",error);
    SDL_FreeSurface(image);
}
static void init_tables(void);
extern void ds_input_pump(void);
static void pump(void){ ds_input_pump(); }
static float bits_float(uint32_t n){float f;memcpy(&f,&n,4);return f;}
static Object *object(uint32_t handle){if(!handle)return NULL;uint32_t i=MEM32(handle+8);return i>0&&i<MAX_OBJECTS&&objects[i].self==handle?&objects[i]:NULL;}
static Object *self(void){Object *o=object(A32(0));if(!o)hle_fatal("D3D8 method on invalid object %#x",A32(0));return o;}
static void out_handle(unsigned arg,uint32_t value){if(!A32(arg))hle_fatal("D3D8 null out pointer in %s",g_cur_import?g_cur_import:"method");MEM32(A32(arg))=value;}
static uint32_t make_object(uint32_t kind){
    uint32_t i=ds_d8_slot_take(&object_slots);if(!i)return 0;
    uint32_t *guest=hle_alloc(12);if(!guest){ds_d8_slot_put(&object_slots,i);return 0;}
    uint32_t o=hle_va(guest);guest[0]=vtables[kind];guest[1]=1;guest[2]=i;
    memset(&objects[i],0,sizeof objects[i]);objects[i].self=o;objects[i].kind=kind;objects[i].device=device_obj;return o;
}
static void addref(uint32_t o){if(o)MEM32(o+4)++;}
static void release(uint32_t handle){Object *o=object(handle);if(!o)return;if(MEM32(handle+4)&&--MEM32(handle+4))return;batch_barrier();uint32_t parent=o->parent;
    if(o->owns_memory)for(unsigned l=0;l<o->levels;l++)if(o->memory[l]){hle_free(hle_ptr(o->memory[l]));resource_bytes-=o->bytes[l];}
    resource_bytes-=o->gpu_bytes;if(o->texture){d8_renderer_forget_texture(&renderer,o->texture);d8_renderer_invalidate_bindings(&renderer);glDeleteTextures(1,&o->texture);}if(o->depth)glDeleteRenderbuffers(1,&o->depth);if(o->fbo)glDeleteFramebuffers(1,&o->fbo);if(o->buffer){d8_renderer_invalidate_attributes(&renderer);d8_renderer_invalidate_bindings(&renderer);glDeleteBuffers(1,&o->buffer);}
    if(handle==device_obj){thing_batch_destroy(draw_batch);draw_batch=NULL;device_obj=0;destroy_scale_target();ds_fps_overlay_close();d8_video_close();d8_renderer_close(&renderer);if(context)SDL_GL_DeleteContext(context);if(window)SDL_DestroyWindow(window);window=NULL;context=NULL;hle_set_pump_hook(NULL);}
    uint32_t slot=(uint32_t)(o-objects);memset(o,0,sizeof(*o));if(ds_d8_slot_put(&object_slots,slot))hle_fatal("D3D8 object slot recycling failed: %u",slot);hle_com_free(handle);if(parent)release(parent);
}
static void unsupported(void){hle_fatal("D3D8 unsupported method %s, object=%#x",g_cur_import?g_cur_import:"unknown",A32(0));}
static void qi(void){Object *o=self();const uint32_t id=A32(1)?MEM32(A32(1)):UINT32_MAX;uint32_t good=id==0 ||
    (o->kind==ROOT&&id==0x1dd9e8da)||(o->kind==DEVICE&&id==0x7385e5df)||
    (o->kind==TEXTURE&&(id==0xe4cdd575||id==0xb4211cfa||id==0x1b36bb7b))||
    (o->kind==SURFACE&&id==0xb96eebca)||(o->kind==VBUFFER&&id==0x8aeeeac7)||(o->kind==IBUFFER&&id==0x0e689c9a);
    out_handle(2,good?o->self:0);if(good)addref(o->self);RET(good?OK:0x80004002u,3);}
static void ref(void){self();uint32_t n=++MEM32(A32(0)+4);RET(n,1);}
static void unref(void){uint32_t h=A32(0),n=MEM32(h+4);release(h);RET(n?n-1:0,1);}
static unsigned bpp(unsigned f){switch(f){case 20:return 3;case 21:case 22:return 4;case 23:case 24:case 25:case 26:case 51:return 2;case 28:case 41:case 50:return 1;default:return 0;}}
static unsigned block_size(unsigned f){return f==0x31545844u?8:f==0x33545844u||f==0x35545844u?16:0;}
static int depth_format(unsigned f){return f==75||f==77||f==80;}
static int supported_format(unsigned f){return bpp(f)||block_size(f)||depth_format(f);}
static unsigned mipdim(unsigned size,unsigned l){return (size>>l)?size>>l:1;}
/* Error-only resource diagnostics: bounded even if a game retries allocation.
 * No GL queries, object scans or environment parsing on successful requests. */
static unsigned texture_allocation_errors;
static void texture_allocation_failure(const char *reason,unsigned w,unsigned h,unsigned fmt,unsigned levels,unsigned usage,unsigned pool,unsigned level,uint64_t requested){
    if(texture_allocation_errors>=32)return;
    ++texture_allocation_errors;
    fprintf(stderr,"[d3d8-texture-allocation] reason=%s size=%ux%u format=%u levels=%u usage=%#x pool=%u mip=%u requested_bytes=%llu tracked=%llu budget=%u guest=%#x\n",reason,w,h,fmt,levels,usage,pool,level,(unsigned long long)requested,(unsigned long long)resource_bytes,TEX_BUDGET,g_cur_func);
    if(texture_allocation_errors==32)fprintf(stderr,"[d3d8-texture-allocation] further failures suppressed\n");
}
static int pixel_allocation_failure(Object *o,const char *reason,unsigned level,uint64_t bytes){
    texture_allocation_failure(reason,o->width,o->height,o->format,o->levels,o->usage,o->pool,level,bytes);return -1;
}
static int allocate_pixels(Object *o){
    o->owns_memory=1;
    for(unsigned l=0;l<o->levels;l++){
        unsigned w=mipdim(o->width,l),h=mipdim(o->height,l),bs=block_size(o->format);
        o->pitch[l]=bs?((w+3)/4)*bs:w*bpp(o->format);o->bytes[l]=o->pitch[l]*(bs?(h+3)/4:h);
        if(!o->bytes[l])return pixel_allocation_failure(o,"zero-mip-bytes",l,0);
        if(resource_bytes+o->bytes[l]>TEX_BUDGET)return pixel_allocation_failure(o,"cpu-resource-budget",l,o->bytes[l]);
        void *m=hle_alloc(o->bytes[l]);if(!m)return pixel_allocation_failure(o,"guest-heap",l,o->bytes[l]);
        o->memory[l]=hle_va(m);o->dirty[l]=1;resource_bytes+=o->bytes[l];
    }
    if(o->kind==TEXTURE){
        uint64_t gpu=0;for(unsigned l=0;l<o->levels;l++)gpu+=(uint64_t)mipdim(o->width,l)*mipdim(o->height,l)*4;
        if(resource_bytes+gpu>TEX_BUDGET)return pixel_allocation_failure(o,"gpu-resource-budget",0,gpu);
        o->gpu_bytes=(uint32_t)gpu;resource_bytes+=gpu;
    }
    return 0;
}
static void rgb565(uint16_t c,uint8_t out[4]){out[0]=(uint8_t)(((c>>11)&31)*255/31);out[1]=(uint8_t)(((c>>5)&63)*255/63);out[2]=(uint8_t)((c&31)*255/31);out[3]=255;}
static void decode_dxt(const uint8_t *src,unsigned fmt,uint8_t out[16][4]){
    unsigned bs=block_size(fmt);const uint8_t *colors=src+(bs==16?8:0);uint16_t c0,c1;memcpy(&c0,colors,2);memcpy(&c1,colors+2,2);uint8_t table[4][4];rgb565(c0,table[0]);rgb565(c1,table[1]);
    for(unsigned c=0;c<3;c++){table[2][c]=(uint8_t)((2*table[0][c]+table[1][c])/3);table[3][c]=(uint8_t)((table[0][c]+2*table[1][c])/3);}table[2][3]=table[3][3]=255;
    if(bs==8&&c0<=c1){for(unsigned c=0;c<3;c++){table[2][c]=(uint8_t)((table[0][c]+table[1][c])/2);table[3][c]=0;}table[3][3]=0;}
    uint32_t code;memcpy(&code,colors+4,4);for(unsigned i=0;i<16;i++)memcpy(out[i],table[(code>>(2*i))&3],4);
    if(fmt==0x33545844u){uint64_t alpha;memcpy(&alpha,src,8);for(unsigned i=0;i<16;i++)out[i][3]=(uint8_t)(((alpha>>(4*i))&15)*17);}
    if(fmt==0x35545844u){uint8_t a[8]={src[0],src[1]};if(a[0]>a[1])for(unsigned i=2;i<8;i++)a[i]=(uint8_t)(((8-i)*a[0]+(i-1)*a[1])/7);else{for(unsigned i=2;i<6;i++)a[i]=(uint8_t)(((6-i)*a[0]+(i-1)*a[1])/5);a[6]=0;a[7]=255;}uint64_t bits=0;for(unsigned i=0;i<6;i++)bits|=(uint64_t)src[2+i]<<(8*i);for(unsigned i=0;i<16;i++)out[i][3]=a[(bits>>(3*i))&7];}
}
static uint8_t *rgba_pixels(Object *o,unsigned level){unsigned w=mipdim(o->width,level),h=mipdim(o->height,level),bs=block_size(o->format);uint8_t *rgba=malloc((size_t)w*h*4);if(!rgba)return NULL;const uint8_t *p=hle_ptr(o->memory[level]);
    if(bs){for(unsigned by=0;by<(h+3)/4;by++)for(unsigned bx=0;bx<(w+3)/4;bx++){uint8_t block[16][4];decode_dxt(p+by*o->pitch[level]+bx*bs,o->format,block);for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++)if(bx*4+x<w&&by*4+y<h)memcpy(rgba+((size_t)(by*4+y)*w+bx*4+x)*4,block[y*4+x],4);}return rgba;}
    unsigned size=bpp(o->format);for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){const uint8_t *s=p+y*o->pitch[level]+x*size;uint8_t *d=rgba+((size_t)y*w+x)*4;uint16_t v=0;if(size==2)memcpy(&v,s,2);
        switch(o->format){case 20:case 21:case 22:d[0]=s[2];d[1]=s[1];d[2]=s[0];d[3]=o->format==21?s[3]:255;break;case 23:rgb565(v,d);break;
        case 24:case 25:d[0]=(uint8_t)(((v>>10)&31)*255/31);d[1]=(uint8_t)(((v>>5)&31)*255/31);d[2]=(uint8_t)((v&31)*255/31);d[3]=o->format==24||v&0x8000?255:0;break;
        case 26:d[0]=(uint8_t)(((v>>8)&15)*17);d[1]=(uint8_t)(((v>>4)&15)*17);d[2]=(uint8_t)((v&15)*17);d[3]=(uint8_t)((v>>12)*17);break;
        case 28:d[0]=d[1]=d[2]=255;d[3]=s[0];break;case 50:d[0]=d[1]=d[2]=s[0];d[3]=255;break;case 51:d[0]=d[1]=d[2]=s[0];d[3]=s[1];break;
        case 41:{uint32_t c=palettes[current_palette&15][s[0]];d[0]=(uint8_t)c;d[1]=(uint8_t)(c>>8);d[2]=(uint8_t)(c>>16);d[3]=255;break;}default:free(rgba);return NULL;}}
    return rgba;
}
/* Trace-only CPU backing snapshots: never read a GPU target, mutate resource
 * state or impose a per-frame readback stall. Raw RGBA preserves atlas alpha. */
static void diagnostic_texture(Object *o){
    if(!diagnostic_frame||!o)return;
    for(unsigned i=0;i<diagnostic_texture_count;i++)if(diagnostic_textures[i]==o->self)return;
    if(diagnostic_texture_count>=8)return;
    diagnostic_textures[diagnostic_texture_count++]=o->self;
    if(o->kind!=TEXTURE||!o->memory[0]||!o->width||!o->height||o->width>512||o->height>512||o->gpu_dirty){
        fprintf(diagnostic_frame,"texture-snapshot handle=%#x skipped=size-or-missing-or-stale-CPU-backing\n",o->self);return;
    }
    const char *root=setting("DS_ROOT");char path[PATH_MAX];
    if(!root||snprintf(path,sizeof path,"%s/render-texture-%u-%08x.rgba",root,frames+1,o->self)>=(int)sizeof path)return;
    uint8_t *px=rgba_pixels(o,0);if(!px){fprintf(diagnostic_frame,"texture-snapshot handle=%#x decode-failed\n",o->self);return;}
    size_t bytes=(size_t)o->width*o->height*4;uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<bytes;i++){hash^=px[i];hash*=UINT64_C(1099511628211);}
    FILE *f=fopen(path,"wb");int ok=0;if(f){ok=fwrite(px,1,bytes,f)==bytes;if(fclose(f))ok=0;}
    fprintf(diagnostic_frame,"texture-snapshot handle=%#x file=%s width=%u height=%u layout=top-down-RGBA8 bytes=%zu fnv1a64=%016llx saved=%d\n",o->self,path,o->width,o->height,bytes,(unsigned long long)hash,ok);
    free(px);
}
static void diagnostic_vertex(const unsigned char *v,unsigned bytes,unsigned index){
    unsigned need=d8_fvf_stride(state.fvf);if(!need||need>bytes)return;
    float xyz[4]={0};unsigned pos=(state.fvf&14)==4?16:12;memcpy(xyz,v,pos);
    fprintf(diagnostic_frame,"decoded index=%u position=%.9g,%.9g,%.9g",index,xyz[0],xyz[1],xyz[2]);
    if(pos==16)fprintf(diagnostic_frame," rhw=%.9g",xyz[3]);
    unsigned offset=pos;if(state.fvf&16)offset+=12;if(state.fvf&32)offset+=4;
    if(state.fvf&64){uint32_t color;memcpy(&color,v+offset,4);fprintf(diagnostic_frame," diffuse=%08x",color);offset+=4;}
    if(state.fvf&128){uint32_t color;memcpy(&color,v+offset,4);fprintf(diagnostic_frame," specular=%08x",color);offset+=4;}
    unsigned sets=(state.fvf>>8)&15;
    for(unsigned t=0;t<sets;t++){unsigned code=(state.fvf>>(16+2*t))&3,dim=code==0?2:code==1?3:code==2?4:1;float uv[4];memcpy(uv,v+offset,dim*4);offset+=dim*4;fprintf(diagnostic_frame," texcoord%u=",t);for(unsigned d=0;d<dim;d++)fprintf(diagnostic_frame,"%s%.9g",d?",":"",uv[d]);}
    fputc('\n',diagnostic_frame);
}
static uint64_t texture_clock(clockid_t clock){
    struct timespec t;if(clock_gettime(clock,&t))hle_fatal("texture profiling clock failed");
    return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;
}
static int upload(Object *o){
    if(trace_render())render_frame.uploads++;
    if(!o||o->kind!=TEXTURE){render_rejected("texture-upload","invalid texture object");return -1;}
    int created=!o->texture,dirty=0;
    for(unsigned l=0;l<o->levels;l++)dirty|=o->dirty[l]!=0;
    /* Existing clean textures need no GL work. The draw backend owns current
     * sampling parameters; resetting them here caused redundant driver calls. */
    if(!created&&!dirty)return 0;
    batch_barrier();
    d8_renderer_invalidate_bindings(&renderer); /* upload binds on the current unit */
    if(created)glGenTextures(1,&o->texture);glBindTexture(GL_TEXTURE_2D,o->texture);
    if(created){glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);d8_renderer_forget_texture(&renderer,o->texture);}
    for(unsigned l=0;l<o->levels;l++)if(o->dirty[l]){
        int timing=trace_render();uint64_t cpu_start=0,wall_start=0;
        if(timing){cpu_start=texture_clock(CLOCK_THREAD_CPUTIME_ID);wall_start=texture_clock(CLOCK_MONOTONIC);}
        uint8_t *px=rgba_pixels(o,l);
        if(timing){
            double wall_ms=(texture_clock(CLOCK_MONOTONIC)-wall_start)/1e6,cpu_ms=(texture_clock(CLOCK_THREAD_CPUTIME_ID)-cpu_start)/1e6;
            render_frame.texture_decode_cpu_ms+=cpu_ms;render_frame.texture_decode_wall_ms+=wall_ms;
            if(block_size(o->format)){render_frame.dxt_decode_cpu_ms+=cpu_ms;render_frame.dxt_decode_wall_ms+=wall_ms;render_frame.dxt_decode_levels++;}
        }
        if(!px){char reason[192];snprintf(reason,sizeof reason,"RGBA decode/allocation failed: texture=%#x format=%#x level=%u size=%ux%u",o->self,o->format,l,mipdim(o->width,l),mipdim(o->height,l));render_rejected("texture-upload",reason);return -1;}
        if(g_copy_diag){copy_decode_calls++;copy_decode_bytes+=(uint64_t)mipdim(o->width,l)*mipdim(o->height,l)*4;}
        if(timing){cpu_start=texture_clock(CLOCK_THREAD_CPUTIME_ID);wall_start=texture_clock(CLOCK_MONOTONIC);}
        glTexImage2D(GL_TEXTURE_2D,l,GL_RGBA,mipdim(o->width,l),mipdim(o->height,l),0,GL_RGBA,GL_UNSIGNED_BYTE,px);
        if(timing){render_frame.texture_upload_wall_ms+=(texture_clock(CLOCK_MONOTONIC)-wall_start)/1e6;render_frame.texture_upload_cpu_ms+=(texture_clock(CLOCK_THREAD_CPUTIME_ID)-cpu_start)/1e6;}
        free(px);
        GLenum e=glGetError();if(e){char reason[192];snprintf(reason,sizeof reason,"glTexImage2D failed: error=%#x texture=%#x format=%#x level=%u size=%ux%u",e,o->self,o->format,l,mipdim(o->width,l),mipdim(o->height,l));render_rejected("texture-upload",reason);return -1;}
        if(trace_render()){render_frame.upload_levels++;render_frame.upload_bytes+=(uint64_t)mipdim(o->width,l)*mipdim(o->height,l)*4;}
        o->dirty[l]=0;o->gpu_origin=0;o->gpu_dirty=0;
    }
    GLenum e=glGetError();if(e){char reason[96];snprintf(reason,sizeof reason,"texture bind/parameter error=%#x texture=%#x",e,o->self);render_rejected("texture-upload",reason);return -1;}
    return 0;
}
static Object *pixel_owner(Object *o,unsigned *level){if(o->kind==SURFACE&&o->parent){*level=o->lod;return object(o->parent);}*level=0;return o;}
static uint32_t new_surface(unsigned w,unsigned h,unsigned fmt,unsigned usage,int back,int depth){if(!w||!h||w>2048||h>2048)return 0;uint32_t handle=make_object(SURFACE);Object *o=object(handle);if(!o)return 0;o->width=w;o->height=h;o->format=fmt;o->usage=usage;o->levels=1;o->backbuffer=back;if(!back&&!depth&&allocate_pixels(o)){release(handle);return 0;}if(depth){uint64_t bytes=(uint64_t)w*h*(fmt==80?2:4);if(resource_bytes+bytes>TEX_BUDGET){release(handle);return 0;}o->gpu_bytes=(uint32_t)bytes;resource_bytes+=bytes;glGenRenderbuffers(1,&o->depth);glBindRenderbuffer(GL_RENDERBUFFER,o->depth);glRenderbufferStorage(GL_RENDERBUFFER,fmt==75?GL_DEPTH24_STENCIL8_OES:fmt==77?GL_DEPTH_COMPONENT24_OES:GL_DEPTH_COMPONENT16,w,h);}return handle;}
static int bind_target(uint32_t color,uint32_t depth){batch_barrier();Object *c=object(color),*d=object(depth);if(!c||c->kind!=SURFACE||d&&d->kind!=SURFACE)return -1;if(c->backbuffer){glBindFramebuffer(GL_FRAMEBUFFER,scale_fbo?scale_fbo:0);scale_bound=scale_fbo?scale_fbo:0;}else{unsigned level=0;Object *t=pixel_owner(c,&level);if(t->kind!=TEXTURE||level)return -1;if(upload(t))return -1;GLint previous_fbo;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous_fbo);if(!c->fbo)glGenFramebuffers(1,&c->fbo);glBindFramebuffer(GL_FRAMEBUFFER,c->fbo);scale_bound=(GLuint)-1;glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t->texture,level);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,d?d->depth:0);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_STENCIL_ATTACHMENT,GL_RENDERBUFFER,d&&d->format==75?d->depth:0);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous_fbo);return -1;}}
    render_target=color;depth_target=depth;return 0;}
static void descriptor(uint32_t dst,Object *o,unsigned level){uint32_t d[8]={o->format,1,o->usage,o->pool,o->bytes[level],0,mipdim(o->width,level),mipdim(o->height,level)};memcpy(hle_ptr(dst),d,sizeof d);}
static void target_written(void){Object *o=object(render_target);if(!o)return;unsigned level;Object *p=pixel_owner(o,&level);p->gpu_dirty=1;p->gpu_origin=1;}
static void pack_pixel(uint8_t *out,unsigned format,const uint8_t rgba[4]){
    uint16_t value=0;
    switch(format){
        case 20:case 21:case 22:out[0]=rgba[2];out[1]=rgba[1];out[2]=rgba[0];if(format!=20)out[3]=format==21?rgba[3]:255;break;
        case 23:value=(uint16_t)(((rgba[0]>>3)<<11)|((rgba[1]>>2)<<5)|(rgba[2]>>3));memcpy(out,&value,2);break;
        case 24:case 25:value=(uint16_t)(((rgba[0]>>3)<<10)|((rgba[1]>>3)<<5)|(rgba[2]>>3)|(format==24||rgba[3]>=128?0x8000:0));memcpy(out,&value,2);break;
        case 26:value=(uint16_t)(((rgba[3]>>4)<<12)|((rgba[0]>>4)<<8)|((rgba[1]>>4)<<4)|(rgba[2]>>4));memcpy(out,&value,2);break;
        case 28:out[0]=rgba[3];break;case 50:out[0]=rgba[0];break;case 51:out[0]=rgba[0];out[1]=rgba[3];break;
        default:break;
    }
}
/* Bring framebuffer-backed pixels into the guest's top-down system-memory
 * representation when CopyRects/LockRect explicitly requests them. */
static int sync_pixels(Object *surface){
    batch_barrier();
    unsigned level=0;Object *o=pixel_owner(surface,&level);
    if(!surface->backbuffer&&!o->gpu_dirty)return 0;
    if(!bpp(o->format)||o->format==41)return -1;
    unsigned width=mipdim(o->width,level),height=mipdim(o->height,level);
    if(!o->memory[level]){if(level||o->levels!=1||allocate_pixels(o))return -1;}
    uint8_t *rgba=malloc((size_t)width*height*4);if(!rgba)return -1;
    GLint previous;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);GLuint temp=0;
    unsigned read_width=surface->backbuffer&&scale_fbo?(unsigned)scale_w:width;
    unsigned read_height=surface->backbuffer&&scale_fbo?(unsigned)scale_h:height;
    if(surface->backbuffer)glBindFramebuffer(GL_FRAMEBUFFER,scale_fbo?scale_fbo:0);
    else{if(!o->texture){free(rgba);return -1;}glGenFramebuffers(1,&temp);glBindFramebuffer(GL_FRAMEBUFFER,temp);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,o->texture,level);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous);glDeleteFramebuffers(1,&temp);free(rgba);return -1;}}
    if(g_copy_diag){copy_sync_calls++;copy_sync_bytes+=(uint64_t)read_width*read_height*4;}
    glReadPixels(0,0,read_width,read_height,GL_RGBA,GL_UNSIGNED_BYTE,rgba);int error=glGetError()!=0;
    glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)previous);if(temp)glDeleteFramebuffers(1,&temp);
    if(!error){uint8_t *dst=hle_ptr(o->memory[level]);unsigned size=bpp(o->format);for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)pack_pixel(dst+y*o->pitch[level]+x*size,o->format,rgba+((size_t)((height-1-y)*read_height/height)*read_width+x*read_width/width)*4);o->gpu_dirty=0;o->dirty[level]=0;}
    free(rgba);return error?-1:0;
}
static void copy_rects(void){
    if(diagnostic_frame)fprintf(diagnostic_frame,"copyrects src=%#x dst=%#x count=%u rectptr=%#x pointptr=%#x\n",A32(1),A32(4),A32(3),A32(2),A32(5));
    Object *src_surface=object(A32(1)),*dst_surface=object(A32(4));
    if(!src_surface||!dst_surface||src_surface->kind!=SURFACE||dst_surface->kind!=SURFACE||dst_surface->backbuffer||sync_pixels(src_surface)||sync_pixels(dst_surface)){RET(INVALID,6);}
    unsigned sl=0,dl=0;Object *src=pixel_owner(src_surface,&sl),*dst=pixel_owner(dst_surface,&dl);
    if(src->format!=dst->format||!src->memory[sl]||!dst->memory[dl]){RET(INVALID,6);}
    unsigned count=A32(3)?A32(3):1,size=bpp(src->format),bs=block_size(src->format);
    const int32_t *rect=A32(2)?APTR(2):NULL,*point=A32(5)?APTR(5):NULL;
    for(unsigned i=0;i<count;i++){
        int32_t left=rect?rect[4*i]:0,top=rect?rect[4*i+1]:0,right=rect?rect[4*i+2]:(int32_t)src_surface->width,bottom=rect?rect[4*i+3]:(int32_t)src_surface->height;
        int32_t x=point?point[2*i]:0,y=point?point[2*i+1]:0,w=right-left,h=bottom-top;
        if(left<0||top<0||w<0||h<0||x<0||y<0||right>(int32_t)src_surface->width||bottom>(int32_t)src_surface->height||x+w>(int32_t)dst_surface->width||y+h>(int32_t)dst_surface->height){RET(INVALID,6);}
        unsigned row_bytes=(unsigned)w*size,rows=(unsigned)h,sx=(unsigned)left*size,sy=(unsigned)top,dx=(unsigned)x*size,dy=(unsigned)y;
        if(bs){if((left|top|x|y)&3){RET(INVALID,6);}sx=(unsigned)left/4*bs;sy=(unsigned)top/4;dx=(unsigned)x/4*bs;dy=(unsigned)y/4;row_bytes=((unsigned)w+3)/4*bs;rows=((unsigned)h+3)/4;}
        if(g_copy_diag){copy_blit_calls++;copy_blit_bytes+=(uint64_t)row_bytes*rows;}
        for(unsigned row=0;row<rows;row++)memmove((uint8_t*)hle_ptr(dst->memory[dl])+(dy+row)*dst->pitch[dl]+dx,(uint8_t*)hle_ptr(src->memory[sl])+(sy+row)*src->pitch[sl]+sx,row_bytes);
        dst->dirty[dl]=1;dst->gpu_dirty=0;
    }
    RET(OK,6);
}
static void caps(void *out){uint32_t c[53]={0};c[0]=1;c[5]=1;c[7]=0x00098fc0;c[8]=0x70;c[9]=0x008001c0;c[10]=255;c[11]=c[12]=0x7ff;c[13]=255;c[14]=0x84208;c[15]=0x4545;c[16]=0x03000300|0x00030000;c[19]=7;c[21]=0x1f;c[22]=c[23]=2048;c[25]=8192;c[26]=2048;c[27]=1;c[28]=0x501502f9;c[29]=c[30]=0xbf800000;c[31]=c[32]=0x3f800000;c[34]=255;c[35]=8;c[36]=0x0080ffff;c[37]=c[38]=2;c[39]=0x7;c[40]=8;c[42]=1;c[44]=0x3f800000;c[45]=100000;c[46]=0x00ffffff;c[47]=1;c[48]=256;memcpy(out,c,sizeof c);}

/* Root IDirect3D8. */
static void create8(void){init_tables();if(!root_obj)root_obj=make_object(ROOT);else addref(root_obj);fprintf(stderr,"[d3d8] Direct3DCreate8 SDK=%u -> %#x\n",A32(0),root_obj);RET(root_obj,1);}
static void adapter_count(void){RET(1,1);}
static void adapter_identifier(void){if(A32(1)||!A32(3)){RET(INVALID,4);}uint8_t *p=APTR(3);memset(p,0,1068);snprintf((char*)p,512,"SDL2/GLES D3D8 static-port driver");snprintf((char*)p+512,512,"Native OpenGL ES fixed-function adapter");uint32_t vendor=0x13b5;memcpy(p+1032,&vendor,4);RET(OK,4);}
static void mode_count(void){RET(A32(1)?0:1,2);}
static void mode(void *out){uint32_t m[4]={640,480,60,22};memcpy(out,m,16);}
static void enum_modes(void){if(A32(1)||A32(2)){RET(INVALID,4);}mode(APTR(3));RET(OK,4);}
static void adapter_mode(void){if(A32(1)){RET(INVALID,3);}mode(APTR(2));RET(OK,3);}
static void check_type(void){RET(!A32(1)&&(A32(2)==1||A32(2)==2)&&supported_format(A32(3))&&bpp(A32(4))?OK:NOTAVAILABLE,6);}
static void check_format(void){uint32_t f=A32(6),type=A32(5);int good=!A32(1)&&supported_format(f)&&type!=4&&type!=5;if((A32(4)&1)&&(!bpp(f)||type!=1&&type!=3))good=0;RET(good?OK:NOTAVAILABLE,7);}
static void check_msaa(void){RET(!A32(1)&&A32(5)==0?OK:NOTAVAILABLE,6);}
static void check_depth(void){RET(depth_format(A32(5))?OK:NOTAVAILABLE,6);}
static void root_caps(void){if(A32(1)){RET(INVALID,4);}caps(APTR(3));RET(OK,4);}
static void monitor(void){RET(A32(1)?0:1,2);}
static int open_device(uint32_t params){if(!params)return -1;memcpy(presentation,hle_ptr(params),sizeof presentation);unsigned w=presentation[0]?presentation[0]:640,h=presentation[1]?presentation[1]:480;if(w>2048||h>2048)return -1;presentation[0]=w;presentation[1]=h;presentation[4]=0;
    if(SDL_InitSubSystem(SDL_INIT_VIDEO|SDL_INIT_EVENTS))return -1;SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE,8);
    window=SDL_CreateWindow("The Thing",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,w,h,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|(setting("THING_WINDOWED")?0:SDL_WINDOW_FULLSCREEN_DESKTOP));if(!window)return -1;context=SDL_GL_CreateContext(window);if(!context)return -1;/* Presentation interval: the guest asks for IMMEDIATE with 0x80000000;
 * otherwise SDL defaults to vsync (1). DS_SWAP_INTERVAL overrides it: 0 is
 * immediate (no refresh wait, tearing possible), 1 vsync. */
    /* Frame cap selection: DS_FPS_LIMIT=30/60/0(uncap). 60 means vsync; 30 uses
     * immediate present plus an explicit 33.33 ms pacer (half of a 60 Hz
     * refresh); uncapped means immediate present with no pacer. DS_SWAP_INTERVAL
     * still overrides the swap interval directly for A/B. */
    {const char *lim=setting("DS_FPS_LIMIT");int limit=0;if(lim){if(!strcmp(lim,"uncap")||!strcmp(lim,"0"))limit=0;else limit=atoi(lim);}
     if(limit==30)frame_limit_ms=1000.0/30.0;else if(limit==60)frame_limit_ms=0;else frame_limit_ms=0;
     int interval=presentation[12]==0x80000000u?0:1;
     if(lim){interval=limit==60?1:0;}
     const char *si=setting("DS_SWAP_INTERVAL");if(si)interval=atoi(si);
     SDL_GL_SetSwapInterval(interval);
     fprintf(stderr,"[d3d8] present: swap_interval=%d frame_limit=%.2f ms (DS_FPS_LIMIT=%s)\n",interval,frame_limit_ms,lim?lim:"guest");}SDL_ShowCursor(SDL_DISABLE);
    fprintf(stderr,"[d3d8] CreateDevice %ux%u GL=%s renderer=%s\n",w,h,glGetString(GL_VERSION),glGetString(GL_RENDERER));if(d8_renderer_init(&renderer))return -1;if(create_scale_target(w,h))return -1;d8_state_defaults(&state,w,h);ds_d8_lights_init(&light_bank);pack_lights();hle_set_screen_size(w,h);hle_set_pump_hook(pump);
    render_target=backbuffer_obj=new_surface(w,h,presentation[2]?presentation[2]:22,1,1,0);addref(render_target);depth_target=presentation[8]?new_surface(w,h,presentation[9],2,0,1):0;memcpy(hle_ptr(params),presentation,sizeof presentation);return 0;
}
static void create_device(void){out_handle(6,0);if(A32(1)||device_obj){RET(NOTAVAILABLE,7);}if(open_device(A32(5))){fprintf(stderr,"[d3d8] context creation failed: %s\n",SDL_GetError());RET(NOTAVAILABLE,7);}device_obj=make_object(DEVICE);if(object(render_target))object(render_target)->device=device_obj;if(object(depth_target))object(depth_target)->device=device_obj;out_handle(6,device_obj);RET(OK,7);}

/* Device and resource methods. */
static void coop(void){RET(OK,1);}
static void available(void){RET(resource_bytes<TEX_BUDGET?TEX_BUDGET-(uint32_t)resource_bytes:0,1);}
static void discard(void){RET(OK,2);}
static void get_root(void){out_handle(1,root_obj);addref(root_obj);RET(OK,2);}
static void dev_caps(void){caps(APTR(1));RET(OK,2);}
static void dev_mode(void){mode(APTR(1));RET(OK,2);}
static void creation(void){uint32_t p[4]={0,1,presentation[6],0x20};memcpy(APTR(1),p,16);RET(OK,2);}
static void cursor_position(void){hle_input_cursor((int)A32(1),(int)A32(2));RETV(4);}
static void cursor_show(void){int old=SDL_ShowCursor(A32(1)?SDL_ENABLE:SDL_DISABLE);RET(old==SDL_ENABLE,2);}
static void reset(void){batch_barrier();if(!A32(1)){RET(INVALID,2);}uint32_t *p=APTR(1);if((p[0]&&p[0]!=presentation[0])||(p[1]&&p[1]!=presentation[1])){fprintf(stderr,"[d3d8] Reset resolution change unsupported\n");RET(NOTAVAILABLE,2);}memcpy(presentation,p,sizeof presentation);SDL_GL_SetSwapInterval(presentation[12]==0x80000000u?0:1);RET(OK,2);}
static void present(void){batch_barrier();if(!window){RET(INVALID,5);}if(scale_fbo){if(ds_rt_active&&ds_rt_active()&&ds_rt_call)ds_rt_call(scale_blit_job,NULL);else scale_blit_now();}ds_input_pump();ds_fps_overlay_draw(window);capture_frame(frames+1);d8_renderer_invalidate_raster(&renderer);d8_renderer_invalidate_attributes(&renderer);int tracing=trace_render();uint64_t swap_begin=tracing?SDL_GetPerformanceCounter():0;if(diagnostic_finish)glFinish();SDL_GL_SwapWindow(window);d8_renderer_begin_frame(&renderer);diagnostic_poll();ds_fps_overlay_presented();frames++;if(draw_batch&&frames%600==0)fprintf(stderr,"[draw-batch] cumulative_inputs=%llu emits=%llu saved=%llu\n",(unsigned long long)batch_inputs,(unsigned long long)batch_emits,(unsigned long long)(batch_inputs-batch_emits));trace_present(tracing?(SDL_GetPerformanceCounter()-swap_begin)*1000.0/SDL_GetPerformanceFrequency():0);profile_frame();if(ds_gl_cost_present)ds_gl_cost_present();if(gl_cost_requested){if(ds_gl_cost_begin)ds_gl_cost_begin(gl_cost_requested);gl_cost_requested=0;}if(frames==1){const char *rt=getenv("DS_RENDER_THREAD");int active=ds_rt_active?ds_rt_active():0;
        fprintf(stderr,"[d3d8] render mode: requested=%s active=%s\n",rt?rt:"(unset)",active?"THREADED":"SINGLE");}
    if(frames==1||frames%600==0)fprintf(stderr,"[d3d8] Present frame=%u guest=%#x\n",frames,g_cur_func);if(frames%600==0)d8_renderer_perf_dump(&renderer);
    /* Frame pacer for the 30 FPS lock (half-refresh). Only waits when the game
     * is faster than the target; it never raises a slower scene's frame rate. */
    if(frame_limit_ms>0){uint64_t freq=SDL_GetPerformanceFrequency(),now=SDL_GetPerformanceCounter();
        double target=(double)frame_limit_ms/1000.0;
        uint64_t target_ticks=(uint64_t)(target*freq);
        if(last_present_tick&&now-last_present_tick<target_ticks){
            uint64_t left=target_ticks-(now-last_present_tick);
            double left_ms=left*1000.0/freq;
            if(left_ms>1.5)SDL_Delay((Uint32)(left_ms-1.0));
            while(SDL_GetPerformanceCounter()-last_present_tick<target_ticks)SDL_Delay(0);
            now=SDL_GetPerformanceCounter();
        }
        last_present_tick=now;}
    if(scale_fbo){scale_bound=(GLuint)-1;bind_game_target();d8_renderer_invalidate_raster(&renderer);}
    if(thing_cpu_profiler_frame)thing_cpu_profiler_frame(frames+1);
    RET(OK,5);}
static void backbuffer(void){if(A32(1)){RET(INVALID,4);}out_handle(3,backbuffer_obj);addref(backbuffer_obj);RET(OK,4);}
static void raster_status(void){uint32_t out[2]={0,0};memcpy(APTR(1),out,8);RET(OK,2);}
static uint16_t gamma_ramp[3][256];
static void set_gamma(void){if(!A32(2)){RETV(3);}memcpy(gamma_ramp,APTR(2),sizeof gamma_ramp);if(window)SDL_SetWindowGammaRamp(window,gamma_ramp[0],gamma_ramp[1],gamma_ramp[2]);RETV(3);}
static void get_gamma(void){for(int c=0;c<3;c++)for(int i=0;i<256;i++)gamma_ramp[c][i]=(uint16_t)(i*257);memcpy(APTR(1),gamma_ramp,sizeof gamma_ramp);RETV(2);}
static void create_texture(void){
    out_handle(7,0);unsigned w=A32(1),h=A32(2),levels=A32(3),fmt=A32(5);
    if(!w||!h||w>2048||h>2048){texture_allocation_failure("invalid-size",w,h,fmt,levels,A32(4),A32(6),0,0);RET(NOTAVAILABLE,8);}
    if(!bpp(fmt)&&!block_size(fmt)){texture_allocation_failure("unsupported-format",w,h,fmt,levels,A32(4),A32(6),0,0);RET(NOTAVAILABLE,8);}
    if(!levels){levels=1;for(unsigned v=w>h?w:h;v>1;v>>=1)levels++;}
    if(levels>MAX_LEVELS){texture_allocation_failure("invalid-level-count",w,h,fmt,levels,A32(4),A32(6),0,0);RET(INVALID,8);}
    uint32_t handle=make_object(TEXTURE);Object *o=object(handle);
    if(!o){texture_allocation_failure(!object_slots.free_head&&object_slots.next>=DS_D3D8_SLOT_CAPACITY?"object-table":"object-guest-heap",w,h,fmt,levels,A32(4),A32(6),0,12);RET(OUTOFMEMORY,8);}
    o->width=w;o->height=h;o->levels=levels;o->format=fmt;o->usage=A32(4);o->pool=A32(6);
    if(allocate_pixels(o)){release(handle);RET(OUTOFMEMORY,8);}
    out_handle(7,handle);RET(OK,8);
}
static void buffer_allocation_failure(int index,unsigned bytes,const char *reason){
    unsigned count[7]={0};uint64_t cpu[7]={0},gpu[7]={0};
    for(unsigned i=1;i<MAX_OBJECTS;i++){Object *o=&objects[i];if(!o->self||o->kind>6)continue;count[o->kind]++;gpu[o->kind]+=o->gpu_bytes;if(o->owns_memory)for(unsigned l=0;l<o->levels;l++)cpu[o->kind]+=o->bytes[l];}
    fprintf(stderr,"[d3d8-allocation] %s failed reason=%s bytes=%u usage=%#x fvf_or_format=%#x pool=%u tracked=%llu budget=%u guest=%#x\n",index?"IB":"VB",reason,bytes,A32(2),A32(3),A32(4),(unsigned long long)resource_bytes,TEX_BUDGET,g_cur_func);
    for(unsigned k=1;k<7;k++)fprintf(stderr,"[d3d8-allocation] kind=%u objects=%u cpu_bytes=%llu gpu_bytes=%llu\n",k,count[k],(unsigned long long)cpu[k],(unsigned long long)gpu[k]);
}
static void create_buffer(int index){
    unsigned bytes=A32(1);out_handle(5,0);
    if(!bytes||bytes>64u<<20){buffer_allocation_failure(index,bytes,"invalid-size");RET(INVALID,6);}
    if(resource_bytes+bytes>TEX_BUDGET){buffer_allocation_failure(index,bytes,"resource-budget");RET(OUTOFMEMORY,6);}
    uint32_t handle=make_object(index?IBUFFER:VBUFFER);Object *o=object(handle);
    if(!o){buffer_allocation_failure(index,bytes,"object-table");RET(OUTOFMEMORY,6);}
    o->levels=1;o->length=bytes;o->bytes[0]=bytes;o->usage=A32(2);o->fvf=index?0:A32(3);o->format=index?A32(3):100;o->pool=A32(4);o->memory[0]=hle_va(hle_alloc(bytes));
    if(!o->memory[0]){buffer_allocation_failure(index,bytes,"guest-heap");release(handle);RET(OUTOFMEMORY,6);}
    o->owns_memory=1;o->buffer_dirty_end=bytes;resource_bytes+=bytes;out_handle(5,handle);RET(OK,6);
}
static void create_vb(void){create_buffer(0);}static void create_ib(void){create_buffer(1);}
static void create_rt(void){out_handle(6,0);if(A32(4)||!bpp(A32(3))||!A32(1)||!A32(2)||A32(1)>2048||A32(2)>2048){RET(NOTAVAILABLE,7);}uint32_t t=make_object(TEXTURE);Object *o=object(t);if(!o){RET(OUTOFMEMORY,7);}o->width=A32(1);o->height=A32(2);o->format=A32(3);o->levels=1;o->usage=1;if(allocate_pixels(o)){release(t);RET(OUTOFMEMORY,7);}uint32_t surf=make_object(SURFACE);Object *s=object(surf);s->width=o->width;s->height=o->height;s->format=o->format;s->parent=t;s->levels=1;s->usage=1;out_handle(6,surf);RET(OK,7);}
static void create_depth(void){out_handle(5,0);if(A32(4)||!depth_format(A32(3))){RET(NOTAVAILABLE,6);}uint32_t o=new_surface(A32(1),A32(2),A32(3),2,0,1);out_handle(5,o);RET(o?OK:OUTOFMEMORY,6);}
static void create_image(void){out_handle(4,0);if(!bpp(A32(3))){RET(NOTAVAILABLE,5);}uint32_t o=new_surface(A32(1),A32(2),A32(3),0,0,0);out_handle(4,o);RET(o?OK:OUTOFMEMORY,5);}
static void update_texture(void){Object *a=object(A32(1)),*b=object(A32(2));if(!a||!b||a->kind!=TEXTURE||b->kind!=TEXTURE||a->format!=b->format||a->width!=b->width||a->height!=b->height){RET(INVALID,3);}unsigned n=a->levels<b->levels?a->levels:b->levels;for(unsigned l=0;l<n;l++){memcpy(hle_ptr(b->memory[l]),hle_ptr(a->memory[l]),b->bytes[l]);b->dirty[l]=1;}RET(OK,3);}
static void set_target(void){if(diagnostic_frame)fprintf(diagnostic_frame,"target color=%#x depth=%#x\n",A32(1),A32(2));uint32_t color=A32(1)?A32(1):render_target,depth=A32(2),old_color=render_target,old_depth=depth_target;if(bind_target(color,depth)){RET(INVALID,3);}if(A32(1)){Object *target=object(color);uint32_t viewport[6]={0,0,target->width,target->height,0,0x3f800000u};memcpy(state.viewport,viewport,sizeof viewport);record_state(state.viewport,sizeof viewport);}addref(color);addref(depth);release(old_color);release(old_depth);RET(OK,3);}
static void get_target(void){out_handle(1,render_target);addref(render_target);RET(OK,2);}
static void get_depth(void){out_handle(1,depth_target);addref(depth_target);RET(OK,2);}
static void begin_scene(void){pump();RET(OK,1);}static void end_scene(void){batch_barrier();RET(OK,1);}
static void clear(void){batch_barrier();if(scale_fbo)bind_game_target();if(diagnostic_frame)fprintf(diagnostic_frame,"clear flags=%#x color=%#x depth=%#x stencil=%u rects=%u rt=%#x\n",A32(3),A32(4),A32(5),A32(6),A32(1),render_target);if(trace_render())render_frame.clears++;unsigned flags=A32(3);uint32_t c=A32(4);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glStencilMask(UINT32_MAX);d8_renderer_invalidate_raster(&renderer);glClearColor(((c>>16)&255)/255.f,((c>>8)&255)/255.f,(c&255)/255.f,(c>>24)/255.f);glClearDepthf(bits_float(A32(5)));glClearStencil(A32(6));GLbitfield mask=(flags&1?GL_COLOR_BUFFER_BIT:0)|(flags&2?GL_DEPTH_BUFFER_BIT:0)|(flags&4?GL_STENCIL_BUFFER_BIT:0);unsigned n=A32(1);if(n&&A32(2)){const int32_t *rect=APTR(2);Object *target=object(render_target);glEnable(GL_SCISSOR_TEST);for(unsigned i=0;i<n;i++){if(renderer.output_scale==1.0f)glScissor(rect[i*4],(GLint)target->height-rect[i*4+3],rect[i*4+2]-rect[i*4],rect[i*4+3]-rect[i*4+1]);
                else{float sc=renderer.output_scale;glScissor((GLint)(rect[i*4]*sc+0.5f),(GLint)(((GLint)target->height-rect[i*4+3])*sc+0.5f),(GLint)((rect[i*4+2]-rect[i*4])*sc+0.5f),(GLint)((rect[i*4+3]-rect[i*4+1])*sc+0.5f));}glClear(mask);}glDisable(GL_SCISSOR_TEST);}else glClear(mask);if(flags&1)target_written();RET(glGetError()?INVALID:OK,7);}
static void set_transform(void){unsigned t=A32(1);if(t>=512||!A32(2)){RET(INVALID,3);}memcpy(state.transform[t],APTR(2),64);record_state(state.transform[t],64);RET(OK,3);}
static void get_transform(void){unsigned t=A32(1);if(t>=512||!A32(2)){RET(INVALID,3);}memcpy(APTR(2),state.transform[t],64);RET(OK,3);}
static void multiply_transform(void){unsigned t=A32(1);if(t>=512||!A32(2)){RET(INVALID,3);}d8_matrix_multiply(state.transform[t],state.transform[t],APTR(2));record_state(state.transform[t],64);RET(OK,3);}
static void set_viewport(void){if(!A32(1)){RET(INVALID,2);}memcpy(state.viewport,APTR(1),24);record_state(state.viewport,24);RET(OK,2);}
static void get_viewport(void){memcpy(APTR(1),state.viewport,24);RET(OK,2);}
static void set_material(void){memcpy(state.material,APTR(1),68);record_state(state.material,68);RET(OK,2);}static void get_material(void){memcpy(APTR(1),state.material,68);RET(OK,2);}
static void set_light(void){uint32_t id=A32(1);if(!A32(2)||ds_d8_lights_set(&light_bank,id,APTR(2))){RET(INVALID,3);}if(record_start)record_light_mask.definition[id]=1;
    /* Definitions do not change the enabled set or its ascending slot order.
     * Update only the affected active slot; disabled definitions need no
     * renderer work. Full packing remains necessary when the set changes. */
    if(light_bank.enabled[id])for(unsigned i=0;i<DS_D8_ACTIVE_LIGHTS;i++)if(active_light_ids[i]==id){memcpy(state.light[i],light_bank.light[id],sizeof state.light[i]);break;}
    RET(OK,3);}
static void get_light(void){if(!A32(2)||ds_d8_lights_get(&light_bank,A32(1),APTR(2))){RET(INVALID,3);}RET(OK,3);}
static void light_enable(void){uint32_t id=A32(1);int was_defined=id<DS_D8_LOGICAL_LIGHTS&&light_bank.defined[id],was_enabled=id<DS_D8_LOGICAL_LIGHTS&&light_bank.enabled[id];if(ds_d8_lights_enable(&light_bank,id,A32(2)!=0)){RET(INVALID,3);}if(record_start){record_light_mask.enable[id]=1;if(!was_defined)record_light_mask.definition[id]=1;}if(was_enabled!=light_bank.enabled[id])pack_lights();RET(OK,3);}
static void light_enabled(void){uint32_t enabled;if(!A32(2)){RET(INVALID,3);}if(ds_d8_lights_get_enable(&light_bank,A32(1),&enabled)){RET(INVALID,3);}out_handle(2,enabled);RET(OK,3);}
static void set_rs(void){if(A32(1)>=256){RET(INVALID,3);}state.rs[A32(1)]=A32(2);record_state(&state.rs[A32(1)],4);RET(OK,3);}static void get_rs(void){if(A32(1)>=256){RET(INVALID,3);}out_handle(2,state.rs[A32(1)]);RET(OK,3);}
static void block_references(StateBlock *b,int retain){
    for(int i=0;i<8;i++)if(b->bindings[i]){if(retain)addref(b->textures[i]);else release(b->textures[i]);}
    if(b->bindings[8]){if(retain)addref(b->stream);else release(b->stream);}
    if(b->bindings[9]){if(retain)addref(b->index);else release(b->index);}
}
static void block_capture(StateBlock *b){
    b->lights=light_bank;
    memcpy(&b->value,&state,sizeof state);memcpy(b->textures,bound_texture,sizeof bound_texture);
    b->stream=stream_obj;b->stride=stream_stride;b->index=index_obj;b->base=base_vertex;block_references(b,1);
}
static int block_apply(const StateBlock *b){
    if(ds_d8_lights_apply_masked(&light_bank,&b->lights,&b->light_mask))return -1;
    for(size_t i=0;i<sizeof state;i++)if(b->mask[i])((uint8_t*)&state)[i]=((const uint8_t*)&b->value)[i];
    for(int i=0;i<8;i++)if(b->bindings[i]){uint32_t old=bound_texture[i];bound_texture[i]=b->textures[i];addref(bound_texture[i]);release(old);}
    if(b->bindings[8]){uint32_t old=stream_obj;stream_obj=b->stream;stream_stride=b->stride;addref(stream_obj);release(old);}
    if(b->bindings[9]){uint32_t old=index_obj;index_obj=b->index;base_vertex=b->base;addref(index_obj);release(old);}
    pack_lights();return 0;
}
static uint32_t new_block(int all){
    for(unsigned i=1;i<128;i++)if(!blocks[i]){
        StateBlock *b=calloc(1,sizeof(*b));if(!b)return 0;
        if(all){ds_d8_light_mask_all(&b->light_mask);memset(b->mask,1,sizeof b->mask);memset(b->bindings,1,sizeof b->bindings);}
        else{b->light_mask=record_light_mask;memcpy(b->mask,record_mask,sizeof b->mask);memcpy(b->bindings,record_bindings,sizeof b->bindings);}
        block_capture(b);blocks[i]=b;return i;
    }
    return 0;
}
static void begin_block(void){
    if(record_start){RET(INVALID,1);}
    record_start=calloc(1,sizeof(*record_start));if(!record_start){RET(OUTOFMEMORY,1);}
    ds_d8_light_mask_all(&record_start->light_mask);
    memset(record_start->mask,1,sizeof record_start->mask);memset(record_start->bindings,1,sizeof record_start->bindings);block_capture(record_start);
    ds_d8_light_mask_clear(&record_light_mask);
    memset(record_mask,0,sizeof record_mask);memset(record_bindings,0,sizeof record_bindings);RET(OK,1);
}
static void end_block(void){
    if(!record_start){RET(INVALID,2);}
    uint32_t b=new_block(0);if(block_apply(record_start))hle_fatal("invalid BeginStateBlock light restore");block_references(record_start,0);free(record_start);record_start=NULL;
    out_handle(1,b);RET(b?OK:OUTOFMEMORY,2);
}
static void apply_block(void){unsigned b=A32(1);if(!b||b>=128||!blocks[b]){RET(INVALID,2);}RET(block_apply(blocks[b])?INVALID:OK,2);}
static void capture_block(void){unsigned b=A32(1);if(!b||b>=128||!blocks[b]){RET(INVALID,2);}block_references(blocks[b],0);block_capture(blocks[b]);RET(OK,2);}
static void delete_block(void){unsigned b=A32(1);if(!b||b>=128||!blocks[b]){RET(INVALID,2);}block_references(blocks[b],0);free(blocks[b]);blocks[b]=NULL;RET(OK,2);}
static void create_block(void){if(A32(1)!=1){out_handle(2,0);RET(NOTAVAILABLE,3);}uint32_t b=new_block(1);out_handle(2,b);RET(b?OK:OUTOFMEMORY,3);}
static void set_clip_status(void){memcpy(clip_status,APTR(1),8);RET(OK,2);}static void get_clip_status(void){memcpy(APTR(1),clip_status,8);RET(OK,2);}
static void set_texture(void){unsigned stage=A32(1);Object *o=object(A32(2));if(stage>=8||A32(2)&&(!o||o->kind!=TEXTURE)){RET(INVALID,3);}if(stage>=2&&A32(2)){RET(NOTAVAILABLE,3);}uint32_t old=bound_texture[stage];bound_texture[stage]=A32(2);record_binding(stage);addref(bound_texture[stage]);release(old);RET(OK,3);}
static void get_texture(void){unsigned t=A32(1);if(t>=8){RET(INVALID,3);}out_handle(2,bound_texture[t]);addref(bound_texture[t]);RET(OK,3);}
static void set_tss(void){unsigned t=A32(1),s=A32(2);if(t>=8||s>=32){RET(INVALID,4);}state.tss[t][s]=A32(3);record_state(&state.tss[t][s],4);RET(OK,4);}static void get_tss(void){unsigned t=A32(1),s=A32(2);if(t>=8||s>=32){RET(INVALID,4);}out_handle(3,state.tss[t][s]);RET(OK,4);}
static void validate(void){for(int i=0;i<8&&state.tss[i][1]!=1&&!((state.tss[i][2]&15)==2&&!bound_texture[i]);i++)if(i>=2){RET(NOTAVAILABLE,2);}out_handle(1,1);RET(OK,2);}
static void set_palette_entries(void){if(A32(1)>=16){RET(INVALID,3);}memcpy(palettes[A32(1)],APTR(2),1024);for(int i=1;i<MAX_OBJECTS;i++)if(objects[i].self&&objects[i].format==41)for(unsigned l=0;l<objects[i].levels;l++)objects[i].dirty[l]=1;RET(OK,3);}
static void get_palette_entries(void){if(A32(1)>=16){RET(INVALID,3);}memcpy(APTR(2),palettes[A32(1)],1024);RET(OK,3);}
static void set_palette(void){if(A32(1)>=16){RET(INVALID,2);}current_palette=A32(1);RET(OK,2);}static void get_palette(void){out_handle(1,current_palette);RET(OK,2);}
extern void d8_renderer_bind_upload_buffer(D8Renderer *,int,GLuint);
static int upload_buffer(Object *o,int index){
    GLenum target=index?GL_ELEMENT_ARRAY_BUFFER:GL_ARRAY_BUFFER;
    /* Clean resources do not mutate GL state. Preserve the draw binding cache. */
    if(o->buffer&&o->buffer_dirty_end<=o->buffer_dirty_start)return 0;
    if(!o->buffer){
        if(resource_bytes+o->length>TEX_BUDGET)return 1; /* Keep optional staging path available. */
        GLuint buffer=0;glGenBuffers(1,&buffer);d8_renderer_bind_upload_buffer(&renderer,index,buffer);glBufferData(target,o->length,hle_ptr(o->memory[0]),GL_DYNAMIC_DRAW);
        if(glGetError()){if(buffer){d8_renderer_invalidate_bindings(&renderer);d8_renderer_invalidate_attributes(&renderer);glDeleteBuffers(1,&buffer);}return draw_failed("native buffer allocation/upload failed");}
        o->buffer=buffer;o->gpu_bytes=o->length;resource_bytes+=o->gpu_bytes;
        d8_renderer_note_buffer_upload(&renderer,index,o->length);
    }else if(o->buffer_dirty_end>o->buffer_dirty_start){
        unsigned off=o->buffer_dirty_start,bytes=o->buffer_dirty_end-off;
        d8_renderer_bind_upload_buffer(&renderer,index,o->buffer);glBufferSubData(target,off,bytes,(const uint8_t*)hle_ptr(o->memory[0])+off);
        if(glGetError())return draw_failed("native buffer update failed");
        d8_renderer_note_buffer_upload(&renderer,index,bytes);
    }
    o->buffer_dirty_start=o->buffer_dirty_end=0;return 0;
}
/* Draw origin telemetry: UP memory has no reusable D3D buffer identity.
 * Only enabled by the existing performance probe; no per-draw logging. */
static uint64_t perf_up_draws,perf_buffer_draws,perf_native_vb,perf_native_ib;
static int perf_origin_enabled=-1;
static int draw(unsigned primitive,unsigned count,const void *vertices,unsigned stride,unsigned vcount,const void *indices,unsigned isize,unsigned base,Object *vb,size_t voff,Object *ib,size_t ioff){
    if(scale_fbo)bind_game_target();
    int tracing=trace_render();uint64_t begin=tracing?SDL_GetPerformanceCounter():0;
    GLuint textures[2]={0};
    for(int i=0;i<2;i++){state.texture_flipy[i]=0;if(state.tss[i][1]==1||((state.tss[i][2]&15)==2&&!bound_texture[i]))break;Object *o=object(bound_texture[i]);if(o){if(upload(o))return draw_failed("texture upload failed; see upload diagnostics");textures[i]=o->texture;state.texture_flipy[i]=(uint32_t)o->gpu_origin;}}
    Object *target=object(render_target);if(!target)return draw_failed("render target object is missing");
    diagnostic_draw(primitive,count);
    if(diagnostic_frame)fprintf(diagnostic_frame,"geometry vb=%#x ib=%#x voff=%zu ioff=%zu stride=%u vertices=%u base=%u index_size=%u\n",vb?vb->self:0,ib?ib->self:0,voff,ioff,stride,vcount,base,isize);
    if(diagnostic_frame)for(int t=0;t<2;t++){Object *o=object(bound_texture[t]);if(o){fprintf(diagnostic_frame,"texture stage=%d handle=%#x size=%ux%u format=%u levels=%u usage=%u gl=%u origin=%d dirty=%d\n",t,o->self,o->width,o->height,o->format,o->levels,o->usage,o->texture,o->gpu_origin,o->gpu_dirty);diagnostic_texture(o);}}
    if(diagnostic_frame){
        unsigned n=d8_primitive_vertices(primitive,count);if(n>4)n=4;
        for(unsigned j=0;j<n;j++){uint32_t ix=j;if(indices&&isize==2){uint16_t v;memcpy(&v,(const char*)indices+j*2,2);ix=v;}else if(indices&&isize==4)memcpy(&ix,(const char*)indices+j*4,4);if(ix>=vcount||ix>UINT32_MAX-base)continue;
            fprintf(diagnostic_frame,"vertex%u index=%u words",j,ix);const unsigned char *v=(const unsigned char*)vertices+(size_t)(ix+base)*stride;unsigned bytes=d8_fvf_stride(state.fvf);if(bytes>stride)bytes=stride;for(unsigned o=0;o+4<=bytes;o+=4){uint32_t word;memcpy(&word,v+o,4);fprintf(diagnostic_frame," %08x",word);}fputc('\n',diagnostic_frame);
            diagnostic_vertex(v,bytes,ix);
        }
    }
    D8NativeBuffers native={0};/* Backbuffer draws (scaled FBO included) have no guest texture attached, so untextured draws may keep cached texture units instead of rebinding unit 0 every draw. */native.default_framebuffer=target->backbuffer!=0;
    if(cache_buffers()&&renderer.gpu_program&&!diagnostic_cpu){
        if(vb&&!vb->buffer_locked){int rc=upload_buffer(vb,0);if(rc<0)return -1;if(!rc){native.vertex=vb->buffer;native.vertex_offset=voff;native.vertex_bytes=vb->length;}}
        if(ib&&!ib->buffer_locked){int rc=upload_buffer(ib,1);if(rc<0)return -1;if(!rc){native.index=ib->buffer;native.index_offset=ioff;native.index_bytes=ib->length;}}
    }
    if(perf_origin_enabled<0){const char *v=setting("DS_PERF_PROBE");perf_origin_enabled=v&&!strcmp(v,"1");}
    if(perf_origin_enabled){
        if(vb)perf_buffer_draws++;else perf_up_draws++;
        perf_native_vb+=native.vertex!=0;perf_native_ib+=native.index!=0;
        if((perf_up_draws+perf_buffer_draws)%100000==0)
            fprintf(stderr,"[draw-origin] cumulative_up=%llu buffer=%llu native_vb=%llu native_ib=%llu cache_enabled=%d\n",
                (unsigned long long)perf_up_draws,(unsigned long long)perf_buffer_draws,
                (unsigned long long)perf_native_vb,(unsigned long long)perf_native_ib,cache_buffers());
    }
    if(diagnostic_cpu||diagnostic_frame)batch_barrier();
    GLuint saved_gpu=renderer.gpu_program;if(diagnostic_cpu)renderer.gpu_program=0;
    /* Default FBO depth/stencil physically exist even when the guest has
     * detached its logical depth surface. Preserve guest-visible RS values. */
    uint32_t saved_depth=state.rs[7],saved_stencil=state.rs[52];Object *depth=object(depth_target);
    if(!depth)state.rs[7]=0;
    if(!depth||depth->format!=75)state.rs[52]=0;
    int rc=1;
    if(batch_requested()&&!cache_buffers()&&!diagnostic_cpu&&!diagnostic_frame&&renderer.gpu_path_enabled&&target->backbuffer&&vb&&ib&&indices&&voff<=vb->length&&ioff<=ib->length){
        if(!draw_batch){draw_batch=thing_batch_create(1024*1024,65536,batch_emit,NULL);if(draw_batch)fprintf(stderr,"[draw-batch] experimental SINGLE/cache-OFF queue enabled\n");}
        if(draw_batch){
            ThingBatchDraw d={.state=&state,.vertices=vertices,.indices=indices,.vertex_bytes=vb->length-voff,.index_bytes=ib->length-ioff,.primitive=primitive,.count=count,.stride=stride,.vcount=vcount,.base=base,.index_size=isize,.vertex_identity=vb->self,.target_identity=render_target,.target_height=target->height,.textures={textures[0],textures[1]},.default_framebuffer=native.default_framebuffer};
            rc=thing_batch_add(draw_batch,&d);if(!rc)batch_inputs++;
        }
    }
    if(rc==1){batch_barrier();rc=d8_renderer_draw_native(&renderer,&state,target->height,primitive,count,vertices,stride,vcount,indices,isize,base,textures,&native);}
    state.rs[7]=saved_depth;state.rs[52]=saved_stencil;
    if(diagnostic_cpu)renderer.gpu_program=saved_gpu;
    if(tracing)render_frame.submit_ms+=(SDL_GetPerformanceCounter()-begin)*1000.0/SDL_GetPerformanceFrequency();
    if(rc)return draw_failed(d8_renderer_error());
    if(tracing){render_frame.accepted++;render_frame.primitives+=count;render_frame.vertices+=d8_primitive_vertices(primitive,count);}
    target_written();return 0;
}
static void draw_up(void){trace_draw_start();unsigned n=d8_primitive_vertices(A32(1),A32(2));RET(draw(A32(1),A32(2),APTR(3),A32(4),n,NULL,0,0,NULL,0,NULL,0)?INVALID:OK,5);}
static void draw_indexed_up(void){trace_draw_start();unsigned size=A32(6)==101?2:A32(6)==102?4:0;if(!size){draw_failed("DrawIndexedPrimitiveUP index format is not INDEX16/INDEX32");RET(INVALID,9);}RET(draw(A32(1),A32(4),APTR(7),A32(8),A32(2)+A32(3),APTR(5),size,0,NULL,0,NULL,0)?INVALID:OK,9);}
static void draw_primitive(void){trace_draw_start();Object *o=object(stream_obj);if(!o||!stream_stride){draw_failed("DrawPrimitive stream0 buffer or stride is missing");RET(INVALID,4);}unsigned n=d8_primitive_vertices(A32(1),A32(3));if(((uint64_t)A32(2)+n)*stream_stride>o->length){draw_failed("DrawPrimitive exceeds the bound vertex buffer");RET(INVALID,4);}RET(draw(A32(1),A32(3),(uint8_t*)hle_ptr(o->memory[0])+A32(2)*stream_stride,stream_stride,n,NULL,0,0,o,(size_t)A32(2)*stream_stride,NULL,0)?INVALID:OK,4);}
static void draw_indexed(void){trace_draw_start();Object *v=object(stream_obj),*i=object(index_obj);unsigned size=i&&i->format==101?2:i&&i->format==102?4:0,n=d8_primitive_vertices(A32(1),A32(5));if(!v||!i||!size||!stream_stride){draw_failed("DrawIndexedPrimitive vertex/index binding or stride is invalid");RET(INVALID,6);}if(((uint64_t)A32(4)+n)*size>i->length||base_vertex>v->length/stream_stride){draw_failed("DrawIndexedPrimitive exceeds the bound vertex/index buffer");RET(INVALID,6);}RET(draw(A32(1),A32(5),hle_ptr(v->memory[0]),stream_stride,v->length/stream_stride-base_vertex,(uint8_t*)hle_ptr(i->memory[0])+A32(4)*size,size,base_vertex,v,0,i,(size_t)A32(4)*size)?INVALID:OK,6);}
static void set_fvf(void){if(!d8_fvf_stride(A32(1))){fprintf(stderr,"[d3d8] unsupported vertex shader/FVF %#x\n",A32(1));RET(NOTAVAILABLE,2);}state.fvf=A32(1);record_state(&state.fvf,4);RET(OK,2);}static void get_fvf(void){out_handle(1,state.fvf);RET(OK,2);}
static void create_vs(void){out_handle(3,0);RET(NOTAVAILABLE,5);}static void create_ps(void){out_handle(2,0);RET(NOTAVAILABLE,3);}static void set_ps(void){RET(A32(1)?NOTAVAILABLE:OK,2);}static void get_ps(void){out_handle(1,0);RET(OK,2);}
static void set_stream(void){Object *o=object(A32(2));if(A32(1)||A32(2)&&(!o||o->kind!=VBUFFER)){RET(INVALID,4);}uint32_t old=stream_obj;stream_obj=A32(2);stream_stride=A32(3);record_binding(8);addref(stream_obj);release(old);RET(OK,4);}static void get_stream(void){if(A32(1)){RET(INVALID,4);}out_handle(2,stream_obj);out_handle(3,stream_stride);addref(stream_obj);RET(OK,4);}
static void set_indices(void){Object *o=object(A32(1));if(A32(1)&&(!o||o->kind!=IBUFFER)){RET(INVALID,3);}uint32_t old=index_obj;index_obj=A32(1);base_vertex=A32(2);record_binding(9);addref(index_obj);release(old);RET(OK,3);}static void get_indices(void){out_handle(1,index_obj);out_handle(2,base_vertex);addref(index_obj);RET(OK,3);}
static void resource_device(void){out_handle(1,self()->device);addref(self()->device);RET(OK,2);}
static void priority_set(void){Object *o=self();uint32_t old=o->priority;o->priority=A32(1);RET(old,2);}static void priority_get(void){RET(self()->priority,1);}static void preload(void){Object *o=self();if(o->kind==TEXTURE&&upload(o))hle_fatal("D3D8 texture upload failed");RETV(1);}static void type_get(void){unsigned k=self()->kind;RET(k==TEXTURE?3:k==SURFACE?1:k==VBUFFER?6:7,1);}
static void lod_set(void){Object *o=self();uint32_t old=o->lod;o->lod=A32(1)<o->levels?A32(1):o->levels-1;RET(old,2);}static void lod_get(void){RET(self()->lod,1);}static void level_count(void){RET(self()->levels,1);}
static void level_desc(void){Object *o=self();unsigned l=A32(1);if(l>=o->levels){RET(INVALID,3);}descriptor(A32(2),o,l);RET(OK,3);}
static void surface_level(void){Object *o=self();unsigned l=A32(1);out_handle(2,0);if(l>=o->levels){RET(INVALID,3);}uint32_t surf=make_object(SURFACE);Object *s=object(surf);if(!s){RET(OUTOFMEMORY,3);}s->width=mipdim(o->width,l);s->height=mipdim(o->height,l);s->format=o->format;s->usage=o->usage;s->pool=o->pool;s->levels=1;s->parent=o->self;s->lod=l;addref(o->self);out_handle(2,surf);RET(OK,3);}
static int lock_rect(Object *o,unsigned l,uint32_t out,uint32_t rect){if(l>=o->levels||!out||!o->memory[l])return -1;unsigned x=0,y=0,w=mipdim(o->width,l),h=mipdim(o->height,l);if(rect){const int32_t *r=hle_ptr(rect);if(r[0]<0||r[1]<0||r[2]>w||r[3]>h||r[0]>r[2]||r[1]>r[3])return -1;x=r[0];y=r[1];}unsigned bs=block_size(o->format);if(bs&&(x%4||y%4))return -1;MEM32(out)=o->pitch[l];MEM32(out+4)=o->memory[l]+(bs?(y/4)*o->pitch[l]+(x/4)*bs:y*o->pitch[l]+x*bpp(o->format));o->dirty[l]=1;return 0;}
static void texture_lock(void){Object *o=self();if(o->gpu_dirty&&(!A32(1)&&sync_pixels(o))){RET(INVALID,5);}RET(lock_rect(o,A32(1),A32(2),A32(3))?INVALID:OK,5);}static void texture_unlock(void){Object *o=self();if(A32(1)>=o->levels){RET(INVALID,2);}o->dirty[A32(1)]=1;RET(OK,2);}static void dirty_rect(void){Object *o=self();for(unsigned l=0;l<o->levels;l++)o->dirty[l]=1;RET(OK,2);}
static void surface_desc(void){Object *o=self();unsigned l=0;Object *p=pixel_owner(o,&l);descriptor(A32(1),p,l);RET(OK,2);}
static void surface_container(void){Object *o=self();out_handle(2,o->parent);if(!o->parent){RET(0x80004002u,3);}addref(o->parent);RET(OK,3);}
static void surface_lock(void){Object *o=self();if(o->backbuffer&&!(A32(3)&16)){RET(NOTAVAILABLE,4);}if(sync_pixels(o)){RET(INVALID,4);}unsigned l=0;Object *p=pixel_owner(o,&l);RET(lock_rect(p,l,A32(1),A32(2))?INVALID:OK,4);}static void surface_unlock(void){Object *o=self();unsigned l=0;Object *p=pixel_owner(o,&l);p->dirty[l]=1;RET(OK,1);}
static void buffer_lock(void){Object *o=self();unsigned off=A32(1),n=A32(2);if(off>o->length||n>o->length-off){RET(INVALID,5);}if(!(A32(4)&16)){unsigned end=n?off+n:o->length;if(o->buffer_dirty_end<=o->buffer_dirty_start){o->buffer_dirty_start=off;o->buffer_dirty_end=end;}else{if(off<o->buffer_dirty_start)o->buffer_dirty_start=off;if(end>o->buffer_dirty_end)o->buffer_dirty_end=end;}}o->buffer_locked++;out_handle(3,o->memory[0]+off);RET(OK,5);}static void buffer_unlock(void){Object *o=self();if(o->buffer_locked)o->buffer_locked--;RET(OK,1);}static void buffer_desc(void){Object *o=self();uint32_t d[6]={o->format,o->kind==VBUFFER?6:7,o->usage,o->pool,o->length,o->fvf};memcpy(APTR(1),d,o->kind==VBUFFER?24:20);RET(OK,2);}

static void methods(uint32_t kind,unsigned count,const char *name,recomp_func_t *fns){static char names[7][100][48];win32hle_shim *m=calloc(count+1,sizeof(*m));if(!m)hle_fatal("D3D8 vtable allocation failed");for(unsigned i=0;i<count;i++){snprintf(names[kind][i],sizeof(names[kind][i]),"D3D8.%s.%u",name,i);m[i].name=names[kind][i];m[i].fn=fns[i]?fns[i]:unsupported;}vtables[kind]=hle_com_vtable(m);free(m);}
static void init_tables(void){if(vtables[ROOT])return;
    recomp_func_t rt[16]={[0]=qi,[1]=ref,[2]=unref,[4]=adapter_count,[5]=adapter_identifier,[6]=mode_count,[7]=enum_modes,[8]=adapter_mode,[9]=check_type,[10]=check_format,[11]=check_msaa,[12]=check_depth,[13]=root_caps,[14]=monitor,[15]=create_device};methods(ROOT,16,"IDirect3D8",rt);
    recomp_func_t d[97]={[0]=qi,[1]=ref,[2]=unref,[3]=coop,[4]=available,[5]=discard,[6]=get_root,[7]=dev_caps,[8]=dev_mode,[9]=creation,[11]=cursor_position,[12]=cursor_show,[14]=reset,[15]=present,[16]=backbuffer,[17]=raster_status,[18]=set_gamma,[19]=get_gamma,[20]=create_texture,[23]=create_vb,[24]=create_ib,[25]=create_rt,[26]=create_depth,[27]=create_image,[28]=copy_rects,[29]=update_texture,[31]=set_target,[32]=get_target,[33]=get_depth,[34]=begin_scene,[35]=end_scene,[36]=clear,[37]=set_transform,[38]=get_transform,[39]=multiply_transform,[40]=set_viewport,[41]=get_viewport,[42]=set_material,[43]=get_material,[44]=set_light,[45]=get_light,[46]=light_enable,[47]=light_enabled,[50]=set_rs,[51]=get_rs,[52]=begin_block,[53]=end_block,[54]=apply_block,[55]=capture_block,[56]=delete_block,[57]=create_block,[58]=set_clip_status,[59]=get_clip_status,[60]=get_texture,[61]=set_texture,[62]=get_tss,[63]=set_tss,[64]=validate,[66]=set_palette_entries,[67]=get_palette_entries,[68]=set_palette,[69]=get_palette,[70]=draw_primitive,[71]=draw_indexed,[72]=draw_up,[73]=draw_indexed_up,[75]=create_vs,[76]=set_fvf,[77]=get_fvf,[83]=set_stream,[84]=get_stream,[85]=set_indices,[86]=get_indices,[87]=create_ps,[88]=set_ps,[89]=get_ps};methods(DEVICE,97,"Device",d);
    recomp_func_t tex[19]={[0]=qi,[1]=ref,[2]=unref,[3]=resource_device,[7]=priority_set,[8]=priority_get,[9]=preload,[10]=type_get,[11]=lod_set,[12]=lod_get,[13]=level_count,[14]=level_desc,[15]=surface_level,[16]=texture_lock,[17]=texture_unlock,[18]=dirty_rect};methods(TEXTURE,19,"Texture",tex);
    recomp_func_t surf[11]={[0]=qi,[1]=ref,[2]=unref,[3]=resource_device,[7]=surface_container,[8]=surface_desc,[9]=surface_lock,[10]=surface_unlock};methods(SURFACE,11,"Surface",surf);
    recomp_func_t buf[14]={[0]=qi,[1]=ref,[2]=unref,[3]=resource_device,[7]=priority_set,[8]=priority_get,[9]=preload,[10]=type_get,[11]=buffer_lock,[12]=buffer_unlock,[13]=buffer_desc};methods(VBUFFER,14,"VertexBuffer",buf);methods(IBUFFER,14,"IndexBuffer",buf);
}
const win32hle_shim ds_d3d8_shims[]={{"Direct3DCreate8",create8},{NULL,NULL}};
