/* Native GLES2 fixed-function pipeline used by the guest D3D8 COM adapter.
 * The guest remains 32-bit; vertex records are decoded with memcpy rather
 * than casting arbitrary guest data to a host ABI structure. */
#include <time.h>
#include "d3d8_renderer.h"
#include "d3d8_index_range.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
/* SDL is linked into the game; declare the two GLES3 entry-point lookups we need
 * without pulling SDL headers into this translation unit. */
extern void *SDL_GL_GetProcAddress(const char *name);
/* Strong only in the threaded build: when the GL worker owns the context, a
 * direct extension-function call made from the producer thread has no current
 * context and silently does nothing. Extension entry points must then fall back
 * to the wrapped core calls (which the proxy queues). */
extern int ds_rt_active(void) __attribute__((weak));
static int render_thread_active(void){
    static int state=-1;
    if(state<0)state=ds_rt_active?ds_rt_active():0;
    return state;
}
/* glInvalidateFramebuffer attachment enums: not in the GLES2 header. */
#ifndef GL_DEPTH
#define GL_DEPTH 0x1801
#endif
#ifndef GL_STENCIL
#define GL_STENCIL 0x1802
#endif

static const char *setting(const char *name){
    if(!strncmp(name,"DS_",3)){char alias[96];snprintf(alias,sizeof alias,"THING_%s",name+3);const char *value=getenv(alias);if(value)return value;}
    return getenv(name);
}
static char draw_error[320];
static D8Renderer *raster_owner;
static D8Renderer *attribute_owner,*texture_owner;
static D8Renderer *bind_owner;
/* Binding caches. GL retains context state, so a draw only needs to reissue a
 * bind when it changes the bound object or after external code touched it.
 * DS_BIND_CACHE=0 restores the pre-cache behaviour (every bind reissued) and
 * exists so the call-count/submission A/B stays reproducible. */
static void invalidate_bindings_values(D8Renderer *r){
    r->bound_array=r->bound_element=r->current_program=r->active_unit=(GLuint)-1;
    r->bound_tex[0]=r->bound_tex[1]=(GLuint)-1;
}
void d8_renderer_invalidate_bindings(D8Renderer *r){invalidate_bindings_values(r);bind_owner=NULL;}
void d8_renderer_begin_frame(D8Renderer *r){
    if(r->ring_vb_disabled)return;
    r->ring_index=(r->ring_index+1)%3;r->ring_used=0;
}
static void bind_owner_begin(D8Renderer *r){
    if(bind_owner!=r){invalidate_bindings_values(r);bind_owner=r;}
}
static void use_program(D8Renderer *r,GLuint program){
    bind_owner_begin(r);
    if(r->bind_cache_disabled||r->current_program!=program){glUseProgram(program);r->current_program=program;r->gl_stats.use_program++;}
}
static void bind_array_buffer(D8Renderer *r,GLuint buffer){
    bind_owner_begin(r);
    if(r->bind_cache_disabled||r->bound_array!=buffer){glBindBuffer(GL_ARRAY_BUFFER,buffer);r->bound_array=buffer;r->gl_stats.bind_array++;}
}
static void bind_element_buffer(D8Renderer *r,GLuint buffer){
    bind_owner_begin(r);
    if(r->bind_cache_disabled||r->bound_element!=buffer){glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffer);r->bound_element=buffer;r->gl_stats.bind_element++;}
}
/* Facade uploads mutate only a buffer binding, not programs or textures. */
void d8_renderer_bind_upload_buffer(D8Renderer *r,int index,GLuint buffer){
    if(index)bind_element_buffer(r,buffer);else bind_array_buffer(r,buffer);
}
static void bind_texture_unit(D8Renderer *r,unsigned unit,GLuint texture){
    bind_owner_begin(r);
    if(r->bind_cache_disabled||r->active_unit!=unit){glActiveTexture(GL_TEXTURE0+unit);r->active_unit=unit;r->gl_stats.active_texture++;}
    if(r->bind_cache_disabled||r->bound_tex[unit]!=texture){glBindTexture(GL_TEXTURE_2D,texture);r->bound_tex[unit]=texture;r->gl_stats.bind_texture++;}
}
void d8_renderer_invalidate_attributes(D8Renderer *r){memset(&r->attributes,0,sizeof r->attributes);attribute_owner=NULL;}
static unsigned texture_slot(GLuint texture){return (texture*2654435761u)>>(32-10);}
static void next_texture_epoch(D8Renderer *r){if(++r->texture_epoch==0){memset(r->texture_parameters,0,sizeof r->texture_parameters);r->texture_epoch=1;}}
void d8_renderer_invalidate_texture_parameters(D8Renderer *r){next_texture_epoch(r);texture_owner=NULL;}
void d8_renderer_forget_texture(D8Renderer *r,GLuint texture){
    if(!texture)return;D8TextureParameters *p=&r->texture_parameters[texture_slot(texture)];if(p->texture==texture)p->valid=0;
    if(texture_owner!=r)texture_owner=NULL;
}
void d8_renderer_invalidate_raster(D8Renderer *r){r->raster.valid=0;raster_owner=NULL;}
const char *d8_renderer_error(void){return draw_error[0]?draw_error:"unspecified renderer rejection";}
static int reject(const char *fmt,...){va_list ap;va_start(ap,fmt);vsnprintf(draw_error,sizeof draw_error,fmt,ap);va_end(ap);return -1;}
typedef struct { float p[4], c[4], spec[4], uv[2][2], fog; } Vertex;
static float asfloat(uint32_t u) { float f; memcpy(&f,&u,4); return f; }
static inline __attribute__((always_inline)) void argb(uint32_t a,float c[4]) { c[0]=((a>>16)&255)/255.f;c[1]=((a>>8)&255)/255.f;c[2]=(a&255)/255.f;c[3]=(a>>24)/255.f; }
/* fminf/fmaxf are out-of-line libm calls on the reference host and showed up
 * at ~4 % of gameplay main-thread CPU. Inputs here are finite lighting/specular
 * arithmetic, so the explicit comparisons are value-identical. */
static inline __attribute__((always_inline)) float clampf(float x,float lo,float hi){return x<lo?lo:(x>hi?hi:x);}
static inline __attribute__((always_inline)) float maxf0(float x){return x>0.f?x:0.f;}
static void identity(float m[16]) { memset(m,0,64);m[0]=m[5]=m[10]=m[15]=1; }
void d8_matrix_multiply(float out[16], const float a[16],const float b[16]) {
    float tmp[16];for(int r=0;r<4;r++)for(int c=0;c<4;c++){tmp[r*4+c]=0;for(int k=0;k<4;k++)tmp[r*4+c]+=a[r*4+k]*b[k*4+c];}memcpy(out,tmp,64);
}
/* The CPU reference vertex path calls this three times per vertex; keeping it
 * as an out-of-line function showed up at ~2-4 % of gameplay main-thread CPU.
 * always_inline preserves the exact accumulation order. */
static inline __attribute__((always_inline)) void vector(float out[4], const float in[4], const float m[16]) {
    float t0=0,t1=0,t2=0,t3=0;
    t0+=in[0]*m[0]; t0+=in[1]*m[4]; t0+=in[2]*m[8];  t0+=in[3]*m[12];
    t1+=in[0]*m[1]; t1+=in[1]*m[5]; t1+=in[2]*m[9];  t1+=in[3]*m[13];
    t2+=in[0]*m[2]; t2+=in[1]*m[6]; t2+=in[2]*m[10]; t2+=in[3]*m[14];
    t3+=in[0]*m[3]; t3+=in[1]*m[7]; t3+=in[2]*m[11]; t3+=in[3]*m[15];
    out[0]=t0;out[1]=t1;out[2]=t2;out[3]=t3;
}
void d8_state_defaults(D8State *s,unsigned w,unsigned h) {
    memset(s,0,sizeof(*s));for(int i=0;i<512;i++)identity(s->transform[i]);
    s->rs[7]=1;s->rs[8]=3;s->rs[9]=2;s->rs[14]=1;s->rs[19]=2;s->rs[20]=1;s->rs[22]=3;s->rs[23]=4;
    s->rs[25]=8;s->rs[37]=0x3f800000;s->rs[38]=0x3f800000;s->rs[53]=s->rs[54]=s->rs[55]=1;s->rs[56]=8;
    s->rs[58]=s->rs[59]=UINT32_MAX;s->rs[60]=UINT32_MAX;s->rs[136]=s->rs[137]=s->rs[141]=1;s->rs[145]=1;s->rs[168]=15;s->rs[171]=1;
    for(int i=0;i<8;i++){s->tss[i][1]=i?1:4;s->tss[i][2]=2;s->tss[i][3]=1;s->tss[i][4]=i?1:2;s->tss[i][5]=2;s->tss[i][6]=1;
        s->tss[i][11]=(uint32_t)i;s->tss[i][13]=s->tss[i][14]=1;s->tss[i][16]=s->tss[i][17]=1;}
    s->viewport[2]=w;s->viewport[3]=h;s->viewport[5]=0x3f800000u;
    for(int i=0;i<4;i++)s->material[i]=1;
}
static GLuint shader(GLenum type,const char *src){GLuint o=glCreateShader(type);glShaderSource(o,1,&src,NULL);glCompileShader(o);GLint ok;glGetShaderiv(o,GL_COMPILE_STATUS,&ok);if(!ok){char msg[4096];glGetShaderInfoLog(o,sizeof(msg),NULL,msg);fprintf(stderr,"[d3d8] shader: %s\n",msg);glDeleteShader(o);return 0;}return o;}
/* Uniform locations belong to a linked program and remain valid until relink.
 * The overlay owns a different program, so it cannot invalidate these tables. */
static void cache_uniform_locations(GLuint program,D8UniformLocations *u){
    memset(u,0xff,sizeof(*u));
#define LOCATION(field,name) u->field=glGetUniformLocation(program,name)
    LOCATION(tex[0],"tex0");LOCATION(tex[1],"tex1");
    LOCATION(has[0],"has0");LOCATION(has[1],"has1");
    LOCATION(cop[0],"cop0");LOCATION(cop[1],"cop1");
    LOCATION(aop[0],"aop0");LOCATION(aop[1],"aop1");
#define NAMED(field) LOCATION(field,#field)
    NAMED(factor);NAMED(afunc);NAMED(aref);NAMED(fogmode);NAMED(fogcolor);NAMED(fogparam);NAMED(specular);
    NAMED(world);NAMED(view);NAMED(projection);NAMED(normalmat);NAMED(texmat0);NAMED(texmat1);NAMED(viewport);NAMED(zrange);
    NAMED(flags);NAMED(sources);NAMED(texflags);NAMED(material);NAMED(power);NAMED(ambient);NAMED(eye);NAMED(camera);NAMED(localviewer);NAMED(lights);NAMED(enabled);
#undef NAMED
#undef LOCATION
    glUseProgram(program);glUniform1i(u->tex[0],0);glUniform1i(u->tex[1],1);
}
/* Exact value caches belong to individual linked programs. Neither switching
 * programs nor drawing the overlay changes another program's uniforms. */
