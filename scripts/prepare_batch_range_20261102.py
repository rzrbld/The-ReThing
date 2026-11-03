#!/usr/bin/env python3
"""Stage a trusted range handoff solely for validated, queue-owned indices."""
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/batch-range-20261102';out.mkdir(exist_ok=True)
def patch(s,old,new):
 assert s.count(old)==1,old
 return s.replace(old,new)
for staged in (False,True):
 source=root/('build/render-thread-src/runtime' if staged else 'runtime')
 dest=out/('staged' if staged else 'runtime');dest.mkdir(exist_ok=True)
 h=(source/'d3d8_renderer.h').read_text()
 h=patch(h,'    int default_framebuffer;','''    int default_framebuffer;
    /* Internal contract: only an owned batch that has validated/rebased every
     * index may supply this range. Data must remain unchanged through draw.
     * Not a hint supplied by guest code or mutable vertex/index buffers. */
    unsigned owned_index_count,owned_index_min,owned_index_max;''')
 (dest/'d3d8_renderer.h').write_text(h)
 c=(source/'d3d8_renderer.c').read_text()
 c=patch(c,' int scan_valid=0;\n if(r->index_range_enabled&&pos!=4){',''' int scan_valid=0;
 if(native&&native->owned_index_count&&indices&&isize==4&&pos!=4&&
    native->owned_index_count==n&&native->owned_index_min<=native->owned_index_max&&
    native->owned_index_max<vcount&&native->owned_index_max<=UINT32_MAX-base){
     min=native->owned_index_min;max=native->owned_index_max;scan_valid=1;
#ifdef THING_BATCH_RANGE_TEST
     extern unsigned thing_batch_range_test_hits;thing_batch_range_test_hits++;
#endif
 }
 if(!scan_valid&&r->index_range_enabled&&pos!=4){''')
 (dest/'d3d8_renderer.c').write_text(c)
 c=(source/'d3d8.c').read_text()
 c=patch(c,'(void)unused;D8NativeBuffers native={.default_framebuffer=d->default_framebuffer};batch_emits++;','''(void)unused;D8NativeBuffers native={.default_framebuffer=d->default_framebuffer,
        .owned_index_count=d->count*3,.owned_index_min=0,.owned_index_max=d->vcount-1};batch_emits++;''')
 (dest/'d3d8.c').write_text(c)
s=(root/'probe/batch/adjacent_batch_selftest.c').read_text()
s='unsigned thing_batch_range_test_hits;\n'+s
s=s.replace('validation();puts(', 'validation();assert(thing_batch_range_test_hits>0);printf("validated range draws=%u\\n",thing_batch_range_test_hits);puts(')
s=patch(s,'(void)opaque;D8NativeBuffers native={.default_framebuffer=d->default_framebuffer};emitted++;','''D8NativeBuffers native={.default_framebuffer=d->default_framebuffer};emitted++;
 if(opaque==&renderer){
  assert(d->index_size==4&&d->base==0&&d->count&&d->vcount);
  unsigned lo=UINT32_MAX,hi=0;
  for(unsigned i=0;i<d->count*3;i++){uint32_t ix;memcpy(&ix,(const char*)d->indices+4*i,4);assert(ix<d->vcount);if(ix<lo)lo=ix;if(ix>hi)hi=ix;}
  assert(lo==0&&hi==d->vcount-1);
  native.owned_index_count=d->count*3;native.owned_index_min=lo;native.owned_index_max=hi;
 }''')
s=s.replace('emit,NULL)', 'emit,&renderer)')
(out/'selftest.c').write_text(s)
# Local header copy keeps all test translation units on the same ABI.
(out/'runtime/adjacent_batch.h').write_bytes((root/'runtime/adjacent_batch.h').read_bytes())
(out/'runtime/adjacent_batch.c').write_bytes((root/'runtime/adjacent_batch.c').read_bytes())
print('Staged owned-batch range candidate; runtime and default artifacts unchanged')
