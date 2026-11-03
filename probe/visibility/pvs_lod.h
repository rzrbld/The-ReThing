#include <stdint.h>
extern uint64_t pv_calls,pv_missing,pv_pass,pv_reject;
extern uint64_t pv_gate,pv_disabled,pv_special,pv_no_cell,pv_expected;
extern void pv_lod(uint32_t node,uint32_t selected,uint32_t detail);
#ifdef THING_DEFINE_PVS_COUNTS
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
uint64_t pv_calls,pv_missing,pv_pass,pv_reject;
uint64_t pv_gate,pv_disabled,pv_special,pv_no_cell,pv_expected;
static uint64_t pv_record_fallback,pv_bitset;
static uint64_t lod_counts[16];
void pv_lod(uint32_t node,uint32_t selected,uint32_t detail){
 lod_counts[selected<15?selected:15]++;
 static unsigned emitted;static struct {uint32_t node,selected;} seen[4096];
 for(unsigned i=0;i<emitted;i++)if(seen[i].node==node&&seen[i].selected==selected)return;
 if(emitted==4096)return;
 seen[emitted].node=node;seen[emitted++].selected=selected;
 int saved=errno;const char *root=getenv("THING_ROOT");char path[4096];
 if(root&&snprintf(path,sizeof path,"%s/lod-selections.csv",root)<(int)sizeof path){
 FILE *f=fopen(path,emitted==1?"w":"a");if(f){
 if(emitted==1)fputs("node,selected,detail_bits,begin,end,threshold_bits\n",f);
 uint32_t begin=MEM32(node+0xE4),end=MEM32(node+0xE8);
 fprintf(f,"0x%x,%u,0x%x,0x%x,0x%x,0x%x\n",node,selected,detail,begin,end,selected<1024?MEM32(begin+selected*0x58+0x14):0);fclose(f);}}
 errno=saved;
}
static void pv_detail(uint32_t obj,uint32_t cell,uint32_t target) {
 if(cell==UINT32_MAX)return;
 uint32_t record=MEM32(MEM32(obj+0xC)+cell*8);
 if((record&0xC0000000u)==0xC0000000u)pv_bitset++;else pv_record_fallback++;
 static struct {uint32_t obj,cell,record;} seen[1024];static unsigned count;
 for(unsigned i=0;i<count;i++)if(seen[i].obj==obj&&seen[i].cell==cell&&seen[i].record==record)return;
 if(count==1024)return;
 seen[count].obj=obj;seen[count].cell=cell;seen[count++].record=record;
 int saved=errno;const char *root=getenv("THING_ROOT");char path[4096];
 if(root&&snprintf(path,sizeof path,"%s/pvs-cells.csv",root)<(int)sizeof path){
 FILE *f=fopen(path,count==1?"w":"a");if(f){
 if(count==1)fputs("query,pvs_object,source_cell,source_record,target_list,record_next_word\n",f);
 fprintf(f,"%llu,0x%x,%u,0x%x,0x%x,0x%x\n",(unsigned long long)pv_calls,obj,cell,record,target,MEM32(MEM32(obj+0xC)+cell*8+4));fclose(f);}}
 errno=saved;
}
void ds_gl_cost_present(void){
 static unsigned frame;static FILE *file;
 if(++frame%300)return;
 int saved=errno;
 if(!file){const char *root=getenv("THING_ROOT");char path[4096];
  if(!root||snprintf(path,sizeof path,"%s/pvs-counts.csv",root)>=(int)sizeof path){errno=saved;return;}
  file=fopen(path,"w");if(!file){errno=saved;return;}
  fputs("frame,calls,missing_camera,visible,invisible,gate,disabled,special,no_cell,expected_query,effects_disable_culling,source_record_fallback,bitset_queries\n",file);
 }
 fprintf(file,"%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%u,%llu,%llu\n",frame,
 (unsigned long long)pv_calls,(unsigned long long)pv_missing,(unsigned long long)pv_pass,(unsigned long long)pv_reject,
 (unsigned long long)pv_gate,(unsigned long long)pv_disabled,(unsigned long long)pv_special,(unsigned long long)pv_no_cell,
 (unsigned long long)pv_expected,(unsigned)MEM8(0x926A49),(unsigned long long)pv_record_fallback,(unsigned long long)pv_bitset);
 fflush(file);errno=saved;
}
#endif
