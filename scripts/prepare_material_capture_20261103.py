#!/usr/bin/env python3
"""Stage original packet/material snapshot plus clear epochs; no draw suppression."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
subprocess.run(['python3','scripts/prepare_queue_bounds_capture_20261103.py'],cwd=root,check=True)
src=root/'build/queue-bounds-capture-20261103';out=root/'build/material-capture-20261103';out.mkdir(exist_ok=True)
s=(src/'identity.h').read_text().replace('extern uint32_t ti_cell,ti_node,ti_target,ti_render;', 'extern uint32_t ti_packet;\nvoid thing_material_snapshot(uint32_t out[2],unsigned char material[32],unsigned char packet[148]);\nextern uint32_t ti_cell,ti_node,ti_target,ti_render;')
s=s.replace('uint32_t ti_cell,ti_node,ti_target,ti_render;', 'uint32_t ti_cell,ti_node,ti_target,ti_render;',1)
needle='uint32_t ti_cell,ti_node,ti_target,ti_render;\nstatic struct'
assert needle in s
s=s.replace(needle,'''uint32_t ti_cell,ti_node,ti_target,ti_render;
uint32_t ti_packet;
void thing_material_snapshot(uint32_t out[2],unsigned char material[32],unsigned char packet[148]){
 memset(out,0,8);memset(material,0,32);memset(packet,0,148);
 if(!ti_packet)return;
 out[0]=ti_packet;out[1]=MEM32(ti_packet);memcpy(packet,(const void*)ADDR(ti_packet),148);
 if(out[1])memcpy(material,(const void*)ADDR(out[1]),32);
}
static struct''')
(out/'identity.h').write_text(s)
s=(src/'capture.h').read_text();s=s.replace('static FILE *gc_bounds;', '''static FILE *gc_material;static uint32_t gc_depth_epoch;
extern void thing_material_snapshot(uint32_t out[2],unsigned char material[32],unsigned char packet[148]);
static void gc_depth_clear(unsigned flags){if(flags&2)gc_depth_epoch++;}
static FILE *gc_bounds;''')
s=s.replace('static void gc_end(void){','static void gc_end(void){if(gc_material){fclose(gc_material);gc_material=NULL;}')
needle='gc_draws=0;char q[4096];';assert needle in s
s=s.replace(needle,'''gc_draws=0;gc_depth_epoch=0;char mp[4096];if(snprintf(mp,sizeof mp,"%s/material-%u.bin",root,frame)<(int)sizeof mp)gc_material=fopen(mp,"wb");char q[4096];''')
needle=' fwrite(h,4,15,gc_file);';assert needle in s
s=s.replace(needle,''' if(gc_material){uint32_t ids[2];unsigned char mat[32],packet[148];thing_material_snapshot(ids,mat,packet);uint32_t mh[]={gc_draws,ids[0],ids[1],gc_depth_epoch};fwrite(mh,4,4,gc_material);fwrite(mat,1,32,gc_material);fwrite(packet,1,148,gc_material);}
'''+needle)
(out/'capture.h').write_text(s)