enum { SU_HAS0,SU_HAS1,SU_COP0,SU_COP1,SU_AOP0,SU_AOP1,SU_FACTOR,SU_FOGCOLOR,
       SU_AFUNC,SU_AREF,SU_FOGMODE,SU_FOGPARAM,SU_SPECULAR,SU_VIEWPORT,SU_ZRANGE,
       SU_FLAGS,SU_SOURCES,SU_TEXFLAGS,SU_LOCALVIEWER };
static int small_changed(D8SmallUniformCache *c,unsigned slot,const void *value,size_t size){
    uint32_t mask=1u<<slot;if((c->valid&mask)&&!memcmp(&c->value[slot],value,size))return 0;
    memcpy(&c->value[slot],value,size);c->valid|=mask;return 1;
}
static void small1i(D8SmallUniformCache *c,unsigned slot,GLint loc,GLint v){if(loc<0)return;if(small_changed(c,slot,&v,sizeof v))glUniform1i(loc,v);}
static void small4i(D8SmallUniformCache *c,unsigned slot,GLint loc,GLint a,GLint b,GLint d,GLint e){if(loc<0)return;GLint v[]={a,b,d,e};if(small_changed(c,slot,v,sizeof v))glUniform4i(loc,a,b,d,e);}
static void small1f(D8SmallUniformCache *c,unsigned slot,GLint loc,GLfloat v){if(loc<0)return;if(small_changed(c,slot,&v,sizeof v))glUniform1f(loc,v);}
static void small2f(D8SmallUniformCache *c,unsigned slot,GLint loc,GLfloat a,GLfloat b){if(loc<0)return;GLfloat v[]={a,b};if(small_changed(c,slot,v,sizeof v))glUniform2f(loc,a,b);}
static void small3f(D8SmallUniformCache *c,unsigned slot,GLint loc,GLfloat a,GLfloat b,GLfloat d){if(loc<0)return;GLfloat v[]={a,b,d};if(small_changed(c,slot,v,sizeof v))glUniform3f(loc,a,b,d);}
static void small4f(D8SmallUniformCache *c,unsigned slot,GLint loc,GLfloat a,GLfloat b,GLfloat d,GLfloat e){if(loc<0)return;GLfloat v[]={a,b,d,e};if(small_changed(c,slot,v,sizeof v))glUniform4f(loc,a,b,d,e);}
static void small4fv(D8SmallUniformCache *c,unsigned slot,GLint loc,const GLfloat v[4]){if(loc<0)return;if(small_changed(c,slot,v,4*sizeof(*v)))glUniform4fv(loc,1,v);}
static GLuint make_gpu_program(GLuint fragment,int has0,int has1,int spec);
/* Shared fragment shader pieces. The generic program declares the stage
 * state as uniforms; specialised variants declare the same symbols as
 * compile-time constants, so the GLSL compiler folds arg/op/stage away while
 * the emitted maths stays identical. */
static const char *FS_HEAD="precision mediump float;varying mediump vec4 dc;varying mediump vec4 sc;varying mediump vec2 tc0;varying mediump vec2 tc1;varying mediump float fd;";
static const char *FS_CODE_FAST=
    "vec4 arg(int n,vec4 t,vec4 cur){int code=n;bool comp=false;bool rep=false;if(code>=32){rep=true;code-=32;}if(code>=16){comp=true;code-=16;}vec4 v=dc;if(code==1)v=cur;else if(code==2)v=t;else if(code==3)v=factor;else if(code==4)v=sc;if(comp)v=vec4(1.)-v;if(rep)v=vec4(v.a);return v;}"
    "vec4 op(int n,vec4 a,vec4 b,vec4 t,vec4 cur){if(n==2)return a;if(n==3)return b;if(n==4)return a*b;if(n==5)return 2.*a*b;if(n==6)return 4.*a*b;if(n==7)return a+b;if(n==8)return a+b-.5;if(n==9)return 2.*(a+b-.5);if(n==10)return a-b;if(n==11)return a+b-a*b;if(n==12)return mix(b,a,dc.a);if(n==13)return mix(b,a,t.a);if(n==14)return mix(b,a,factor.a);if(n==15)return a+b*(1.-t.a);if(n==16)return mix(b,a,cur.a);if(n==24)return vec4(dot(a.rgb*2.-1.,b.rgb*2.-1.));return cur;}"
    "vec4 stage(ivec4 co,ivec4 ao,vec4 t,vec4 cur){if(co.x==1)return cur;vec4 o=op(co.x,arg(co.y,t,cur),arg(co.z,t,cur),t,cur);if(ao.x!=1)o.a=op(ao.x,arg(ao.y,t,cur),arg(ao.z,t,cur),t,cur).a;return clamp(o,0.,1.);}"
    "void main(){vec4 cur=dc;vec4 t0=has0?texture2D(tex0,tc0):vec4(1.);vec4 t1=has1?texture2D(tex1,tc1):vec4(1.);cur=stage(cop0,aop0,t0,cur);if(cop0.x!=1)cur=stage(cop1,aop1,t1,cur);if(specular)cur.rgb=clamp(cur.rgb+sc.rgb,0.,1.);"
    "if(afunc==1)discard;if(afunc==2&&!(cur.a<aref))discard;if(afunc==3&&!(abs(cur.a-aref)<.001))discard;if(afunc==4&&!(cur.a<=aref))discard;if(afunc==5&&!(cur.a>aref))discard;if(afunc==6&&abs(cur.a-aref)<.001)discard;if(afunc==7&&!(cur.a>=aref))discard;"
    "float f=1.;if(fogmode==4)f=fd;else if(fogmode==1)f=exp(-fogparam.z*fd);else if(fogmode==2)f=exp(-pow(fogparam.z*fd,2.));else if(fogmode==3)f=(fogparam.y-fd)/max(.0001,fogparam.y-fogparam.x);cur.rgb=mix(fogcolor.rgb,cur.rgb,clamp(f,0.,1.));gl_FragColor=cur;}";
static const char *FS_GENERIC_HEAD_FAST=
    "uniform sampler2D tex0;uniform sampler2D tex1;uniform bool has0;uniform bool has1;uniform ivec4 cop0;uniform ivec4 aop0;uniform ivec4 cop1;uniform ivec4 aop1;uniform vec4 factor;uniform int afunc;uniform float aref;uniform int fogmode;uniform vec4 fogcolor;uniform vec3 fogparam;uniform bool specular;";
