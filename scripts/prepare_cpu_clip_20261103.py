#!/usr/bin/env python3
from pathlib import Path
import sys,hashlib
root=Path(__file__).resolve().parents[1];p=Path(sys.argv[1]);s=p.read_text()
for marker in ('#include <stdarg.h>', 'static char draw_error[320];', 'void d8_renderer_begin_frame(D8Renderer *r){'):
 assert s.count(marker)==1, marker
assert 'thing_cpu_clip(' not in s, 'already patched'
s=s.replace('#include <stdarg.h>','#include <stdarg.h>\n#include <SDL.h>\n#include "../../../probe/visibility/cpu_clip.h"\n/* clip_header_sha256='+hashlib.sha256((root/'probe/visibility/cpu_clip.h').read_bytes()).hexdigest()+' */')
s=s.replace('static char draw_error[320];',r'''static unsigned clip_enabled,clip_init;
static unsigned long long clip_tested,clip_skipped,clip_refs,clip_verified,clip_failed;
static void (*cq_gen)(GLsizei,GLuint*),(*cq_delete)(GLsizei,const GLuint*),(*cq_begin)(GLenum,GLuint),(*cq_end)(GLenum),(*cq_get)(GLuint,GLenum,GLuint*);
static void clip_config(void){if(!clip_init){clip_init=1;const char *v=setting("DS_CPU_CLIP");clip_enabled=v?atoi(v):0;if(clip_enabled>2)clip_enabled=0;
 if(clip_enabled==2){
 *(void**)&cq_gen=SDL_GL_GetProcAddress("glGenQueries");*(void**)&cq_delete=SDL_GL_GetProcAddress("glDeleteQueries");*(void**)&cq_begin=SDL_GL_GetProcAddress("glBeginQuery");*(void**)&cq_end=SDL_GL_GetProcAddress("glEndQuery");*(void**)&cq_get=SDL_GL_GetProcAddress("glGetQueryObjectuiv");
 if(!cq_gen||!cq_delete||!cq_begin||!cq_end||!cq_get){fprintf(stderr,"[cpu-clip] verification unavailable\n");clip_enabled=0;}}
 fprintf(stderr,"[cpu-clip] mode=%u (0=off 1=reject 2=verify) conservative XYZ AABB clipping\n",clip_enabled);}}
static char draw_error[320];''')
s=s.replace('void d8_renderer_begin_frame(D8Renderer *r){',r'''void d8_renderer_begin_frame(D8Renderer *r){
    static unsigned clip_frames;if(clip_enabled&&++clip_frames%300==0)fprintf(stderr,"[cpu-clip] tested=%llu candidates=%llu candidate_refs=%llu verified=%llu visible_errors=%llu\n",clip_tested,clip_skipped,clip_refs,clip_verified,clip_failed);''')
needle='    select_fragment_program(r,get_frag_variant(r,frag_state_key(s,textures,stages)));';assert s.count(needle)==1
s=s.replace(needle,'''    clip_config();unsigned clip_check=0;
    if(clip_enabled && r->gpu_program && (s->fvf&14)==2 && primitive>=4 && primitive<=6){
        clip_tested++;
        if(thing_cpu_clip(s->transform[256],s->transform[2],s->transform[3],data,stride,vcount,indices,isize,base,n)){
            clip_skipped++;clip_refs+=n;
            if(clip_enabled==1)return 0;
            if(clip_verified<20000 && (clip_verified<2000 || clip_skipped%128==0))clip_check=1;
        }
    }
'''+needle)
needle='    static const GLenum modes[]={0,GL_POINTS';assert s.count(needle)==1
s=s.replace(needle,'    GLuint clip_query=0;if(clip_check){cq_gen(1,&clip_query);cq_begin(0x8C2F,clip_query);}\n'+needle)
needle='r->gl_stats.draws++;GLenum e=0;';assert s.count(needle)==1
s=s.replace(needle,r'''if(clip_check){GLuint passed=0;cq_end(0x8C2F);cq_get(clip_query,0x8866,&passed);cq_delete(1,&clip_query);clip_verified++;if(passed){clip_failed++;if(clip_failed<5)fprintf(stderr,"[cpu-clip] ERROR proposed rejection produced visible samples\n");}}
    '''+needle)
p.write_text(s)
