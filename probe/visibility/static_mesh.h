#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
uint32_t thing_probe_node,thing_probe_mesh,thing_probe_lod,thing_probe_lod_count;
static void static_mesh_note(uint32_t node,uint32_t record,uint32_t detail){
 uint32_t b=MEM32(node+0xE4),e=MEM32(node+0xE8);
 uint32_t n=(e>=b)?(e-b)/24:0,slot=(b&&record>=b&&record<e)?(record-b)/24:UINT32_MAX;
 thing_probe_node=node;thing_probe_mesh=MEM32(record);thing_probe_lod=slot;thing_probe_lod_count=n;
 static struct {uint32_t node,slot;} seen[4096];static unsigned count;
 for(unsigned i=0;i<count;i++)if(seen[i].node==node&&seen[i].slot==slot)return;
 if(count==4096)return;seen[count].node=node;seen[count++].slot=slot;
 int saved=errno;char path[4096];const char *root=getenv("THING_ROOT");
 if(root&&snprintf(path,sizeof path,"%s/static-lod.csv",root)<(int)sizeof path){FILE*f=fopen(path,count==1?"w":"a");if(f){
 if(count==1)fputs("node,mesh,lod,count,detail_bits\n",f);
 fprintf(f,"0x%x,0x%x,%u,%u,0x%x\n",node,thing_probe_mesh,slot,n,detail);fclose(f);}}errno=saved;
}