static GLuint build_frag_shader_source(char *out,size_t cap,uint64_t key){
    int stages=(int)(key&3u),has0=(int)((key>>2)&1u),spec=(int)((key>>4)&1u);
    unsigned cop0=(unsigned)((key>>5)&31u),aop0=(unsigned)((key>>10)&31u);
    unsigned c0y=(unsigned)((key>>15)&31u),c0z=(unsigned)((key>>20)&31u);
    unsigned a0y=(unsigned)((key>>25)&31u),a0z=(unsigned)((key>>30)&31u);
    /* Bit 35: alpha test is disabled for this draw state, so afunc is a
     * compile-time 8. The discard block then constant-folds away and the Mali
     * driver can enable forward pixel kill, which a shader that can discard
     * never gets (Halo-G31 port notes: alpha test and forward pixel kill). */
    int noalpha=(int)((key>>35)&1u);
    (void)stages;
    /* Match the specialised vertex shader: declare (and only then use) the
     * varyings that vertex stage actually writes. has1 is always false in a
     * variant, so tc1 is replaced with a constant; tc0 exists only for
     * textured variants; sc is always written by the variant vertex shader. */
    char head[256];
    snprintf(head,sizeof head,"precision mediump float;varying mediump vec4 dc;varying mediump vec4 sc;%s%svarying mediump float fd;",
             has0?"varying mediump vec2 tc0;":"",has0?"":"");
    char code[3072];
    snprintf(code,sizeof code,"%s",FS_CODE_FAST);
    if(!has0){char *p;while((p=strstr(code,"tc0"))){memmove(p+8,p+3,strlen(p+3)+1);memcpy(p,"vec2(0.)",8);}}
    {char *p;while((p=strstr(code,"tc1"))){memmove(p+8,p+3,strlen(p+3)+1);memcpy(p,"vec2(0.)",8);}}
    snprintf(out,cap,"%s"
        "uniform sampler2D tex0;uniform sampler2D tex1;uniform vec4 factor;%s uniform float aref;uniform int fogmode;uniform vec4 fogcolor;uniform vec3 fogparam;"
        "const bool has0=%s;const bool has1=false;const bool specular=%s;"
        "const ivec4 cop0=ivec4(%u,%u,%u,0);const ivec4 aop0=ivec4(%u,%u,%u,0);const ivec4 cop1=ivec4(1,0,0,0);const ivec4 aop1=ivec4(1,0,0,0);"
        "%s",head,noalpha?"const int afunc=8;":"uniform int afunc;",has0?"true":"false",spec?"true":"false",cop0,c0y,c0z,aop0,a0y,a0z,code);
    return shader(GL_FRAGMENT_SHADER,out);
}
static uint64_t frag_state_key(const D8State *s,const GLuint textures[2],unsigned stages,int alpha_elide){
    if(stages>1)return ~(uint64_t)0;
    uint64_t has0=textures[0]?1u:0u;
    return (uint64_t)(stages&3u)|(has0<<2)|((uint64_t)(s->rs[29]!=0)<<4)
        |((uint64_t)(s->tss[0][1]&31u)<<5)|((uint64_t)(s->tss[0][4]&31u)<<10)
        |((uint64_t)(s->tss[0][2]&31u)<<15)|((uint64_t)(s->tss[0][3]&31u)<<20)
        |((uint64_t)(s->tss[0][5]&31u)<<25)|((uint64_t)(s->tss[0][6]&31u)<<30)
        |((uint64_t)((alpha_elide&&!s->rs[15])?1u:0u)<<35);
}
static int get_frag_variant(D8Renderer *r,uint64_t key);
static uint64_t variant_key(int stages,int has0,int spec,unsigned cop0,unsigned aop0,
                            unsigned cy,unsigned cz,unsigned ay,unsigned az,int noalpha){
    return (uint64_t)(stages&3)|((uint64_t)(has0?1u:0u)<<2)|((uint64_t)(spec?1u:0u)<<4)
        |((uint64_t)(cop0&31u)<<5)|((uint64_t)(aop0&31u)<<10)
        |((uint64_t)(cy&31u)<<15)|((uint64_t)(cz&31u)<<20)
        |((uint64_t)(ay&31u)<<25)|((uint64_t)(az&31u)<<30)
        |((uint64_t)(noalpha?1u:0u)<<35);
}
int d8_renderer_init(D8Renderer *r) {
    memset(r,0,sizeof(*r));const char *trim=setting("DS_GPU_TRIM_VB");r->trim_vertex_prefix=trim&&atoi(trim);
    invalidate_bindings_values(r);r->current_program=0; /* GL starts with program 0 bound */
    const char *cache=setting("DS_BIND_CACHE");r->bind_cache_disabled=cache&&!strcmp(cache,"0");
    /* Opt-in streaming ring: three buffers rotate per presented frame so no
     * upload targets in-flight storage (a single cross-frame buffer serialised
     * on Mali in the first device run). */
    const char *ring=setting("DS_RING_VB");r->ring_vb_disabled=!(ring&&!strcmp(ring,"1"));
    {const char *range=setting("DS_INDEX_RANGE");r->index_range_enabled=range&&!strcmp(range,"1");if(r->index_range_enabled)fprintf(stderr,"[d3d8] experimental index-range reduction enabled\n");}
    const char *shstat=setting("DS_SHADER_STATS");r->shader_stats=shstat&&!strcmp(shstat,"1");
    {const char *pp=setting("THING_PERF_PROBE");if(!pp)pp=setting("DS_PERF_PROBE");r->perf_enabled=pp&&!strcmp(pp,"1");}r->output_scale=1.0f;
    const char *every=setting("DS_DRAW_ERROR_CHECK_EVERY");int interval=every?atoi(every):1;
    if(interval<1)interval=1;r->draw_error_interval=(uint32_t)interval;r->draw_error_countdown=r->draw_error_interval;
    static const char *VS_FAST="attribute vec4 p;attribute vec4 c;attribute vec4 spec;attribute vec2 uv0;attribute vec2 uv1;attribute float fog;varying mediump vec4 dc;varying mediump vec4 sc;varying mediump vec2 tc0;varying mediump vec2 tc1;varying mediump float fd;void main(){gl_Position=p;dc=c;sc=spec;tc0=uv0;tc1=uv1;fd=fog;}";
    const char *vs=VS_FAST;
    char fsbuf[8192];snprintf(fsbuf,sizeof fsbuf,"%s%s%s",FS_HEAD,FS_GENERIC_HEAD_FAST,FS_CODE_FAST);const char *fs=fsbuf;
    GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);if(!v||!f)return -1;
    r->program=glCreateProgram();glAttachShader(r->program,v);glAttachShader(r->program,f);
    const char *attrs[]={"p","c","spec","uv0","uv1","fog"};for(int i=0;i<6;i++)glBindAttribLocation(r->program,(GLuint)i,attrs[i]);
    glLinkProgram(r->program);
    const char *gpu=setting("DS_GPU_VERTEX");if(gpu&&atoi(gpu)){r->gpu_program=make_gpu_program(f,1,1,1);if(r->gpu_program){glGenBuffers(1,&r->gpu_vbo);glGenBuffers(1,&r->gpu_ibo);fprintf(stderr,"[d3d8] optional GPU vertex path enabled\n");}else fprintf(stderr,"[d3d8] GPU vertex shader unavailable; using CPU reference\n");}
    {const char *one=setting("DS_ONE_UPLOAD");r->one_upload=one&&!strcmp(one,"1");if(r->one_upload){glGenBuffers(1,&r->gpu_combo);fprintf(stderr,"[d3d8] one-upload packing enabled\n");}}
    /* GLES3 entry points available on this driver: ranged base-vertex draws
     * (skip the driver's index scan) and framebuffer invalidation. */
    r->range_draw_proc=(D8RangeDrawProc)SDL_GL_GetProcAddress("glDrawRangeElementsBaseVertex");
    r->invalidate_proc=(D8InvalidateProc)SDL_GL_GetProcAddress("glInvalidateFramebuffer");
    {const char *rd=setting("DS_RANGE_DRAW");r->range_draw_enabled=r->range_draw_proc&&!(rd&&!strcmp(rd,"0"));if(r->range_draw_enabled)fprintf(stderr,"[d3d8] ranged base-vertex draws enabled\n");}
    {const char *ae=setting("DS_ALPHA_ELIDE");r->alpha_elide=!(ae&&!strcmp(ae,"0"));}
    {const char *inv=setting("DS_INVALIDATE_DEPTH");r->invalidate_depth=r->invalidate_proc&&!(inv&&!strcmp(inv,"0"));}
    GLint ok;glGetProgramiv(r->program,GL_LINK_STATUS,&ok);if(!ok){glDeleteShader(v);glDeleteShader(f);return -1;}
    r->shared_vs=v;r->generic_program=r->program;r->generic_gpu_program=r->gpu_program;
    r->gpu_path_enabled=r->gpu_program!=0;r->active_frag=-1;
    cache_uniform_locations(r->generic_program,&r->generic_uniforms);
    if(r->generic_gpu_program)cache_uniform_locations(r->generic_gpu_program,&r->generic_gpu_uniforms);
    r->uniforms=r->generic_uniforms;r->gpu_uniforms=r->generic_gpu_uniforms;
    glUseProgram(0);r->current_program=(GLuint)-1;
    glDeleteShader(f);
    glGenBuffers(1,&r->vbo);
    const char *fen=setting("DS_FAST_SHADER");r->fast_shader_enabled=!(fen&&!strcmp(fen,"0"));
    if(r->fast_shader_enabled){
        /* Pre-build the states the device draw-state histogram shows in normal
         * play (all use args texture/current; fog and alpha-test stay dynamic
         * uniforms so they do not split the key). Compiling these at startup
         * removes the first-appearance shader-compile hitches observed as
         * stutter in the on-demand build. */
        static const struct {int stages,has0,spec;unsigned cop0,aop0;} common[]={
            {1,1,0,5,4}, /* texture*2*diffuse modulate2x: 87.5% of device draws */
            {0,0,0,1,1}, /* untextured diffuse */
            {1,1,1,5,4}, /* ...with specular */
            {1,1,0,4,4}, /* texture*diffuse modulate */
            {0,0,1,5,4},
            {0,0,0,4,4},
        };
        for(unsigned i=0;i<sizeof(common)/sizeof(common[0]);i++){
            (void)get_frag_variant(r,variant_key(common[i].stages,common[i].has0,common[i].spec,common[i].cop0,common[i].aop0,2,1,2,1,0));
            /* The same common state without alpha test: no discard instruction,
             * so Mali can enable forward pixel kill for opaque draws. */
            if(r->alpha_elide)(void)get_frag_variant(r,variant_key(common[i].stages,common[i].has0,common[i].spec,common[i].cop0,common[i].aop0,2,1,2,1,1));
        }
        /* Additional states seen in lead-variants2/run.log. The histogram's
         * bit layout differs from variant_key; decode explicitly. */
        static const uint64_t observed[]={
            0x0000021104442405ULL,0x00000211044422c5ULL,
            0x0000021104442400ULL,0x0000021104442c25ULL,
            0x0000021104442c35ULL,0x0000021104410c00ULL,
            0x0000021104442425ULL,0x0000021104442420ULL,
            0x0000021104442ae5ULL,0x0000021104442c30ULL,
            0x0000021104410ac0ULL,0x0000021104442c20ULL};
        for(unsigned i=0;i<sizeof(observed)/sizeof(observed[0]);i++){
            uint64_t k=observed[i];
            if((k&3)>1||(k&8))continue;
            (void)get_frag_variant(r,variant_key(k&3,(k>>2)&1,(k>>4)&1,
                (k>>11)&31,(k>>16)&31,(k>>21)&31,(k>>26)&31,
                (k>>31)&31,(k>>36)&31,0));
        }
        fprintf(stderr,"[d3d8] fragment specialisation enabled (%u common variants prebuilt, max %d)\n",r->frag_count,DS_FRAG_VARIANTS);
    }
    return 0;
}
void d8_renderer_close(D8Renderer *r){if(raster_owner==r)raster_owner=NULL;if(attribute_owner==r)attribute_owner=NULL;if(texture_owner==r)texture_owner=NULL;if(bind_owner==r)bind_owner=NULL;glDeleteBuffers(3,r->ring_vbo);glDeleteBuffers(1,&r->gpu_vbo);glDeleteBuffers(1,&r->gpu_ibo);glDeleteBuffers(1,&r->gpu_combo);free(r->combo_staging);for(unsigned i=0;i<r->frag_count;i++){if(r->frag_variants[i].cpu_program)glDeleteProgram(r->frag_variants[i].cpu_program);if(r->frag_variants[i].gpu_program)glDeleteProgram(r->frag_variants[i].gpu_program);if(r->frag_variants[i].fragment)glDeleteShader(r->frag_variants[i].fragment);}
    glDeleteProgram(r->generic_gpu_program);glDeleteProgram(r->generic_program);if(r->shared_vs)glDeleteShader(r->shared_vs);glDeleteBuffers(1,&r->vbo);free(r->rebased_indices);free(r->vertices);memset(r,0,sizeof(*r));}
