#!/usr/bin/env python3
"""Generate an isolated lazy HLE light packing candidate; never edit runtime."""
import argparse,hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--input',default='runtime/d3d8.c')
parser.add_argument('--output-dir',default='build/light-pack-lazy-20261102')
args=parser.parse_args()
p=Path(args.input);p=p if p.is_absolute() else root/p
out=Path(args.output_dir);out=out if out.is_absolute() else root/out
p=p.resolve(strict=True);out=out.resolve()
assert p not in (out/'d3d8.baseline.c',out/'d3d8.candidate.c'), 'input must not be an output artifact'
s=p.read_text();before=hashlib.sha256(p.read_bytes()).hexdigest();out.mkdir(parents=True,exist_ok=True)
(out/'d3d8.baseline.c').write_text(s)
def change(old,new):
 global s
 assert s.count(old)==1,old[:90]
 s=s.replace(old,new)
change('static void pack_lights(void){ds_d8_lights_pack(&light_bank,state.light,state.light_on,active_light_ids);}', '''/* Packed renderer slots are derived state; the logical bank is authoritative. */
static int light_pack_dirty;
#ifdef THING_LIGHT_PACK_COUNTERS
static uint64_t light_pack_opportunities,light_pack_actual;
#define LIGHT_PACK_COUNT(x) (++(x))
#else
#define LIGHT_PACK_COUNT(x) ((void)0)
#endif
static void pack_lights(void){ds_d8_lights_pack(&light_bank,state.light,state.light_on,active_light_ids);light_pack_dirty=0;LIGHT_PACK_COUNT(light_pack_actual);}
static void invalidate_lights(void){light_pack_dirty=1;LIGHT_PACK_COUNT(light_pack_opportunities);}
static void sync_lights(void){if(light_pack_dirty)pack_lights();}''')
change('if(light_bank.enabled[id])for(unsigned i=0;i<DS_D8_ACTIVE_LIGHTS;i++)','if(!light_pack_dirty&&light_bank.enabled[id])for(unsigned i=0;i<DS_D8_ACTIVE_LIGHTS;i++)')
change('if(was_enabled!=light_bank.enabled[id])pack_lights();','if(was_enabled!=light_bank.enabled[id])invalidate_lights();')
change('static void block_capture(StateBlock *b){\n    b->lights=light_bank;','static void block_capture(StateBlock *b){\n    sync_lights();\n    b->lights=light_bank;')
change('    pack_lights();return 0;\n}', '    invalidate_lights();return 0;\n}')
change('static void diagnostic_draw(unsigned primitive,unsigned count){\n    if(!diagnostic_frame)return;', 'static void diagnostic_draw(unsigned primitive,unsigned count){\n    if(!diagnostic_frame)return;\n    sync_lights();')
change('Object *vb,size_t voff,Object *ib,size_t ioff){\n', 'Object *vb,size_t voff,Object *ib,size_t ioff){\n    sync_lights();\n')
change('    d8_renderer_report_gl_stats(&renderer);', '    d8_renderer_report_gl_stats(&renderer);\n#ifdef THING_LIGHT_PACK_COUNTERS\n    fprintf(stderr,"[light-pack] eager_opportunities=%llu actual_packs=%llu\\n",(unsigned long long)light_pack_opportunities,(unsigned long long)light_pack_actual);\n#endif')
# Preserve the existing direct-slot SetLight path when packed order is valid.
s=s.replace('Full packing remains necessary when the set changes.', 'When the enabled set is dirty, its eventual pack reads the updated bank.')
(out/'d3d8.candidate.c').write_text(s)
assert hashlib.sha256(p.read_bytes()).hexdigest()==before
(out/'source.json').write_text(json.dumps({'input':str(p),'baseline_sha256':before,'candidate_sha256':hashlib.sha256(s.encode()).hexdigest(),'input_unchanged':True,'runtime_unchanged':True},indent=2)+'\n')
print(out/'d3d8.candidate.c')
