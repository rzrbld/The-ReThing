#!/usr/bin/env python3
"""Fail-closed hot-function local x87 top experiment; does not alter game files."""
import json,re,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def transform(body):
 assert not re.search(r'\bg_(?:fp_top|st|st_i64)\b|\b_fp_top\b',re.sub(r'/\*.*?\*/','',body,flags=re.S))
 assert body.count('RECOMP_REGS_LOCALS')==1
 result=body.replace('RECOMP_REGS_LOCALS','RECOMP_REGS_LOCALS\n int _thing_fp_top=g_fp_top;')
 for old,new in [('fp_push(', 'thing_fp_push(&_thing_fp_top,'),('fp_pop()', 'thing_fp_pop(&_thing_fp_top)'),('FP_ST(', 'THING_FP_ST('),('FP_SI(', 'THING_FP_SI('),('fp_xch(', 'THING_FP_XCH('),('RECOMP_CALL(', 'THING_FP_CALL('),('RECOMP_ICALL(', 'THING_FP_ICALL(')]:result=result.replace(old,new)
 result=result.replace('void recomp_thething_', '__attribute__((optimize("fp-contract=off"))) void recomp_thething_',1)
 return result.replace('RECOMP_REGS_OUT()','(g_fp_top=_thing_fp_top,RECOMP_REGS_OUT())')
ALLOWED=set('ADDR MEM8 MEM16 MEM32 MEM64 MEMSET32 PUSH32 POP32_VAL FP_ST FP_SI fp_push fp_pop fp_xch fp_to_int FPU_CMP sqrt fabs RECOMP_ENTER RECOMP_CALL RECOMP_ICALL RECOMP_FLAGS_IN RECOMP_FLAGS_OUT RECOMP_REGS_OUT RECOMP_PF recomp_flags_pack recomp_flags_sbb recomp_cond_cf recomp_rep_movs FK_NARROW LO8 HI8 LO16 SET_LO8 SET_HI8 SET_LO16 if while'.split())
ALLOWED.update('CMP_'+s for s in 'AE A B BE EQ NE G GE L LE S NS O NO'.split())
ALLOWED.update('TEST_'+s for s in 'Z NZ S NS G GE L LE O NO'.split())
def main():
 out=ROOT/'build/fp-local-top-20261102';out.mkdir(exist_ok=True)
 data=json.loads((ROOT/'docs/evidence/device-skin-simd-20261102/native-gameplay.json').read_text())
 counts={n.rsplit('!',1)[-1]:c for n,c in data['functions']};patches=[];rejected=[]
 for path in (ROOT/'build/translated-local-regs/thething').glob('functions_*.c'):
  for m in re.finditer(r'void (recomp_thething_[0-9A-F]+)\(void\) \{.*?(?=\nvoid recomp_|\Z)',path.read_text(),re.S):
   name=m[1];count=counts.get(name,0)
   if count<10:continue
   clean=re.sub(r'/\*.*?\*/','',m[0],flags=re.S)
   if 'FP_ST(' not in clean:continue
   calls=set(re.findall(r'\b(\w+)\s*\(',clean))-{name}
   unknown=calls-ALLOWED
   if unknown or re.search(r'\bg_(?:fp_top|st|st_i64)\b|\b_fp_top\b',clean):
    rejected.append(dict(name=name,samples=count,unknown=sorted(unknown)));continue
   # All explicit exits must publish through the generated register exit macro.

   if re.search(r'\breturn\b',re.sub(r'RECOMP_REGS_OUT\(\);\s*return;', '', clean)) or len(re.findall(r'\b'+name+r'\s*\(',clean))!=1:
    rejected.append(dict(name=name,samples=count,unknown=['noncanonical return/writeback count']));continue
   patches.append(dict(address=name[-8:],samples=count,unit=path.name,original=m[0],candidate=transform(m[0]),source_sha256=hashlib.sha256(m[0].encode()).hexdigest()))
 (out/'patches.json').write_text(json.dumps(patches,indent=2)+'\n')
 report=dict(selected=[{k:p[k]for k in ('address','samples','unit')}for p in patches],samples=sum(p['samples']for p in patches),total_samples=data['total_interval_samples'],rejected=rejected)
 (out/'audit.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