void d8_renderer_report_gl_stats(D8Renderer *r){
    const char *enabled=setting("DS_GL_STATS");if(!enabled||strcmp(enabled,"1"))return;
    fprintf(stderr,"[d3d8-gl] calls use_program=%llu bind_array=%llu bind_element=%llu active_texture=%llu bind_texture=%llu draws=%llu range_draws=%llu error_checks=%llu error_detections=%llu buffer_data=%llu buffer_subdata=%llu bind_cache=%s ring_vb=%s error_check_every=%u\n",
        (unsigned long long)r->gl_stats.use_program,(unsigned long long)r->gl_stats.bind_array,
        (unsigned long long)r->gl_stats.bind_element,(unsigned long long)r->gl_stats.active_texture,
        (unsigned long long)r->gl_stats.bind_texture,(unsigned long long)r->gl_stats.draws,(unsigned long long)r->gl_stats.range_draws,
        (unsigned long long)r->gl_stats.error_checks,(unsigned long long)r->gl_stats.error_detections,
        (unsigned long long)r->gl_stats.buffer_data,(unsigned long long)r->gl_stats.buffer_subdata,
        r->bind_cache_disabled?"off":"on",r->ring_vb_disabled?"off":"on",r->draw_error_interval);
    if(r->fast_shader_enabled)fprintf(stderr,"[d3d8-shader] variants=%u active=%d draws=%llu\n",r->frag_count,r->active_frag,(unsigned long long)r->fast_draws);
    if(r->shader_stats)for(unsigned i=0;i<r->shader_hist_n;i++)fprintf(stderr,"[d3d8-shader-state] key=%016llx draws=%llu\n",(unsigned long long)r->shader_hist[i].key,(unsigned long long)r->shader_hist[i].count);
}
void d8_renderer_note_buffer_upload(D8Renderer *r,int index,size_t bytes){
    if(index){r->uploads.index_bytes+=bytes;r->uploads.index_uploads++;}
    else{r->uploads.vertex_bytes+=bytes;r->uploads.vertex_uploads++;}
}
D8UploadStats d8_renderer_take_upload_stats(D8Renderer *r){D8UploadStats value=r->uploads;memset(&r->uploads,0,sizeof r->uploads);return value;}
unsigned d8_primitive_vertices(unsigned t,unsigned n){if(t==1)return n;if(t==2)return n*2;if(t==3)return n+1;if(t==4)return n*3;if(t==5||t==6)return n+2;return 0;}
unsigned d8_fvf_stride(uint32_t f){unsigned p=f&14,n=(f>>8)&15;if(p!=2&&p!=4)return 0;if(n>8)return 0;unsigned z=p==4?16:12;if(f&16)z+=12;if(f&32)z+=4;if(f&64)z+=4;if(f&128)z+=4;for(unsigned i=0;i<n;i++){unsigned d=(f>>(16+2*i))&3;z+=(d==0?2:d==1?3:d==2?4:1)*4;}return z;}
static GLenum cmp(unsigned n){static const GLenum a[]={GL_ALWAYS,GL_NEVER,GL_LESS,GL_EQUAL,GL_LEQUAL,GL_GREATER,GL_NOTEQUAL,GL_GEQUAL,GL_ALWAYS};return n<9?a[n]:GL_ALWAYS;}
static GLenum blend(unsigned n){static const GLenum a[]={GL_ONE,GL_ZERO,GL_ONE,GL_SRC_COLOR,GL_ONE_MINUS_SRC_COLOR,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA,GL_DST_COLOR,GL_ONE_MINUS_DST_COLOR,GL_SRC_ALPHA_SATURATE};return n<12?a[n]:GL_ONE;}
static GLenum stencil(unsigned n){static const GLenum a[]={GL_KEEP,GL_KEEP,GL_ZERO,GL_REPLACE,GL_INCR,GL_DECR,GL_INVERT,GL_INCR_WRAP,GL_DECR_WRAP};return n<9?a[n]:GL_KEEP;}
static int raster_changed(D8RasterCache *c,unsigned bit,void *old,const void *key,size_t bytes){
    uint32_t mask=1u<<bit;if((c->valid&mask)&&!memcmp(old,key,bytes))return 0;
    memcpy(old,key,bytes);c->valid|=mask;return 1;
}
static void raster(D8Renderer *r,const D8State *s,unsigned target_height){
    D8RasterCache *c=&r->raster;
    /* Raster state belongs to the GL context, unlike program-owned uniforms.
     * Two renderer instances interleaving in one context must reapply state. */
    if(raster_owner!=r){c->valid=0;raster_owner=r;}
    uint32_t depth[]={s->rs[7],s->rs[14],s->rs[23]};
    if(raster_changed(c,0,c->depth,depth,sizeof depth)){
        if(s->rs[7])glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
        glDepthMask(s->rs[14]!=0);glDepthFunc(cmp(s->rs[23]));
    }
    uint32_t blending[]={s->rs[27],s->rs[19],s->rs[20],s->rs[171]};
    if(raster_changed(c,1,c->blend,blending,sizeof blending)){
        if(s->rs[27])glEnable(GL_BLEND);else glDisable(GL_BLEND);
        glBlendFunc(blend(s->rs[19]),blend(s->rs[20]));
        glBlendEquation(s->rs[171]==2?GL_FUNC_SUBTRACT:s->rs[171]==3?GL_FUNC_REVERSE_SUBTRACT:GL_FUNC_ADD);
    }
    if(raster_changed(c,2,&c->cull,&s->rs[22],4)){
        if(s->rs[22]==1)glDisable(GL_CULL_FACE);else{glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(s->rs[22]==2?GL_CCW:GL_CW);}
    }
    uint32_t stencil_state[]={s->rs[52],s->rs[56],s->rs[57],s->rs[58],s->rs[53],s->rs[54],s->rs[55],s->rs[59]};
    if(raster_changed(c,3,c->stencil,stencil_state,sizeof stencil_state)){
        if(s->rs[52]){glEnable(GL_STENCIL_TEST);glStencilFunc(cmp(s->rs[56]),(GLint)s->rs[57],s->rs[58]);glStencilOp(stencil(s->rs[53]),stencil(s->rs[54]),stencil(s->rs[55]));glStencilMask(s->rs[59]);}else glDisable(GL_STENCIL_TEST);
    }
    if(raster_changed(c,4,&c->color,&s->rs[168],4))glColorMask((s->rs[168]&1)!=0,(s->rs[168]&2)!=0,(s->rs[168]&4)!=0,(s->rs[168]&8)!=0);
    if(raster_changed(c,5,&c->bias,&s->rs[47],4)){
        if(s->rs[47]){glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(0,-(float)s->rs[47]);}else glDisable(GL_POLYGON_OFFSET_FILL);
    }
    uint32_t viewport[]={s->viewport[0],s->viewport[1],s->viewport[2],s->viewport[3],target_height};
    if(raster_changed(c,6,c->viewport,viewport,sizeof viewport)){
        if(r->output_scale==1.0f)glViewport(s->viewport[0],(GLint)target_height-(GLint)s->viewport[1]-s->viewport[3],s->viewport[2],s->viewport[3]);
        else{float sc=r->output_scale,gh=(float)target_height*sc;
            glViewport((GLint)(s->viewport[0]*sc+0.5f),(GLint)(gh-(float)(s->viewport[1]+s->viewport[3])*sc+0.5f),
                       (GLint)((float)s->viewport[2]*sc+0.5f),(GLint)((float)s->viewport[3]*sc+0.5f));}
    }
    if(raster_changed(c,7,c->depth_range,&s->viewport[4],8))glDepthRangef(asfloat(s->viewport[4]),asfloat(s->viewport[5]));
}
static inline __attribute__((always_inline)) void normalize(float v[3]){float n=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);if(n>0){v[0]/=n;v[1]/=n;v[2]/=n;}}
static int inverse3(const float m[16],float inv[9]) {
    float a=m[0],b=m[1],c=m[2],d=m[4],e=m[5],f=m[6],g=m[8],h=m[9],i=m[10];
    float det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(fabsf(det)<1e-20f)return -1;
    inv[0]=(e*i-f*h)/det;inv[1]=(c*h-b*i)/det;inv[2]=(b*f-c*e)/det;
    inv[3]=(f*g-d*i)/det;inv[4]=(a*i-c*g)/det;inv[5]=(c*d-a*f)/det;
    inv[6]=(d*h-e*g)/det;inv[7]=(b*g-a*h)/det;inv[8]=(a*e-b*d)/det;return 0;
}
static const float *material_source(unsigned source,const float *material,const float *color,const float *spec,uint32_t fvf) {
    return source==1&&(fvf&64)?color:source==2&&(fvf&128)?spec:material;
}
/* These matrices are draw invariants. Reuse only their inverse results;
 * normal/eye vectors and all vertex arithmetic retain the reference order. */
typedef struct {float world[9],view[9];int world_valid,view_valid;} D8CpuLighting;
static void cpu_lighting_prepare(const D8State *s,D8CpuLighting *cache){
    cache->world_valid=!inverse3(s->transform[256],cache->world);
    cache->view_valid=s->rs[142]&&!inverse3(s->transform[2],cache->view);
}
static void light_vertex(const D8State *s,const D8CpuLighting *cache,const float p[4],const float normal[3],float color[4],float specular[4]){
    float ambient[4],n[3]={normal[0],normal[1],normal[2]};argb(s->rs[139],ambient);
    if(cache->world_valid)for(int j=0;j<3;j++)n[j]=normal[0]*cache->world[j*3]+normal[1]*cache->world[j*3+1]+normal[2]*cache->world[j*3+2];
    if(s->rs[143])normalize(n);
    const float *diffuse=material_source(s->rs[141]?s->rs[145]:0,s->material,color,specular,s->fvf);
    const float *ma=material_source(s->rs[141]?s->rs[147]:0,s->material+4,color,specular,s->fvf);
    const float *ms=material_source(s->rs[141]?s->rs[146]:0,s->material+8,color,specular,s->fvf);
    const float *me=material_source(s->rs[141]?s->rs[148]:0,s->material+12,color,specular,s->fvf);
    float c[4],highlight[4]={0,0,0,specular[3]},eye[3]={-s->transform[2][2],-s->transform[2][6],-s->transform[2][10]};
    if(cache->view_valid)for(int j=0;j<3;j++)eye[j]=-(s->transform[2][12]*cache->view[j]+s->transform[2][13]*cache->view[3+j]+s->transform[2][14]*cache->view[6+j])-p[j];
    normalize(eye);
    for(int k=0;k<3;k++)c[k]=me[k]+ambient[k]*ma[k];c[3]=diffuse[3];
    for(int i=0;i<8;i++)if(s->light_on[i]){
        const float *l=s->light[i];uint32_t type;memcpy(&type,l,4);float dir[3],att=1;
        if(type==3){dir[0]=-l[16];dir[1]=-l[17];dir[2]=-l[18];}
        else if(type==1||type==2){dir[0]=l[13]-p[0];dir[1]=l[14]-p[1];dir[2]=l[15]-p[2];float distance=sqrtf(dir[0]*dir[0]+dir[1]*dir[1]+dir[2]*dir[2]);if(distance>l[19])continue;att=1/(l[21]+l[22]*distance+l[23]*distance*distance>0.000001f?l[21]+l[22]*distance+l[23]*distance*distance:0.000001f);}
        else continue;
        normalize(dir);
        if(type==2){float spot[3]={l[16],l[17],l[18]};normalize(spot);float cosine=-(dir[0]*spot[0]+dir[1]*spot[1]+dir[2]*spot[2]),outer=cosf(l[25]*.5f),inner=cosf(l[24]*.5f);if(cosine<=outer)continue;if(cosine<inner)att*=powf((cosine-outer)/(inner-outer>1e-8f?inner-outer:1e-8f),l[20]);}
        float dot=maxf0(n[0]*dir[0]+n[1]*dir[1]+n[2]*dir[2]);
        for(int k=0;k<3;k++)c[k]+=att*(l[1+k]*diffuse[k]*dot+l[9+k]*ma[k]);
        if(s->rs[29]&&dot>0){float half[3]={dir[0]+eye[0],dir[1]+eye[1],dir[2]+eye[2]};normalize(half);float power=powf(maxf0(n[0]*half[0]+n[1]*half[1]+n[2]*half[2]),s->material[16]);for(int k=0;k<3;k++)highlight[k]+=att*l[5+k]*ms[k]*power;}
    }
    for(int k=0;k<4;k++){color[k]=clampf(c[k],0.f,1.f);specular[k]=clampf(highlight[k],0.f,1.f);}
}
/* Opt-in native FVF path. Matrices are row-major D3D matrices: uploading with
 * transpose=false gives the column-vector transpose required by GLSL. */
