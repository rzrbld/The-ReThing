#!/usr/bin/env python3
"""Stage an instrumented runtime with streaming tests but NO draw suppression."""
import re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/streaming-shadow-20261103';out.mkdir(exist_ok=True)
subprocess.run(['python3','scripts/prepare_queue_bounds_capture_20261103.py'],cwd=root,check=True)
# Reuse exact tested arithmetic; rename internal symbols to avoid facade globals.
coarse=(root/'probe/visibility/coarse_occlusion_bench.c').read_text();coarse=coarse[coarse.index('typedef struct'):coarse.index('static unsigned run(')]
pipeline=(root/'probe/visibility/occlusion_pipeline_bench.c').read_text();pipeline=pipeline[pipeline.index('typedef struct'):pipeline.index('static int by_draw(')]
stream=(root/'probe/visibility/streaming_occlusion_bench.c').read_text();start=stream.index('  const Mesh *a=m+k;');end=stream.index('\n }\n return rejected;',start);body=stream[start:end].replace('  const Mesh *a=m+k;','')
body=re.sub(r'  if\(verify&&hidden.*?exit\(2\);\}\n','',body)
body=body.replace('  rejected+=hidden;if(hidden)continue;','  if(hidden)return 1;')
body=body.replace('b.score<=rank[nr-1]))continue;','b.score<=rank[nr-1]))return 0;')
body=body.replace('int valid=1;for(unsigned i=0;i<a->n;i++)if(!project(a->xyz+i*3,a->a+6,scratch+i*3)){valid=0;break;}if(!valid)continue;', 'int valid=1;for(unsigned i=0;i<a->n;i++){double xyz[3];if(!shadow_vertex(i,xyz)||!project(xyz,a->a+6,scratch+i*3)){valid=0;break;}}if(!valid)return 0;')
core=coarse+pipeline+'''static unsigned segment,nr,scans;static double rank[4];
static void stream_reset(void){segment=nr=scans=0;for(unsigned i=0;i<64*48;i++)depth[i]=INFINITY;}
static int stream_step(const Mesh *a,double *scratch){
'''+body+'\n return 0;\n}\n'
for name in ['Triangle','Query','Mesh','Box','Ranked','depth','clampi','raster','project','box','clip_triangle','segment','nr','scans','rank','stream_reset','stream_step']:
 core=re.sub(r'\b'+name+r'\b','shadow_'+name,core)
(out/'core.h').write_text(core)
(out/'identity.h').write_text((root/'build/queue-bounds-capture-20261103/identity.h').read_text())
