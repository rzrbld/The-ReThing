/* Actual HLE entry points/state blocks and draw facade; renderer boundary is a
 * capture stub. No GL context, image equivalence or performance claim here. */
#define d8_renderer_draw_native test_renderer_draw_native
#include THING_TEST_D3D8_SOURCE
#undef d8_renderer_draw_native
#include <assert.h>
#include <stdarg.h>
ptrdiff_t g_mem_base;
uint32_t g_eax,g_ecx,g_edx,g_esp,g_ebx,g_ebp,g_esi,g_edi,g_cur_func;
const char *g_cur_import;
static unsigned char guest[65536];
static D8LightBank reference;
static uint64_t digest=1469598103934665603ull;
static unsigned draws,checks;
static void hash(const void *p,size_t n){const unsigned char *b=p;while(n--){digest^=*b++;digest*=1099511628211ull;}}
void hle_fatal(const char *fmt,...){va_list a;va_start(a,fmt);vfprintf(stderr,fmt,a);va_end(a);abort();}
uint32_t hle_va_bad(const void *p){(void)p;abort();}
void *hle_alloc(uint32_t n){(void)n;abort();}
void hle_free(void *p){(void)p;abort();}
void hle_com_free(uint32_t p){(void)p;abort();}
void hle_set_pump_hook(void(*fn)(void)){(void)fn;abort();}
void (*hle_on_exit)(int code);
void hle_exit(int code){(void)code;abort();}
void hle_input_cursor(int x,int y){(void)x;(void)y;abort();}
void hle_set_screen_size(int x,int y){(void)x;(void)y;abort();}
uint32_t hle_com_vtable(const win32hle_shim *m){(void)m;abort();}
void ds_input_pump(void){abort();}
void ds_fps_overlay_draw(SDL_Window *w){(void)w;abort();}
void ds_fps_overlay_presented(void){abort();}
void ds_fps_overlay_close(void){abort();}
void d8_video_close(void){abort();}
static void check_bank(void){assert(!memcmp(&reference,&light_bank,sizeof reference));}
static void check_packed(const D8State *s){
 unsigned k=0;
 for(unsigned id=0;id<DS_D8_LOGICAL_LIGHTS;id++)if(reference.enabled[id]){
  assert(k<8&&s->light_on[k]==1&&active_light_ids[k]==id);
  assert(!memcmp(s->light[k],reference.light[id],sizeof s->light[k]));k++;
 }
 for(;k<8;k++){assert(!s->light_on[k]&&active_light_ids[k]==UINT32_MAX);for(unsigned j=0;j<26;j++)assert(!memcmp(&s->light[k][j],&(float){0},4));}
 hash(s,sizeof *s);checks++;
}
int test_renderer_draw_native(D8Renderer *r,const D8State *s,unsigned th,unsigned primitive,unsigned count,const void *data,size_t stride,unsigned vcount,const void *indices,unsigned isize,unsigned base,const GLuint textures[2],const D8NativeBuffers *native){
 (void)r;(void)th;(void)primitive;(void)count;(void)data;(void)stride;(void)vcount;(void)indices;(void)isize;(void)base;(void)textures;(void)native;check_packed(s);draws++;return 0;
}
static uint32_t invoke(void(*fn)(void),unsigned argc,uint32_t a,uint32_t b,uint32_t c){
 g_esp=1024;MEM32(1024)=0xfeed1234;MEM32(1028)=0;MEM32(1032)=a;MEM32(1036)=b;MEM32(1040)=c;
 fn();assert(g_esp==1024+4+4*argc);hash(&g_eax,4);return g_eax;
}
static void enable(unsigned id,int value){int expected=ds_d8_lights_enable(&reference,id,value);assert(invoke(light_enable,3,id,value,0)==(expected?INVALID:OK));check_bank();}
static void define(unsigned id,unsigned variant,int valid){float l[26];for(unsigned j=0;j<26;j++)l[j]=(j+variant)*.03125f;unsigned type=valid?1+variant%3:9;memcpy(l,&type,4);memcpy(guest+2048,l,sizeof l);int expected=ds_d8_lights_set(&reference,id,l);assert(invoke(set_light,3,id,2048,0)==(expected?INVALID:OK));check_bank();}
static void getters(unsigned id){
 float l[26];int r=ds_d8_lights_get(&reference,id,l);memset(guest+4096,0x9a,sizeof l);assert(invoke(get_light,3,id,4096,0)==(r?INVALID:OK));if(!r){assert(!memcmp(l,guest+4096,sizeof l));hash(l,sizeof l);}
 uint32_t on=0; r=ds_d8_lights_get_enable(&reference,id,&on);assert(invoke(light_enabled,3,id,4096,0)==(r?INVALID:OK));if(!r){assert(MEM32(4096)==on);hash(&on,4);}
}
static void consume_draw(void){float v[15]={0};assert(!draw(4,1,v,20,3,NULL,0,0,NULL,0,NULL,0));check_bank();}
static void consume_capture(void){StateBlock b={0};block_capture(&b);assert(!memcmp(&b.lights,&reference,sizeof reference));check_packed(&b.value);}
static unsigned seed=0x832f;
static unsigned next(void){seed=1664525u*seed+1013904223u;return seed;}
int main(void){
 g_mem_base=(ptrdiff_t)guest;state.fvf=0x102;state.tss[0][1]=1;state.viewport[2]=640;state.viewport[3]=480;
 render_target=512;MEM32(520)=1;objects[1].self=512;objects[1].kind=SURFACE;objects[1].height=480;objects[1].width=640;objects[1].backbuffer=1;
 ds_d8_lights_init(&light_bank);ds_d8_lights_init(&reference);pack_lights();
 /* Undefined default creation, including disable on undefined and getters. */
 getters(511);enable(511,0);getters(511);enable(511,1);define(511,3,1);getters(511);consume_draw();
 /* Reordering enabled slots, SetLight while dirty, unchanged enables. */
 enable(7,1);enable(3,1);define(511,9,1);define(7,11,1);enable(7,1);getters(7);consume_capture();consume_draw();
 /* Invalid definition/ID/capacity failures must not mutate authoritative state. */
 assert(invoke(set_light,3,7,0,0)==INVALID);check_bank();define(7,0,0);enable(512,1);define(512,0,1);getters(512);for(unsigned i=20;i<26;i++)enable(i,1);consume_draw();
 /* Masked apply exceeding eight active lights must be transactional. */
 StateBlock invalid_block={0};ds_d8_lights_init(&invalid_block.lights);assert(!ds_d8_lights_enable(&invalid_block.lights,111,1));invalid_block.light_mask.enable[111]=1;invalid_block.light_mask.definition[111]=1;
 assert(block_apply(&invalid_block)==-1);check_bank();consume_draw();
 for(unsigned i=0;i<512;i++)if(reference.enabled[i])enable(i,0);consume_draw();
 /* Partial recorded definition, restore after EndStateBlock, apply definition
  * while preserving the CURRENT enable state. */
 enable(11,1);define(11,2,1);D8LightBank before=reference;
 assert(invoke(begin_block,1,0,0,0)==OK);define(11,5,1);D8LightBank recorded=reference;
 assert(invoke(end_block,2,4096,0,0)==OK);unsigned partial=MEM32(4096);reference=before;check_bank();consume_draw();
 enable(11,0);assert(!ds_d8_lights_set(&reference,11,recorded.light[11]));assert(invoke(apply_block,2,partial,0,0)==OK);check_bank();getters(11);consume_draw();
 /* Enable-only recorded change must preserve a later current definition. */
 before=reference;assert(invoke(begin_block,1,0,0,0)==OK);enable(11,1);assert(invoke(end_block,2,4096,0,0)==OK);unsigned enable_only=MEM32(4096);reference=before;check_bank();
 define(11,19,1);assert(!ds_d8_lights_enable(&reference,11,1));assert(invoke(apply_block,2,enable_only,0,0)==OK);check_bank();consume_capture();consume_draw();
 /* Complete snapshot and later capture: packed state and logical bank agree. */
 assert(invoke(create_block,3,1,4096,0)==OK);unsigned full=MEM32(4096);D8LightBank full_reference=reference;
 enable(11,0);enable(501,1);define(501,27,1);assert(invoke(apply_block,2,full,0,0)==OK);reference=full_reference;check_bank();consume_draw();
 enable(501,1);define(501,31,1);assert(invoke(capture_block,2,full,0,0)==OK);full_reference=reference;enable(501,0);assert(invoke(apply_block,2,full,0,0)==OK);reference=full_reference;check_bank();consume_draw();
 assert(invoke(delete_block,2,partial,0,0)==OK);assert(invoke(delete_block,2,enable_only,0,0)==OK);assert(invoke(delete_block,2,full,0,0)==OK);
 /* Long random sequence with multiple transitions between draw snapshots. */
 for(unsigned i=0;i<20000;i++){
  unsigned id=(next()>>16)%24,op=(next()>>16)%4;
  if(op<2)enable(id,op);else if(op==2)define(id,next()%100,1);else getters(id);
  if(i%13==0)consume_draw();if(i%137==0)consume_capture();
 }
 consume_draw();
 diagnostic_frame=tmpfile();assert(diagnostic_frame);enable(511,0);diagnostic_draw(4,1);fclose(diagnostic_frame);diagnostic_frame=NULL;check_packed(&state);
#ifdef THING_LIGHT_PACK_COUNTERS
 fprintf(stderr,"lazy opportunities=%llu actual_packs=%llu draws=%u checks=%u\n",(unsigned long long)light_pack_opportunities,(unsigned long long)light_pack_actual,draws,checks);
 assert(light_pack_actual<light_pack_opportunities);
#endif
 printf("PASS HLE getters, defaults, dirty SetLight, invalid/capacity, partial/full state blocks, draw snapshots; digest=%016llx draws=%u checks=%u\n",(unsigned long long)digest,draws,checks);
}
