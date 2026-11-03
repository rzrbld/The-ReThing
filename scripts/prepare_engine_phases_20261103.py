#!/usr/bin/env python3
from pathlib import Path
import sys,re,hashlib
root=Path(__file__).resolve().parents[1]
if sys.argv[1]=='facade':
 out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
 s=(root/'build/decal-option-20261102/d3d8.c').read_text()
 old='static void present(void){';assert s.count(old)==1;s=s.replace(old,'extern void thing_engine_phase_present(unsigned frame);\n'+old)
 old='frames++;if(draw_batch';assert s.count(old)==1;s=s.replace(old,'frames++;thing_engine_phase_present(frames);if(draw_batch')
 old='static void profile_exit(int code){';assert s.count(old)==1;s=s.replace(old,'extern void thing_engine_phase_finish(void);\n'+old+'thing_engine_phase_finish();')
 (out/'d3d8.c').write_text(s)
else:
 p=Path(sys.argv[2]);s=p.read_text();assert 'engine_phases/timing.h' not in s
 implementation='#define THING_ENGINE_PHASE_IMPLEMENTATION\n' if p.stem=='functions_0165' else ''
 s=s.replace('#include "functions.h"','#include "functions.h"\n'+implementation+'#include "../../../probe/engine_phases/timing.h"\n/* phase_header_sha256='+hashlib.sha256((root/'probe/engine_phases/timing.h').read_bytes()).hexdigest()+' */',1)
 sites=['00688ABA','00688ACE','00688ADF','00688AF5','0068AC85','0068AD21','0068AD32','0068AE2B','0068AE86','0068AEA4','0068AAD8','0068AC90','0068AD47','0068ADAA','0068AE91','0068AEB9','0068AF15']
 sites=list(enumerate(sites)) if p.stem=='functions_0165' else ([(17,'004777A8')] if p.stem=='functions_0036' else [(18,'006A74F7'),(19,'006A05C6'),(20,'006A05CE'),(21,'006A069F')])
 for i,site in sites:
  matches=list(re.finditer(r'^    ((?:RECOMP|THING_FP)_(I?CALL))\((.*?)\); /\* 0x'+site+r':.*?\*/$',s,re.M));assert len(matches)==1,(site,len(matches));m=matches[0];macro,kind,arg=m.group(1,2,3)
  target=arg if kind=='ICALL' else '0x'+arg.rsplit('_',1)[-1]+'u'
  call=f'{macro}(_ep_target)' if kind=='ICALL' else f'{macro}({arg})'
  new=f'    {{ uint32_t _ep_target={target};uint64_t _ep_start=ep_begin({i},_ep_target);{call};ep_end({i},_ep_start); }} /* phase callsite0x{site} */'
  s=s[:m.start()]+new+s[m.end():]
 p.write_text(s)
