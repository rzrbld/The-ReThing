/* Experimental observer only: NEVER suppresses a draw. */
static const unsigned char *shadow_vertices,*shadow_indices;
static unsigned shadow_stride,shadow_vcount,shadow_isize,shadow_base;
static size_t shadow_vertex_bytes,shadow_index_bytes;
static int shadow_vertex(unsigned i,double out[3]){
 uint32_t ix=i;
 if(shadow_indices){
  if((size_t)i*shadow_isize+shadow_isize>shadow_index_bytes)return 0;
  ix=0;memcpy(&ix,shadow_indices+(size_t)i*shadow_isize,shadow_isize);
 }
 if(ix>=shadow_vcount||ix>UINT32_MAX-shadow_base)return 0;
 size_t off=(size_t)(ix+shadow_base)*shadow_stride;
 if(off>shadow_vertex_bytes||shadow_vertex_bytes-off<12)return 0;
 float p[3];memcpy(p,shadow_vertices+off,12);
 for(unsigned j=0;j<3;j++){out[j]=p[j];if(!isfinite(out[j]))return 0;}return 1;
}
#include "../../build/streaming-shadow-20261103/core.h"
extern void thing_geometry_identity(uint32_t out[5]);
extern void thing_geometry_record_bounds(float out[54],uint32_t *valid,uint32_t *flags);
void thing_queue_view_projection(float out[32]){memcpy(out,state.transform[2],64);memcpy(out+16,state.transform[3],64);}
static unsigned shadow_epoch=1,shadow_previous_epoch,shadow_draws,shadow_frames;
static uint32_t shadow_targets[2],shadow_viewport[6];static float shadow_camera[32];
static uint64_t shadow_tested,shadow_hidden,shadow_calls,shadow_ticks;
static int shadow_enabled=-1;
static void shadow_clear(unsigned flags){if(flags&2)shadow_epoch++;}
static void shadow_frame(void){
 shadow_stream_reset();shadow_epoch++;shadow_draws=0;
 if(++shadow_frames%300==0&&shadow_enabled>0)fprintf(stderr,"[streaming-shadow] frames=%u calls=%llu tested=%llu proposed=%llu wall_ms=%.3f no_suppression=1\n",shadow_frames,(unsigned long long)shadow_calls,(unsigned long long)shadow_tested,(unsigned long long)shadow_hidden,shadow_ticks*1000.0/SDL_GetPerformanceFrequency());
}
static void shadow_draw(unsigned primitive,unsigned count,const void *vertices,unsigned stride,unsigned vcount,const void *indices,unsigned isize,unsigned base,Object *vb,size_t voff,Object *ib,size_t ioff){
 if(shadow_enabled<0){const char *v=getenv("THING_STREAMING_SHADOW");shadow_enabled=v&&!strcmp(v,"1");}
 if(!shadow_enabled)return;
 uint64_t begin=SDL_GetPerformanceCounter();shadow_calls++;shadow_draws++;
 if(shadow_previous_epoch!=shadow_epoch||shadow_targets[0]!=render_target||shadow_targets[1]!=depth_target||memcmp(shadow_viewport,state.viewport,24)||memcmp(shadow_camera,state.transform[2],64)||memcmp(shadow_camera+16,state.transform[3],64)){
  shadow_epoch++;shadow_previous_epoch=shadow_epoch;shadow_targets[0]=render_target;shadow_targets[1]=depth_target;
  memcpy(shadow_viewport,state.viewport,24);memcpy(shadow_camera,state.transform[2],64);memcpy(shadow_camera+16,state.transform[3],64);
 }
 uint32_t identity[5],valid,flag;float bounds[54];thing_geometry_identity(identity);
 if(identity[3]!=0x794310||identity[4])goto done;
 thing_geometry_record_bounds(bounds,&valid,&flag);if(valid!=1)goto done;
 for(unsigned i=0;i<54;i++)if(!isfinite(bounds[i]))goto done;
 for(unsigned i=0;i<3;i++)if(bounds[i]>bounds[i+3])goto done;
 if(memcmp(bounds+6,state.transform[256],64)||memcmp(bounds+22,state.transform[2],64)||memcmp(bounds+38,state.transform[3],64))goto done;
 unsigned n=d8_primitive_vertices(primitive,count);
 shadow_Mesh mesh={0};mesh.id=shadow_draws;mesh.shadow_segment=shadow_epoch;mesh.cull=state.rs[22];mesh.bits=1;
 for(unsigned i=0;i<54;i++)mesh.a[i]=bounds[i];
 int depth_ok=depth_target&&state.rs[7]==1&&(state.rs[23]==2||state.rs[23]==4)&&!state.rs[52]&&!state.rs[47]&&state.viewport[4]==0&&state.viewport[5]==0x3f800000;
 if(depth_ok&&flag==0)mesh.bits|=4;
 if(depth_ok&&state.rs[14]==1&&!state.rs[15]&&!state.rs[27]&&primitive==4&&(state.fvf&14)==2&&n&&n<=16384&&n%3==0&&stride>=12&&vertices&&vb&&!vb->buffer_locked&&voff<=vb->length&&(!indices||(ib&&!ib->buffer_locked&&ioff<=ib->length&&(isize==2||isize==4)))){mesh.bits|=2;mesh.n=n;}
 shadow_vertices=vertices;shadow_indices=indices;shadow_stride=stride;shadow_vcount=vcount;shadow_isize=isize;shadow_base=base;
 shadow_vertex_bytes=vb&&voff<=vb->length?vb->length-voff:0;shadow_index_bytes=ib&&ioff<=ib->length?ib->length-ioff:0;
 static double scratch[16384*3];shadow_tested++;shadow_hidden+=shadow_stream_step(&mesh,scratch)!=0;
 done:shadow_ticks+=SDL_GetPerformanceCounter()-begin;
}