static GLuint make_gpu_program(GLuint fragment,int has0,int has1,int spec){
 const char *src=
 "%s"
 "precision highp float;attribute vec4 p;attribute vec3 normal;attribute vec4 color;attribute vec4 spec;\n"
 "#ifdef VS_HAS0\nattribute vec4 uv0;\n#endif\n#ifdef VS_HAS1\nattribute vec4 uv1;\n#endif\n"
 "varying mediump vec4 dc;\n"
 "#ifdef VS_SPEC\nvarying mediump vec4 sc;\n#endif\n"
 "#ifdef VS_HAS0\nvarying mediump vec2 tc0;\n#endif\n"
 "#ifdef VS_HAS1\nvarying mediump vec2 tc1;\n#endif\n"
 "varying mediump float fd;"
 "uniform mat4 world,view,projection,texmat0,texmat1;uniform mat3 normalmat;uniform vec4 viewport;uniform vec2 zrange;uniform vec3 eye,camera;uniform bool localviewer;"
 "uniform ivec4 flags;uniform ivec4 sources;uniform ivec4 texflags;uniform vec4 material[4];uniform float power;uniform vec4 ambient;%s"
 "uniform vec4 lights[56];uniform vec4 enabled[2];"
 "vec3 norm(vec3 a){float n=length(a);return n>0.?a/n:a;}"
 "vec4 source(int i,vec4 m){if(i==1)return color.bgra;if(i==2)return spec.bgra;return m;}"
 "vec2 coord(vec4 uv,mat4 m,int f,int flip){if(f!=0)uv=m*uv;float q=1.;if(f>=256){int n=f-256;if(n==1)q=uv.x;else if(n==2)q=uv.y;else if(n==3)q=uv.z;else q=uv.w;}if(q==0.)q=1.;vec2 t=uv.xy/q;if(flip!=0)t.y=1.-t.y;return t;}"
 "void main(){%sdc=color.bgra;\n"
 "#ifdef VS_SPEC\nsc=spec.bgra;\n#endif\n"
 "#ifdef VS_HAS0\ntc0=coord(uv0,texmat0,texflags.x,texflags.z);\n#endif\n"
 "#ifdef VS_HAS1\ntc1=coord(uv1,texmat1,texflags.y,texflags.w);\n#endif\n"
 "if(flags.x!=0){float w=p.w!=0.?1./p.w:1.;vec2 xy=(p.xy+vec2(.5)-viewport.xy)/viewport.zw;float z=zrange.y!=zrange.x?(p.z-zrange.x)/(zrange.y-zrange.x):0.;gl_Position=vec4((xy.x*2.-1.)*w,(1.-xy.y*2.)*w,(2.*z-1.)*w,w);fd=flags.w!=0?p.z:spec.a;}"
 "else {vec4 wp=world*vec4(p.xyz,1.);vec4 vp=view*wp;gl_Position=projection*vp;gl_Position.z=2.*gl_Position.z-gl_Position.w;fd=abs(vp.z);"
 "if(flags.y!=0){vec3 n=normalmat*normal;if(flags.z!=0)n=norm(n);vec4 md=source(sources.x,material[0]);vec4 ma=source(sources.y,material[1]);vec4 ms=source(sources.z,material[2]);vec4 me=source(sources.w,material[3]);"
 "vec3 c=me.rgb+ambient.rgb*ma.rgb;vec3 highlight=vec3(0.);vec3 e=localviewer?norm(camera-wp.xyz):norm(eye);"
 "for(int i=0;i<8;i++){float on=0.;if(i<4){if(i==0)on=enabled[0].x;if(i==1)on=enabled[0].y;if(i==2)on=enabled[0].z;if(i==3)on=enabled[0].w;}else{if(i==4)on=enabled[1].x;if(i==5)on=enabled[1].y;if(i==6)on=enabled[1].z;if(i==7)on=enabled[1].w;}if(on==0.)continue;"
 "int j=i*7;vec4 a=lights[j];vec4 b=lights[j+1];vec4 c0=lights[j+2];vec4 d=lights[j+3];vec4 e0=lights[j+4];vec4 f=lights[j+5];vec4 g=lights[j+6];float type=a.x;vec3 l;float att=1.;"
 "if(type==3.)l=-e0.xyz;else if(type==1.||type==2.){l=d.yzw-wp.xyz;float distance=length(l);if(distance>e0.w)continue;att=1./max(.000001,f.y+f.z*distance+f.w*distance*distance);}else continue;l=norm(l);"
 "if(type==2.){float cosine=-dot(l,norm(e0.xyz));float outer=cos(g.y*.5),inner=cos(g.x*.5);if(cosine<=outer)continue;if(cosine<inner)att*=pow((cosine-outer)/max(.00000001,inner-outer),f.x);}"
 "float nd=max(0.,dot(n,l));c+=att*(a.yzw*md.rgb*nd+c0.yzw*ma.rgb);if(g.z!=0.&&nd>0.)highlight+=att*b.yzw*ms.rgb*pow(max(0.,dot(n,norm(l+e))),power);"
 "}dc=clamp(vec4(c%s,md.a),0.,1.);\n"
 "#ifdef VS_SPEC\nsc=clamp(vec4(highlight,spec.a),0.,1.);\n#endif\n}}}";
 char srcbuf[8192];
 char defs[128];
 snprintf(defs,sizeof defs,"%s%s%s",has0?"#define VS_HAS0\n":"",has1?"#define VS_HAS1\n":"",spec?"#define VS_SPEC\n":"");
 snprintf(srcbuf,sizeof srcbuf,src,defs,"",
          "",
          "");
 GLuint v=shader(GL_VERTEX_SHADER,srcbuf);if(!v)return 0;GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,fragment);
 const char *attrs[]={"p","normal","color","spec","uv0","uv1"};for(int i=0;i<6;i++)glBindAttribLocation(p,i,attrs[i]);glLinkProgram(p);glDeleteShader(v);GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){char msg[2048];glGetProgramInfoLog(p,sizeof msg,NULL,msg);fprintf(stderr,"[d3d8] GPU vertex link: %s\n",msg);glDeleteProgram(p);return 0;}return p;
}
static void attributes_begin(D8Renderer *r){
    if(attribute_owner!=r){memset(&r->attributes,0,sizeof r->attributes);attribute_owner=r;}
}
static void gpu_attribute(D8Renderer *r,unsigned a,unsigned n,GLenum type,int normalized,size_t stride,size_t offset,GLuint buffer){
    D8AttributeCache *c=&r->attributes;uint32_t bit=1u<<a;
    if(!(c->enable_valid&bit)||!(c->enabled&bit)){glEnableVertexAttribArray(a);c->enable_valid|=bit;c->enabled|=bit;}
    /* Enabled-array draws may leave the generic current value undefined. */
    c->constant_valid&=~bit;
    if(!(c->pointer_valid&bit)||c->pointer[a].buffer!=buffer||c->pointer[a].size!=(GLint)n||c->pointer[a].type!=type||c->pointer[a].normalized!=(GLboolean)normalized||c->pointer[a].stride!=(GLsizei)stride||c->pointer[a].offset!=offset){
        glVertexAttribPointer(a,n,type,normalized,stride,(void*)offset);
        c->pointer[a].buffer=buffer;c->pointer[a].size=n;c->pointer[a].type=type;c->pointer[a].normalized=normalized;c->pointer[a].stride=stride;c->pointer[a].offset=offset;c->pointer_valid|=bit;
    }
}
static void gpu_constant(D8Renderer *r,unsigned a,float x,float y,float z,float w){
    D8AttributeCache *c=&r->attributes;uint32_t bit=1u<<a;float v[]={x,y,z,w};
    if(!(c->enable_valid&bit)||(c->enabled&bit)){glDisableVertexAttribArray(a);c->enable_valid|=bit;c->enabled&=~bit;}
    if(!(c->constant_valid&bit)||memcmp(c->constant[a],v,sizeof v)){glVertexAttrib4f(a,x,y,z,w);memcpy(c->constant[a],v,sizeof v);c->constant_valid|=bit;}
}
static void texture_parameters(D8Renderer *r,GLuint texture,const uint32_t tss[32]){
    if(texture_owner!=r){next_texture_epoch(r);texture_owner=r;}
    D8TextureParameters *p=&r->texture_parameters[texture_slot(texture)];
    if(p->texture!=texture||p->epoch!=r->texture_epoch){p->texture=texture;p->epoch=r->texture_epoch;p->valid=0;}
    GLint value[]={tss[16]>1?GL_LINEAR:GL_NEAREST,tss[17]>1?GL_LINEAR:GL_NEAREST,tss[13]==1?GL_REPEAT:tss[13]==2?GL_MIRRORED_REPEAT:GL_CLAMP_TO_EDGE,tss[14]==1?GL_REPEAT:tss[14]==2?GL_MIRRORED_REPEAT:GL_CLAMP_TO_EDGE};
    static const GLenum name[]={GL_TEXTURE_MAG_FILTER,GL_TEXTURE_MIN_FILTER,GL_TEXTURE_WRAP_S,GL_TEXTURE_WRAP_T};
    for(unsigned i=0;i<4;i++)if(!(p->valid&(1u<<i))||p->parameter[i]!=value[i]){glTexParameteri(GL_TEXTURE_2D,name[i],value[i]);p->parameter[i]=value[i];p->valid|=1u<<i;}
}
/* Cache only values owned by this specific linked GPU program. GL program
 * switches (including the FPS overlay) leave uniform values intact. */
static int gpu_upload_changed(D8GpuUploadCache *c,unsigned bit,void *old,const void *value,size_t size){
    uint32_t mask=1u<<bit;if((c->valid&mask)&&!memcmp(old,value,size))return 0;
    memcpy(old,value,size);c->valid|=mask;return 1;
}
/* Uniform values belong to GL programs. Keep their mirrors in place instead
 * of copying the matrix/light caches at every fragment-program switch. */
