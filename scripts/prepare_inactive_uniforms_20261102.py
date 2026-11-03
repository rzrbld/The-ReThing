#!/usr/bin/env python3
"""Generate isolated inactive-uniform early-outs; preserve source by hash."""
import argparse,hashlib,json,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
a=argparse.ArgumentParser(description=__doc__);a.add_argument('--input',default='runtime/d3d8_renderer.c');a.add_argument('--output-dir',default='build/inactive-uniforms-20261102');args=a.parse_args()
p=Path(args.input);p=(p if p.is_absolute() else root/p).resolve(strict=True)
out=Path(args.output_dir);out=(out if out.is_absolute() else root/out).resolve();out.mkdir(parents=True,exist_ok=True)
assert p not in [out/'d3d8_renderer.baseline.c',out/'d3d8_renderer.candidate.c']
s=p.read_text();original=s;before=hashlib.sha256(p.read_bytes()).hexdigest()
for name in ['small1i','small4i','small1f','small2f','small3f','small4f','small4fv']:
 matches=list(re.finditer(r'static void '+name+r'\([^\n]*?\)\{',s));assert len(matches)==1,name
 at=matches[0].end();s=s[:at]+'if(loc<0)return;'+s[at:]
old='float fc[4],factor[4];argb(s->rs[34],fc);argb(s->rs[60],factor);small4fv(small,SU_FACTOR,u->factor,factor);small4fv(small,SU_FOGCOLOR,u->fogcolor,fc);'
new='float fc[4];argb(s->rs[34],fc);if(u->factor>=0){float factor[4];argb(s->rs[60],factor);small4fv(small,SU_FACTOR,u->factor,factor);}small4fv(small,SU_FOGCOLOR,u->fogcolor,fc);'
assert s.count(old)==1;s=s.replace(old,new)
(out/'d3d8_renderer.baseline.c').write_text(original);(out/'d3d8_renderer.candidate.c').write_text(s)
assert hashlib.sha256(p.read_bytes()).hexdigest()==before
(out/'source.json').write_text(json.dumps({'input':str(p),'baseline_sha256':before,'candidate_sha256':hashlib.sha256(s.encode()).hexdigest(),'input_unchanged':True,'scope':'seven small uniform helpers; factor conversion for active location only'},indent=2)+'\n')
print(out/'d3d8_renderer.candidate.c')
