#!/usr/bin/env python3
"""Stage diagnostic additions; original geometry bounds, world and queue-time view/projection."""
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/queue-bounds-capture-20261103';out.mkdir(exist_ok=True)
s=(root/'probe/visibility/terrain_identity.h').read_text()
s=s.replace('void ti_tag(uint32_t key);','void ti_tag(uint32_t key);\nvoid ti_capture_bounds(uint32_t key,uint32_t world);\nvoid thing_geometry_record_bounds(float out[54],uint32_t *valid,uint32_t *flags);\nvoid thing_queue_view_projection(float out[32]);')
s=s.replace('uint32_t key,cell,node,target,ambiguous;','uint32_t key,cell,node,target,ambiguous;float qb[54];uint32_t qb_valid,qb_flags;')
marker='void ti_present(void){'
extra='''void ti_capture_bounds(uint32_t key,uint32_t world){
 if(!key||!world)return;unsigned h=ti_slot(key);if(h==16384)return;
 uint32_t mesh=MEM32(key+0x14),index=MEM32(key+0x18);if(!mesh||index>100000)return;
 uint32_t subsets=MEM32(mesh+0x3c);if(!subsets||index>(UINT32_MAX-subsets-0x34u)/0x34u)return;
 float qb[54];memcpy(qb,(const void*)ADDR(subsets+index*0x34u+0x1c),24);memcpy(qb+6,(const void*)ADDR(world),64);thing_queue_view_projection(qb+22);
 uint32_t flags=MEM8(key+0x10);
 if(!ti_map[h].key)ti_map[h].key=key;
 if(ti_map[h].qb_valid&&(memcmp(qb,ti_map[h].qb,sizeof qb)||ti_map[h].qb_flags!=flags)){ti_map[h].qb_valid=2;return;}
 if(ti_map[h].qb_valid!=2){memcpy(ti_map[h].qb,qb,sizeof qb);ti_map[h].qb_flags=flags;ti_map[h].qb_valid=1;}
}
void thing_geometry_record_bounds(float out[54],uint32_t *valid,uint32_t *flags){
 memset(out,0,216);*valid=*flags=0;unsigned h=ti_render?ti_slot(ti_render):16384;
 if(h<16384&&ti_map[h].key){memcpy(out,ti_map[h].qb,216);*valid=ti_map[h].qb_valid;*flags=ti_map[h].qb_flags;}
}
'''
assert marker in s;s=s.replace(marker,extra+marker);(out/'identity.h').write_text(s)
s=(root/'probe/visibility/geometry_capture.h').read_text()
s=s.replace('static FILE *gc_file;', 'static FILE *gc_bounds;\nvoid thing_queue_view_projection(float out[32]){memcpy(out,state.transform[2],64);memcpy(out+16,state.transform[3],64);}\nextern void thing_geometry_record_bounds(float out[54],uint32_t *valid,uint32_t *flags);\nstatic FILE *gc_file;')
s=s.replace('static void gc_end(void){','static void gc_end(void){if(gc_bounds){fclose(gc_bounds);gc_bounds=NULL;}')
s=s.replace('gc_draws=0;if(gc_file)', 'gc_draws=0;char q[4096];if(snprintf(q,sizeof q,"%s/queue-bounds-%u.bin",root,frame)<(int)sizeof q)gc_bounds=fopen(q,"wb");if(gc_file)')
s=s.replace('fwrite(h,4,15,gc_file);','if(gc_bounds){float qb[54];uint32_t valid,flags;thing_geometry_record_bounds(qb,&valid,&flags);uint32_t qh[]={gc_draws,identity[0],valid,flags};fwrite(qh,4,4,gc_bounds);fwrite(qb,4,54,gc_bounds);}\n fwrite(h,4,15,gc_file);')
(out/'capture.h').write_text(s)