static D8SmallUniformCache *gpu_small_cache(D8Renderer *r){
 return r->active_frag>=0?&r->frag_gpu_small[r->active_frag]:&r->generic_gpu_small;
}
static D8GpuUploadCache *gpu_large_cache(D8Renderer *r){
 return r->active_frag>=0?&r->frag_uploaded[r->active_frag]:&r->generic_uploaded;
}
static int gpu_prepare(D8Renderer *r,const D8State *s,unsigned n,const void *data,size_t stride,unsigned vcount,const void *indices,unsigned isize,unsigned base,unsigned stages,const D8NativeBuffers *native){
 unsigned max=0,min=UINT32_MAX,pos=s->fvf&14,tc=(s->fvf>>8)&15;r->gpu_index_offset=0;
 if(native&&native->index&&indices&&(native->index_offset>native->index_bytes||(size_t)n*isize>native->index_bytes-native->index_offset))return reject("native index buffer range exceeds allocation: offset=%zu bytes=%zu count=%u size=%u",native->index_offset,native->index_bytes,n,isize);

 struct timespec index_t0,index_t1;if(r->perf_enabled)clock_gettime(CLOCK_MONOTONIC,&index_t0);
 /* XYZ indices need only a range reduction. Keep RHW checks and invalid
  * draw diagnostics in their original ordered loop. Nonindexed XYZ draws
  * have an implicit contiguous range and do not need a per-vertex scan. */
 int scan_valid=0;
 if(r->index_range_enabled&&pos!=4){
     if(indices){
         if(isize!=2&&isize!=4)return reject("invalid GPU index size %u",isize);
         d8_index_range(indices,n,isize,&min,&max);
     }else if(n){min=0;max=n-1;}
     scan_valid=!n||(max<vcount&&max<=UINT32_MAX-base);
 }
 if(!scan_valid){
 for(unsigned i=0;i<n;i++){unsigned ix=i;if(indices){if(isize==2){uint16_t x;memcpy(&x,(const char*)indices+i*2,2);ix=x;}else if(isize==4)memcpy(&ix,(const char*)indices+i*4,4);else return reject("invalid GPU index size %u",isize);}if(ix>=vcount||ix>UINT32_MAX-base)return reject("GPU index outside vertex range: i=%u index=%u count=%u base=%u",i,ix,vcount,base);if(ix>max)max=ix;if(ix<min)min=ix;
 if(pos==4){float rhw;memcpy(&rhw,(const char*)data+(size_t)(ix+base)*stride+12,4);if(!isfinite(rhw))return reject("invalid GPU RHW: vertex=%u rhw=%.9g",i,rhw);}}
 }

 if(r->perf_enabled){clock_gettime(CLOCK_MONOTONIC,&index_t1);r->perf_index_ns+=(uint64_t)(index_t1.tv_sec-index_t0.tv_sec)*1000000000ull+(uint64_t)(index_t1.tv_nsec-index_t0.tv_nsec);r->perf_index_draws++;}
 const void *uploaded_indices=indices;unsigned first=r->trim_vertex_prefix&&indices&&!(native&&(native->vertex||native->index))?min:0;
 size_t attribute_base=0;
 if(native&&native->vertex){
     uint64_t index=(uint64_t)base+max;size_t need=d8_fvf_stride(s->fvf);
     if(native->vertex_offset>native->vertex_bytes||index>(native->vertex_bytes-native->vertex_offset)/stride)return reject("native vertex buffer range exceeds allocation: offset=%zu bytes=%zu base=%u max=%u stride=%zu",native->vertex_offset,native->vertex_bytes,base,max,stride);
     size_t offset=native->vertex_offset+(size_t)index*stride;
     if(need>native->vertex_bytes-offset)return reject("native vertex buffer range exceeds allocation: offset=%zu need=%zu bytes=%zu",offset,need,native->vertex_bytes);
     attribute_base=native->vertex_offset+(size_t)base*stride;
 }
 /* Ranged base-vertex draw parameters: start/end are the index values of the
  * drawn range (0..max-first after any trim rebase), basevertex is the vertex
  * offset added by the driver. Native vertex buffers fold the base into the
  * attribute pointers, so their basevertex is 0. */
 /* Both upload paths already fold the vertex base into the buffer start /
  * attribute pointer, so the driver only needs the index range: basevertex 0. */
 if(indices&&r->range_draw_enabled){r->range_start=(GLuint)(first?0:min);r->range_end=(GLuint)(max-first);r->range_base=0;}
 if(first){
     size_t bytes=(size_t)n*isize;if(r->rebased_capacity<bytes){void *scratch=realloc(r->rebased_indices,bytes);if(!scratch)return reject("GPU index rebasing allocation failed: %zu bytes",bytes);r->rebased_indices=scratch;r->rebased_capacity=bytes;}
     for(unsigned i=0;i<n;i++){if(isize==2){uint16_t ix;memcpy(&ix,(const char*)indices+i*2,2);ix=(uint16_t)(ix-first);memcpy((char*)r->rebased_indices+i*2,&ix,2);}else{uint32_t ix;memcpy(&ix,(const char*)indices+i*4,4);ix-=first;memcpy((char*)r->rebased_indices+i*4,&ix,4);}}
     uploaded_indices=r->rebased_indices;
 }
 GLuint p=r->gpu_program;use_program(r,p);
 const D8UniformLocations *u=&r->gpu_uniforms;D8SmallUniformCache *small=gpu_small_cache(r);
 D8GpuUploadCache *uploaded=gpu_large_cache(r);
 /* The RHW branch consumes screen coordinates directly. Leave dormant
  * snapshots unchanged so the next XYZ draw uploads its actual state. */
 if(pos!=4){
 float inv[9];
 if(gpu_upload_changed(uploaded,0,uploaded->matrix[0],s->transform[256],64)){
     glUniformMatrix4fv(u->world,1,GL_FALSE,s->transform[256]);
     float nm[9];if(inverse3(s->transform[256],inv)){memset(inv,0,sizeof inv);inv[0]=inv[4]=inv[8]=1;}
     for(int i=0;i<3;i++)for(int j=0;j<3;j++)nm[i*3+j]=inv[j*3+i];glUniformMatrix3fv(u->normalmat,1,GL_FALSE,nm);
 }
 if(gpu_upload_changed(uploaded,1,uploaded->matrix[1],s->transform[2],64)){
     glUniformMatrix4fv(u->view,1,GL_FALSE,s->transform[2]);
     float eye[3]={-s->transform[2][2],-s->transform[2][6],-s->transform[2][10]},camera[3]={0};
     if(!inverse3(s->transform[2],inv))for(int j=0;j<3;j++)camera[j]=-(s->transform[2][12]*inv[j]+s->transform[2][13]*inv[3+j]+s->transform[2][14]*inv[6+j]);
     glUniform3fv(u->eye,1,eye);glUniform3fv(u->camera,1,camera);
 }
 if(gpu_upload_changed(uploaded,2,uploaded->matrix[2],s->transform[3],64))glUniformMatrix4fv(u->projection,1,GL_FALSE,s->transform[3]);
 }
 if(gpu_upload_changed(uploaded,3,uploaded->matrix[3],s->transform[16],64))glUniformMatrix4fv(u->texmat0,1,GL_FALSE,s->transform[16]);
 if(gpu_upload_changed(uploaded,4,uploaded->matrix[4],s->transform[17],64))glUniformMatrix4fv(u->texmat1,1,GL_FALSE,s->transform[17]);
 small4f(small,SU_VIEWPORT,u->viewport,s->viewport[0],s->viewport[1],s->viewport[2],s->viewport[3]);small2f(small,SU_ZRANGE,u->zrange,asfloat(s->viewport[4]),asfloat(s->viewport[5]));
 small4i(small,SU_FLAGS,u->flags,pos==4,s->rs[137]&&(s->fvf&16),s->rs[143],s->rs[35]);
 int lighting=pos!=4&&s->rs[137]&&(s->fvf&16);
 if(lighting){
 unsigned src[4]={s->rs[145],s->rs[147],s->rs[146],s->rs[148]};for(int i=0;i<4;i++)if(!s->rs[141]||(src[i]==1&&!(s->fvf&64))||(src[i]==2&&!(s->fvf&128)))src[i]=0;small4i(small,SU_SOURCES,u->sources,src[0],src[1],src[2],src[3]);
 }
 small4i(small,SU_TEXFLAGS,u->texflags,stages>0?s->tss[0][24]:0,stages>1?s->tss[1][24]:0,stages>0?s->texture_flipy[0]:0,stages>1?s->texture_flipy[1]:0);
 /* Unlit XYZ and RHW draws cannot consume lighting uniforms. Crucially,
  * do not mark deferred values as uploaded when updating guest state. */
 if(lighting){
 if(gpu_upload_changed(uploaded,5,uploaded->material,s->material,sizeof s->material)){glUniform4fv(u->material,4,s->material);glUniform1f(u->power,s->material[16]);}
 if(gpu_upload_changed(uploaded,6,&uploaded->ambient,&s->rs[139],4)){float amb[4];argb(s->rs[139],amb);glUniform4fv(u->ambient,1,amb);}
 small1i(small,SU_LOCALVIEWER,u->localviewer,s->rs[142]!=0);
 int changed=gpu_upload_changed(uploaded,7,uploaded->light,s->light,sizeof s->light);
 changed|=gpu_upload_changed(uploaded,8,&uploaded->specular,&s->rs[29],4);
 if(changed){float lights[56*4]={0};for(int i=0;i<8;i++){float *l=lights+i*28;memcpy(l,s->light[i],26*4);uint32_t type;memcpy(&type,s->light[i],4);l[0]=(float)type;l[26]=s->rs[29]!=0;}glUniform4fv(u->lights,56,lights);}
 if(gpu_upload_changed(uploaded,9,uploaded->light_on,s->light_on,sizeof s->light_on)){float on[8];for(int i=0;i<8;i++)on[i]=s->light_on[i]!=0;glUniform4fv(u->enabled,2,on);}
 }
 size_t vertex_bytes=((size_t)max-first+1)*stride;
 size_t index_bytes=indices?(size_t)n*isize:0;
 /* DS_ONE_UPLOAD: one buffer bound to both targets and one glBufferData per
  * draw, instead of the legacy vertex upload + index upload (two driver
  * calls). Only the non-native path has anything to pack. */
 int combo=r->one_upload&&indices&&!(native&&(native->vertex||native->index));
 GLuint attribute_buffer;size_t attribute_offset_base=attribute_base;
 if(native&&native->vertex){bind_array_buffer(r,native->vertex);r->uploads.native_vertex_draws++;attribute_buffer=native->vertex;}
 else if(combo){
     size_t need=vertex_bytes+index_bytes;
     if(r->combo_capacity<need){void *p=realloc(r->combo_staging,need);if(!p)return reject("GPU one-upload staging allocation failed: %zu bytes",need);r->combo_staging=p;r->combo_capacity=need;}
     memcpy(r->combo_staging,(const char*)data+((size_t)base+first)*stride,vertex_bytes);
     memcpy((char*)r->combo_staging+vertex_bytes,uploaded_indices,index_bytes);
     bind_array_buffer(r,r->gpu_combo);
     glBufferData(GL_ARRAY_BUFFER,need,r->combo_staging,GL_STREAM_DRAW);
     r->gl_stats.buffer_data++;
     d8_renderer_note_buffer_upload(r,0,vertex_bytes);
     d8_renderer_note_buffer_upload(r,1,index_bytes);
     attribute_buffer=r->gpu_combo;attribute_offset_base=0;r->gpu_index_offset=vertex_bytes;
 }
 else{bind_array_buffer(r,r->gpu_vbo);glBufferData(GL_ARRAY_BUFFER,vertex_bytes,(const char*)data+((size_t)base+first)*stride,GL_STREAM_DRAW);d8_renderer_note_buffer_upload(r,0,vertex_bytes);attribute_buffer=r->gpu_vbo;}
 r->uploads.prefix_vertex_bytes+=((size_t)max+1)*stride;r->uploads.avoidable_prefix_bytes+=(size_t)min*stride;if(first)r->uploads.trimmed_draws++;
 attributes_begin(r);
 size_t off=pos==4?16:12;gpu_attribute(r,0,pos==4?4:3,GL_FLOAT,0,stride,attribute_offset_base,attribute_buffer);
 if(s->fvf&16){gpu_attribute(r,1,3,GL_FLOAT,0,stride,attribute_offset_base+off,attribute_buffer);off+=12;}else gpu_constant(r,1,0,0,1,0);if(s->fvf&32)off+=4;
 if(s->fvf&64){gpu_attribute(r,2,4,GL_UNSIGNED_BYTE,1,stride,attribute_offset_base+off,attribute_buffer);off+=4;}else gpu_constant(r,2,1,1,1,1);
 if(s->fvf&128){gpu_attribute(r,3,4,GL_UNSIGNED_BYTE,1,stride,attribute_offset_base+off,attribute_buffer);off+=4;}else gpu_constant(r,3,0,0,0,0);
 size_t offsets[8];unsigned dims[8];for(unsigned i=0;i<tc;i++){unsigned d=(s->fvf>>(16+2*i))&3;dims[i]=d==0?2:d==1?3:d==2?4:1;offsets[i]=off;off+=dims[i]*4;}
 for(int t=0;t<2;t++){if((unsigned)t>=stages){gpu_constant(r,4+t,0,0,0,0);continue;}unsigned ix=s->tss[t][11]&65535;if(ix>=8)return reject("GPU invalid texcoord index stage %d index %u",t,ix);if(ix<tc)gpu_attribute(r,4+t,dims[ix],GL_FLOAT,0,stride,attribute_offset_base+offsets[ix],attribute_buffer);else gpu_constant(r,4+t,0,0,0,0);}
 if(indices){
   if(combo){bind_element_buffer(r,r->gpu_combo);}
   else if(native&&native->index){bind_element_buffer(r,native->index);r->uploads.native_index_draws++;}
   else{bind_element_buffer(r,r->gpu_ibo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,index_bytes,uploaded_indices,GL_STREAM_DRAW);d8_renderer_note_buffer_upload(r,1,index_bytes);}
 }return 0;
}

