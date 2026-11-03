#!/usr/bin/env python3
"""Conservative LLVM IR experiment: guest arena disjoint from CPU-state globals.

Only candidate* functions receive guest annotations. A guest pointer must be an
inttoptr of (loaded g_mem_base + zero-extended i32 or bounded constant). Unknown
provenance stays unannotated. This is a prototype, not a production build pass.
"""
import json,re,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]) if len(sys.argv)>1 else root/'build/guest-alias-20261102'
source=(out/'raw.ll').read_text()
last=max(map(int,re.findall(r'!(\d+)',source)),default=0)+1
domain,guest,state,guest_list,state_list=range(last,last+5)
native={'g_'+n for n in 'eax ecx edx ebx esp esi edi ebp cur_func fp_top st st_i64 fpu_cw mem_base flag_a flag_b flag_cf flag_k'.split()}
base=set();offset=set();address=set();gp=set();np=set();name='';counts={};lines=[]
def bounded(value):
 return value in offset or bool(re.fullmatch(r'\d+',value) and int(value)<=0xffffffff)
for line in source.splitlines():
 m=re.match(r'define .*?@([\w.]+)\(',line)
 if m:
  name=m[1];base=set();offset=set();address=set();gp=set();np=set();counts[name]={'guest':0,'state':0}
 m=re.match(r'\s*(%[\w.]+) = (.*)',line)
 if m:
  dst,op=m.groups()
  if re.match(r'load i64, ptr @g_mem_base\b',op):base.add(dst)
  if re.match(r'zext i32 .* to i64$',op):offset.add(dst)
  add=re.fullmatch(r'add(?: nuw| nsw)* i64 ([%\w.]+), ([%\w.]+)',op)
  if add:
   a,b=add.groups()
   if (a in base and bounded(b))or(b in base and bounded(a)):address.add(dst)
  cast=re.fullmatch(r'inttoptr i64 (%[\w.]+) to ptr',op)
  if cast and cast[1] in address:gp.add(dst)
  gep=re.match(r'getelementptr(?: inbounds)? .*?, ptr ([@%][\w.]+),',op)
  if gep and (gep[1] in np or gep[1].removeprefix('@') in native):np.add(dst)
 access=re.match(r'\s*(?:%[\w.]+ = )?(?:load .*?,|store .*?,) ptr ([@%][\w.]+), align \d+',line)
 if access and name:
  ptr=access[1];kind=None
  if ptr in np or ptr.removeprefix('@') in native:kind='state'
  elif name.startswith('candidate') and ptr in gp:kind='guest'
  if kind:
   assert '!alias.scope' not in line and '!noalias' not in line
   scope,other=(guest_list,state_list)if kind=='guest'else(state_list,guest_list)
   line+=f', !alias.scope !{scope}, !noalias !{other}';counts[name][kind]+=1
 lines.append(line)
 if line=='}':name=''
lines += [f'!{domain} = distinct !{{!{domain}}}',f'!{guest} = distinct !{{!{guest}, !{domain}, !"guest arena"}}',f'!{state} = distinct !{{!{state}, !{domain}, !"native CPU state"}}',f'!{guest_list} = !{{!{guest}}}',f'!{state_list} = !{{!{state}}}']
expected=int(sys.argv[2]) if len(sys.argv)>2 else 5
assert all(counts['candidate'+str(i)]['guest']>0 for i in range(expected))
(out/'annotated.ll').write_text('\n'.join(lines)+'\n');(out/'annotations.json').write_text(json.dumps(counts,indent=2)+'\n')
print({k:v for k,v in counts.items()if k.startswith('candidate')})
