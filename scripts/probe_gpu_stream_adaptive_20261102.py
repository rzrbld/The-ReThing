#!/usr/bin/env python3
"""Separate GPU streaming variant: grow chunks on rollover, capped at 4 MiB."""
import subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
subprocess.run(['python3',str(root/'scripts/probe_gpu_stream_20261102.py')],check=True)
base=root/'build/gpu-stream-20261102';out=root/'build/gpu-stream-adaptive-20261102';out.mkdir(exist_ok=True)
s=(base/'d3d8_renderer.c').read_text()
marker='if(r->gpu_stream_orphan[kind]||bytes>capacity||offset>capacity-bytes){'
assert s.count(marker)==1
s=s.replace(marker,marker+'''
  if(!r->gpu_stream_orphan[kind]&&bytes<=capacity&&offset>capacity-bytes&&capacity<r->gpu_stream_limit){
   capacity*=2;if(capacity>r->gpu_stream_limit)capacity=r->gpu_stream_limit;
  }''',1)
(out/'d3d8_renderer.c').write_text(s)
(out/'d3d8_renderer.h').write_bytes((base/'d3d8_renderer.h').read_bytes())
s=(base/'selftest.c').read_text().replace('rep<12','rep<200').replace('rep%4==0','rep%160==0')
s=s.replace('    if(!g_gpu.gl_stats.buffer_subdata)overall=0;', '    printf("stream_capacity vertex=%zu index=%zu\\n",g_gpu.gpu_stream_capacity[0],g_gpu.gpu_stream_capacity[1]);\n    if(g_gpu.gpu_stream_capacity[0]<=65536||!g_gpu.gl_stats.buffer_subdata)overall=0;')
(out/'selftest.c').write_text(s)
print('Staged separate adaptive GPU streaming candidate')