/* Streaming upload for the CPU-reference draw path. Every CPU draw uses the
 * same Vertex layout, so the de-indexed array is appended to one persistent
 * buffer and drawn with glDrawArrays(first=...): attribute pointers stay valid
 * and the driver never reallocates storage per draw. When the ring is
 * exhausted the buffer is orphaned with glBufferData(NULL) — the standard
 * streaming pattern that keeps in-flight data alive. DS_RING_VB=0 restores the
 * old one-glBufferData-per-draw path for A/B. */
static GLint upload_cpu_vertices(D8Renderer *r,const void *data,size_t bytes,GLuint *buffer){
    if(r->ring_vb_disabled||bytes==0){
        bind_array_buffer(r,r->vbo);glBufferData(GL_ARRAY_BUFFER,bytes,data,GL_STREAM_DRAW);r->gl_stats.buffer_data++;
        *buffer=r->vbo;return 0;
    }
    unsigned idx=r->ring_index%3;GLuint b=r->ring_vbo[idx];
    if(!b){glGenBuffers(1,&r->ring_vbo[idx]);b=r->ring_vbo[idx];}
    /* Grow (and orphan) only when this frame's vertices exceed the buffer; the
     * regular path allocates once and never touches in-flight storage because
     * the buffer rotates every presented frame. */
    if(r->ring_cap[idx]<bytes||r->ring_used+bytes>r->ring_cap[idx]){
        size_t capacity=r->ring_used+bytes+(bytes>>1);
        if(capacity<(4u<<20))capacity=4u<<20;
        bind_array_buffer(r,b);glBufferData(GL_ARRAY_BUFFER,capacity,NULL,GL_STREAM_DRAW);r->gl_stats.buffer_data++;
        r->ring_cap[idx]=capacity;r->ring_used=0;
    }
    bind_array_buffer(r,b);
    glBufferSubData(GL_ARRAY_BUFFER,(GLintptr)r->ring_used,(GLsizeiptr)bytes,data);
    r->gl_stats.buffer_subdata++;
    GLint first=(GLint)(r->ring_used/sizeof(Vertex));
    r->ring_used+=bytes;*buffer=b;
    return first;
}

/* Build or find the specialised fragment program for one draw state. Each
 * variant is the generic shader with the stage constants folded; the emitted
 * arithmetic is identical, only the dynamic arg/op/stage selection is gone. */
static int get_frag_variant(D8Renderer *r,uint64_t key){
    if(!r->fast_shader_enabled||key==~(uint64_t)0)return -1;
    for(unsigned i=0;i<r->frag_count;i++)if(r->frag_variants[i].key==key)return (int)i;
    if(r->frag_count>=DS_FRAG_VARIANTS)return -1;
    char src[4096];GLuint fr=build_frag_shader_source(src,sizeof src,key);
    if(!fr)return -1;
    GLuint cpu=glCreateProgram();glAttachShader(cpu,r->shared_vs);glAttachShader(cpu,fr);
    static const char *attrs[]={"p","c","spec","uv0","uv1","fog"};
    for(int i=0;i<6;i++)glBindAttribLocation(cpu,(GLuint)i,attrs[i]);
    glLinkProgram(cpu);GLint ok;glGetProgramiv(cpu,GL_LINK_STATUS,&ok);
    GLuint gpu=0;
    if(ok&&r->gpu_path_enabled){gpu=make_gpu_program(fr,(int)((key>>2)&1u),0,1); /* sc stays written: cheap, and avoids any variant path reading an unwritten varying */if(!gpu)ok=0;}
    if(!ok){glDeleteProgram(cpu);glDeleteShader(fr);return -1;}
    unsigned idx=r->frag_count++;
    r->frag_variants[idx].key=key;r->frag_variants[idx].fragment=fr;
    r->frag_variants[idx].cpu_program=cpu;r->frag_variants[idx].gpu_program=gpu;
    cache_uniform_locations(cpu,&r->frag_variants[idx].uniforms);
    if(gpu)cache_uniform_locations(gpu,&r->frag_variants[idx].gpu_uniforms);
    {const char*e=setting("DS_DBG_LOC");if(e&&!strcmp(e,"1"))fprintf(stderr,"[dbg-build] idx=%u key=%016llx frag=%u cpu=%u gpu=%u cpu_normalmat=%d gpu_normalmat=%d gpu_fogcolor=%d\n",idx,(unsigned long long)key,fr,cpu,gpu,r->frag_variants[idx].uniforms.normalmat,r->frag_variants[idx].gpu_uniforms.normalmat,r->frag_variants[idx].gpu_uniforms.fogcolor);}
    /* cache_uniform_locations() binds the program to set sampler units; the
     * renderer's program cache no longer describes the real GL state. */
    glUseProgram(0);r->current_program=(GLuint)-1;
    return (int)idx;
}
/* Switch the active fragment program, carrying each program's uploaded uniform
 * cache with it (GL program uniform state survives switches; the cache only
 * mirrors it, so no re-upload storm occurs when specialised and generic draws
 * interleave). */
