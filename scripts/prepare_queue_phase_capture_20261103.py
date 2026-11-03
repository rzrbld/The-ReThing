#!/usr/bin/env python3
"""Observe queue/draw ordering and material lifetime; never suppress work."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
subprocess.run(['python3','scripts/prepare_material_capture_20261103.py'],cwd=root,check=True)
src=root/'build/material-capture-20261103';out=root/'build/queue-phase-capture-20261103';out.mkdir(exist_ok=True)
s=(src/'identity.h').read_text()
s=s.replace('extern uint32_t ti_packet;', '''void ti_phase_queue(uint32_t key,uint32_t material,uint32_t flags);
void ti_phase_draw(void);
void ti_phase_snapshot(uint32_t out[6],unsigned char material[32]);
extern uint32_t ti_packet;''')
s=s.replace('uint32_t qb_valid,qb_flags;', 'uint32_t qb_valid,qb_flags;uint32_t phase_valid,phase_event,phase_draws,phase_material,phase_flags;unsigned char phase_bytes[32];')
s=s.replace('static unsigned ti_overflow;', 'static unsigned ti_overflow;static uint32_t phase_event,phase_draws;')
marker='void ti_present(void){';assert s.count(marker)==1
s=s.replace(marker,'''void ti_phase_queue(uint32_t key,uint32_t material,uint32_t flags){
 uint32_t event=++phase_event;unsigned h=key?ti_slot(key):16384;if(h==16384)return;
 if(!ti_map[h].key)ti_map[h].key=key;
 if(ti_map[h].phase_valid){ti_map[h].phase_valid=2;return;}
 ti_map[h].phase_valid=1;ti_map[h].phase_event=event;ti_map[h].phase_draws=phase_draws;
 ti_map[h].phase_material=material;ti_map[h].phase_flags=flags;
 if(material)memcpy(ti_map[h].phase_bytes,(const void*)ADDR(material),32);
}
void ti_phase_draw(void){++phase_event;++phase_draws;}
void ti_phase_snapshot(uint32_t out[6],unsigned char material[32]){
 memset(out,0,24);memset(material,0,32);unsigned h=ti_render?ti_slot(ti_render):16384;
 out[3]=phase_event;
 if(h==16384||!ti_map[h].key)return;
 out[0]=ti_map[h].phase_valid;out[1]=ti_map[h].phase_event;out[2]=ti_map[h].phase_draws;
 out[4]=ti_map[h].phase_material;out[5]=ti_map[h].phase_flags;
 memcpy(material,ti_map[h].phase_bytes,32);
}
'''+marker+'phase_event=phase_draws=0;')
(out/'identity.h').write_text(s)
s=(src/'capture.h').read_text()
s=s.replace('static FILE *gc_material;', 'static FILE *gc_phase;\nextern void ti_phase_snapshot(uint32_t out[6],unsigned char material[32]);\nstatic FILE *gc_material;')
s=s.replace('static void gc_end(void){','static void gc_end(void){if(gc_phase){fclose(gc_phase);gc_phase=NULL;}')
needle='gc_draws=0;gc_depth_epoch=0;';assert needle in s
s=s.replace(needle,needle+'char pp[4096];if(snprintf(pp,sizeof pp,"%s/queue-phase-%u.bin",root,frame)<(int)sizeof pp)gc_phase=fopen(pp,"wb");')
needle=' fwrite(h,4,15,gc_file);';assert needle in s
s=s.replace(needle,''' if(gc_phase){uint32_t phase[6];unsigned char mat[32];ti_phase_snapshot(phase,mat);uint32_t ph[]={gc_draws,identity[0]};fwrite(ph,4,2,gc_phase);fwrite(phase,4,6,gc_phase);fwrite(mat,1,32,gc_phase);}
'''+needle)
(out/'capture.h').write_text(s)
