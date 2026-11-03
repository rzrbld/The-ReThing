#!/usr/bin/env python3
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
s=(root/'third_party/pcrecomp/runtime/win32hle/host_lite.c').read_text()
def patch(old,new):
 global s
 assert s.count(old)==1,old;s=s.replace(old,new,1)
patch('static int g_shim_n;','static int g_shim_n;\n'+(root/'probe/facade/counters.inc').read_text())
# Registration occurs before any guest entry; includes lazily registered COM methods.
patch('    int added = 0;', '    facade_init();\n    int added = 0;')
patch('    g_shim[i].fn();','    if(thing_facade_enabled)facade_calls[i]++;\n    g_shim[i].fn();')
patch('recomp_func_t recomp_lookup_import(uint32_t va) {','recomp_func_t recomp_lookup_import(uint32_t va) {\n    if(thing_facade_enabled)facade_lookups++;')
patch('        g_pending_shim = i;','        if(thing_facade_enabled)facade_hits++;\n        g_pending_shim = i;')
(out/'host_lite.c').write_text(s)
s=(root/'build/decal-option-20261102/d3d8.c').read_text()
patch('static void profile_exit(int code){','extern void thing_facade_finish(void);\nstatic void profile_exit(int code){thing_facade_finish();')
patch('static void present(void){','extern int thing_facade_enabled;\nextern void thing_facade_present(unsigned frame);\nextern void thing_facade_same(unsigned id);\nstatic void present(void){')
patch('frames++;if(draw_batch','frames++;thing_facade_present(frames);if(draw_batch')
for old,new in [
 ('state.rs[A32(1)]=A32(2);','if(thing_facade_enabled&&state.rs[A32(1)]==A32(2))thing_facade_same(0);state.rs[A32(1)]=A32(2);'),
 ('state.tss[t][s]=A32(3);','if(thing_facade_enabled&&state.tss[t][s]==A32(3))thing_facade_same(1);state.tss[t][s]=A32(3);'),
 ('uint32_t old=bound_texture[stage];','if(thing_facade_enabled&&bound_texture[stage]==A32(2))thing_facade_same(2);uint32_t old=bound_texture[stage];'),
 ('uint32_t old=stream_obj;stream_obj=A32(2);','if(thing_facade_enabled&&stream_obj==A32(2)&&stream_stride==A32(3))thing_facade_same(3);uint32_t old=stream_obj;stream_obj=A32(2);'),
 ('uint32_t old=index_obj;index_obj=A32(1);','if(thing_facade_enabled&&index_obj==A32(1)&&base_vertex==A32(2))thing_facade_same(4);uint32_t old=index_obj;index_obj=A32(1);'),
 ('state.fvf=A32(1);','if(thing_facade_enabled&&state.fvf==A32(1))thing_facade_same(5);state.fvf=A32(1);')]:patch(old,new)
(out/'d3d8.c').write_text(s)