static void select_fragment_program(D8Renderer *r,int idx){
    if(idx==r->active_frag){
        /* The D3D8 facade saves/restores renderer.gpu_program around each draw
         * (diagnostic CPU forcing); re-apply the selected handles even when the
         * variant did not change, otherwise uniforms go to the wrong program. */
        if(idx>=0&&r->gpu_path_enabled)r->gpu_program=r->frag_variants[idx].gpu_program?r->frag_variants[idx].gpu_program:r->generic_gpu_program;
        else if(idx>=0)r->program=r->frag_variants[idx].cpu_program;
        else{r->program=r->generic_program;if(r->gpu_path_enabled)r->gpu_program=r->generic_gpu_program;}
        return;
    }
    r->active_frag=idx;
    if(idx>=0&&r->gpu_path_enabled){
        r->gpu_program=r->frag_variants[idx].gpu_program?r->frag_variants[idx].gpu_program:r->generic_gpu_program;
        r->gpu_uniforms=r->frag_variants[idx].gpu_uniforms;
        r->program=r->generic_program;r->uniforms=r->generic_uniforms;r->small_uniforms=r->generic_small;
    }else if(idx>=0){
        r->program=r->frag_variants[idx].cpu_program;r->uniforms=r->frag_variants[idx].uniforms;
        r->small_uniforms=r->frag_small[idx];
        r->gpu_program=r->generic_gpu_program;r->gpu_uniforms=r->generic_gpu_uniforms;
    }else{
        r->program=r->generic_program;r->uniforms=r->generic_uniforms;r->small_uniforms=r->generic_small;
        if(r->gpu_path_enabled){r->gpu_program=r->generic_gpu_program;r->gpu_uniforms=r->generic_gpu_uniforms;}
    }
}
void d8_renderer_set_output_scale(D8Renderer *r,float scale){r->output_scale=(scale>0.2f&&scale<=1.0f)?scale:1.0f;}
void d8_renderer_perf_dump(D8Renderer *r){
    if(!r->perf_enabled)return;
    fprintf(stderr,"[perf-probe] draws=%u gpu=%u index_draws=%u lit_draws=%u index_scan_ms=%.1f\n",
        r->perf_draws,r->perf_gpu_draws,r->perf_index_draws,r->perf_lit_draws,(double)r->perf_index_ns/1e6);
    r->perf_draws=r->perf_gpu_draws=r->perf_index_draws=r->perf_lit_draws=0;r->perf_index_ns=0;
}
int d8_renderer_draw_native(D8Renderer *r,const D8State *s,unsigned th,unsigned primitive,unsigned count,const void *data,size_t stride,unsigned vcount,const void *indices,unsigned isize,unsigned base,const GLuint textures[2],const D8NativeBuffers *native){
    draw_error[0]=0;unsigned n=d8_primitive_vertices(primitive,count),need=d8_fvf_stride(s->fvf);if(!r->program||!n||n>300000||!need||stride<need||!data)return reject("invalid draw input: program=%u vertices=%u need_stride=%u stride=%zu fvf=%#x data=%p",r->program,n,need,stride,s->fvf,data);
    if(s->rs[8]!=3||s->rs[151]||s->rs[167]||s->rs[152]){return reject("unsupported RS: FILLMODE=%u VERTEXBLEND=%u INDEXEDVERTEXBLEND=%u CLIPPLANEENABLE=%#x",s->rs[8],s->rs[151],s->rs[167],s->rs[152]);}
    /* COLOROP=DISABLE terminates the cascade. Higher stages and their
     * coordinate generators/operations are dormant, regardless of stale state. */
    unsigned stages=0;
    for(unsigned t=0;t<2&&s->tss[t][1]!=1;t++,stages++){
        /* D3D also terminates when COLORARG1 selects an absent texture.
         * Preserve the incoming diffuse/current alpha instead of substituting
         * an opaque white texture (e.g. the scope's 25%-alpha orange tint). */
        if(!textures[t]&&(s->tss[t][2]&15)==2)break;
        if((s->tss[t][11]&0xffff0000u)||(s->tss[t][1]>16&&s->tss[t][1]!=24)||(s->tss[t][4]>16&&s->tss[t][4]!=24))return reject("unsupported TSS stage=%u COLOROP=%u ALPHAOP=%u TEXCOORDINDEX=%#x",t,s->tss[t][1],s->tss[t][4],s->tss[t][11]);
        unsigned ix=s->tss[t][11]&65535,flags=s->tss[t][24],pc=flags&15;
        if(ix>=8)return reject("texture coordinate index invalid: stage=%u index=%u",t,ix);
        if((flags&256)&&(pc<1||pc>4))return reject("invalid projected texture coordinate count: stage=%u flags=%#x",t,flags);
    }
    if(r->shader_stats){
        uint64_t key=(uint64_t)(stages&3)|((uint64_t)(textures[0]!=0)<<2)|((uint64_t)(textures[1]!=0)<<3)
            |((uint64_t)(s->rs[29]!=0)<<4)|((uint64_t)(s->rs[28]!=0)<<5)
            |((uint64_t)(s->rs[15]!=0)<<6)|((uint64_t)((s->rs[15]?s->rs[25]:8)&15)<<7)
            |((uint64_t)(s->tss[0][1]&31)<<11)|((uint64_t)(s->tss[0][4]&31)<<16)
            |((uint64_t)(s->tss[0][2]&31)<<21)|((uint64_t)(s->tss[0][3]&31)<<26)
            |((uint64_t)(s->tss[0][5]&31)<<31)|((uint64_t)(s->tss[0][6]&31)<<36)
            |((uint64_t)(s->tss[1][1]&31)<<41)|((uint64_t)(s->tss[1][2]&31)<<46);
        unsigned i;for(i=0;i<r->shader_hist_n;i++)if(r->shader_hist[i].key==key){r->shader_hist[i].count++;break;}
        if(i==r->shader_hist_n&&r->shader_hist_n<64){r->shader_hist[i].key=key;r->shader_hist[i].count=1;r->shader_hist_n++;}
    }
    select_fragment_program(r,get_frag_variant(r,frag_state_key(s,textures,stages,r->alpha_elide)));
    if(r->active_frag>=0)r->fast_draws++;
    int gpu=r->gpu_program!=0;
 if(r->perf_enabled){r->perf_draws++;if(gpu)r->perf_gpu_draws++;if((s->fvf&14)!=4&&s->rs[137]&&(s->fvf&16))r->perf_lit_draws++;}
    /* GLES2 lacks core 32-bit element indices. CPU deindexing remains valid. */
    if(gpu&&indices&&isize==4){const char *version=(const char*)glGetString(GL_VERSION),*ext=(const char*)glGetString(GL_EXTENSIONS);if(!strstr(version,"OpenGL ES 3")&&(!ext||!strstr(ext,"GL_OES_element_index_uint"))){static int warned;if(!warned++){fprintf(stderr,"[d3d8] GPU uint32 indices unsupported; using CPU reference\n");}gpu=0;}}
    if(gpu&&gpu_prepare(r,s,n,data,stride,vcount,indices,isize,base,stages,native)){
        /* The GPU vertex path is a fast path, not the definition of
         * correctness. An unsupported state/format combination must still be
         * drawn by the CPU reference path instead of dropping the primitive.
         * gpu_prepare validates before its GL mutations, and the CPU path below
         * re-binds its own program, attributes, raster state and textures. */
        static unsigned gpu_fallbacks;
        if(!gpu_fallbacks++)fprintf(stderr,"[d3d8] GPU vertex path declined a draw (%s); CPU reference path used\n",d8_renderer_error());
        gpu=0;
    }
    GLuint program=gpu?r->gpu_program:r->program;const D8UniformLocations *u=gpu?&r->gpu_uniforms:&r->uniforms;D8SmallUniformCache *small=gpu?gpu_small_cache(r):&r->small_uniforms;
    size_t bytes=(size_t)n*sizeof(Vertex);if(!gpu&&r->capacity<bytes){void *p=realloc(r->vertices,bytes);if(!p)return reject("vertex staging allocation failed: bytes=%zu",bytes);r->vertices=p;r->capacity=bytes;}Vertex *dst=r->vertices;
    unsigned pos=s->fvf&14,tc=(s->fvf>>8)&15;
    D8CpuLighting lighting;
    if(!gpu&&pos!=4&&s->rs[137]&&(s->fvf&16))cpu_lighting_prepare(s,&lighting);
    for(unsigned i=0;!gpu&&i<n;i++){
        unsigned index=i;if(indices){if(isize==2){uint16_t x;memcpy(&x,(const char*)indices+i*2,2);index=x;}else if(isize==4)memcpy(&index,(const char*)indices+i*4,4);else return reject("invalid index size: %u",isize);}
        if(index>=vcount||index>UINT32_MAX-base)return reject("index outside vertex range: i=%u index=%u count=%u base=%u",i,index,vcount,base);index+=base;const uint8_t *src=(const uint8_t*)data+(size_t)index*stride;
        Vertex *v=dst+i;float p[4]={0,0,0,1},normal[3]={0,0,1};memcpy(p,src,pos==4?16:12);src+=pos==4?16:12;
        if(s->fvf&16){memcpy(normal,src,12);src+=12;}if(s->fvf&32)src+=4;
        uint32_t c=UINT32_MAX,spec=0;if(s->fvf&64){memcpy(&c,src,4);src+=4;}if(s->fvf&128){memcpy(&spec,src,4);src+=4;}argb(c,v->c);argb(spec,v->spec);
        float coords[8][4]={{0}};for(unsigned t=0;t<tc;t++){unsigned dim=(s->fvf>>(16+2*t))&3;dim=dim==0?2:dim==1?3:dim==2?4:1;coords[t][3]=1;memcpy(coords[t],src,dim*4);src+=dim*4;}
        for(int t=0;t<2;t++){if((unsigned)t>=stages){v->uv[t][0]=v->uv[t][1]=0;continue;}unsigned ix=s->tss[t][11]&0xffff;if(ix>=8)return reject("texture coordinate index invalid: stage=%d index=%u",t,ix);float uv[4];memcpy(uv,coords[ix],16);if(s->tss[t][24])vector(uv,uv,s->transform[16+t]);unsigned projected_count=s->tss[t][24]&15;if((s->tss[t][24]&256)&&(projected_count<1||projected_count>4))return reject("invalid projected texture coordinate count: stage=%d flags=%#x",t,s->tss[t][24]);float q=(s->tss[t][24]&256)?uv[projected_count-1]:1;if(q==0)q=1;v->uv[t][0]=uv[0]/q;v->uv[t][1]=uv[1]/q;if(s->texture_flipy[t])v->uv[t][1]=1-v->uv[t][1];}
        if(pos==4){if(!isfinite(p[3]))return reject("invalid RHW: vertex=%u position=(%.9g,%.9g,%.9g) rhw=%.9g",i,p[0],p[1],p[2],p[3]);if(p[3]==0)p[3]=1;float x=(p[0]+.5f-s->viewport[0])/s->viewport[2],y=(p[1]+.5f-s->viewport[1])/s->viewport[3];v->p[0]=(2*x-1)/p[3];v->p[1]=(1-2*y)/p[3];float zmin=asfloat(s->viewport[4]),zmax=asfloat(s->viewport[5]);float z=zmax!=zmin?(p[2]-zmin)/(zmax-zmin):0;v->p[2]=(2*z-1)/p[3];v->p[3]=1/p[3];v->fog=s->rs[35]?p[2]:v->spec[3];}
        else{float world[4],view[4];vector(world,p,s->transform[256]);vector(view,world,s->transform[2]);vector(v->p,view,s->transform[3]);v->p[2]=2*v->p[2]-v->p[3];v->fog=fabsf(view[2]);if(s->rs[137]&&(s->fvf&16))light_vertex(s,&lighting,world,normal,v->c,v->spec);}
    }
    /* gpu_prepare already made this program current before its uniforms. */
    if(!gpu)use_program(r,program);raster(r,s,th);
    for(int t=0;t<2;t++){GLuint texture=(unsigned)t<stages?textures[t]:0;if(texture||!native||!native->default_framebuffer){bind_texture_unit(r,(unsigned)t,texture);if(texture)texture_parameters(r,texture,s->tss[t]);}
        small1i(small,SU_HAS0+t,u->has[t],texture!=0);small4i(small,SU_COP0+t,u->cop[t],(unsigned)t<stages?s->tss[t][1]:1,s->tss[t][2],s->tss[t][3],0);small4i(small,SU_AOP0+t,u->aop[t],s->tss[t][4],s->tss[t][5],s->tss[t][6],0);}
    float fc[4];argb(s->rs[34],fc);if(u->factor>=0){float factor[4];argb(s->rs[60],factor);small4fv(small,SU_FACTOR,u->factor,factor);}small4fv(small,SU_FOGCOLOR,u->fogcolor,fc);
    small1i(small,SU_AFUNC,u->afunc,s->rs[15]?s->rs[25]:8);small1f(small,SU_AREF,u->aref,(s->rs[24]&255)/255.f);
    unsigned fog=s->rs[35]?s->rs[35]:pos==4?4:s->rs[140];small1i(small,SU_FOGMODE,u->fogmode,s->rs[28]?(GLint)fog:0);small3f(small,SU_FOGPARAM,u->fogparam,asfloat(s->rs[36]),asfloat(s->rs[37]),asfloat(s->rs[38]));small1i(small,SU_SPECULAR,u->specular,s->rs[29]!=0);
    GLint array_first=0;GLuint cpu_buffer=r->vbo;
    if(!gpu){array_first=upload_cpu_vertices(r,dst,bytes,&cpu_buffer);r->uploads.vertex_bytes+=bytes;r->uploads.prefix_vertex_bytes+=bytes;r->uploads.vertex_uploads++;size_t offsets[]={offsetof(Vertex,p),offsetof(Vertex,c),offsetof(Vertex,spec),offsetof(Vertex,uv[0]),offsetof(Vertex,uv[1]),offsetof(Vertex,fog)};int sizes[]={4,4,4,2,2,1};attributes_begin(r);for(int i=0;i<6;i++)gpu_attribute(r,i,sizes[i],GL_FLOAT,GL_FALSE,sizeof(Vertex),offsets[i],cpu_buffer);
    }
    static const GLenum modes[]={0,GL_POINTS,GL_LINES,GL_LINE_STRIP,GL_TRIANGLES,GL_TRIANGLE_STRIP,GL_TRIANGLE_FAN};
    if(gpu&&indices){
        GLenum it=isize==2?GL_UNSIGNED_SHORT:GL_UNSIGNED_INT;void *ip=(void*)(native&&native->index?native->index_offset:r->gpu_index_offset);
        if(r->range_draw_enabled&&r->range_draw_proc&&!render_thread_active()){r->range_draw_proc(modes[primitive],r->range_start,r->range_end,n,it,ip,r->range_base);r->gl_stats.range_draws++;}
        else glDrawElements(modes[primitive],n,it,ip);
    } else glDrawArrays(modes[primitive],array_first,n);r->gl_stats.draws++;GLenum e=0;if(--r->draw_error_countdown==0){r->draw_error_countdown=r->draw_error_interval;r->gl_stats.error_checks++;e=glGetError();}if(e){r->gl_stats.error_detections++;d8_renderer_invalidate_raster(r);d8_renderer_invalidate_attributes(r);d8_renderer_invalidate_texture_parameters(r);d8_renderer_invalidate_bindings(r);r->small_uniforms.valid=0;gpu_small_cache(r)->valid=0;gpu_large_cache(r)->valid=0;return reject("GL draw error %#x",e);}return 0;
}

void d8_renderer_invalidate_depth(D8Renderer *r, int offscreen){
    if(!r->invalidate_depth||!r->invalidate_proc||render_thread_active())return;
    /* Named FBOs use attachment enums; GL_DEPTH/STENCIL are default-only. */
    GLenum att[2]={offscreen?GL_DEPTH_ATTACHMENT:GL_DEPTH,offscreen?GL_STENCIL_ATTACHMENT:GL_STENCIL};
    r->invalidate_proc(GL_FRAMEBUFFER,2,att);
}

int d8_renderer_draw(D8Renderer *r,const D8State *s,unsigned th,unsigned primitive,unsigned count,const void *data,size_t stride,unsigned vcount,const void *indices,unsigned isize,unsigned base,const GLuint textures[2]){
    return d8_renderer_draw_native(r,s,th,primitive,count,data,stride,vcount,indices,isize,base,textures,NULL);
}
