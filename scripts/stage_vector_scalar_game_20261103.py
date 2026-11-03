#!/usr/bin/env python3
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1];p=root/'build/translated-local-regs/thething/functions_0005.c';s=p.read_text()
for i,address in enumerate(('00414210','00414230')):
 pattern=r'void recomp_thething_'+address+r'\(void\) \{.*?(?=\nvoid recomp_|\Z)'
 m=re.search(pattern,s,re.S);assert m
 fallback=m.group().replace('void recomp_thething_'+address,f'static void vector_reference{i}',1)
 candidate=(root/f'build/vector-scalar-20261103/candidate{i}.c').read_text().split('#include "recomp_types.h"\n',1)[1]
 candidate=candidate.replace(f'extern void reference{i}(void);','').replace(f'reference{i}()',f'vector_reference{i}()').replace(f'void candidate{i}(void)',f'void recomp_thething_{address}(void)')
 s=s[:m.start()]+fallback+'\n'+candidate+s[m.end():]
p.write_text(s)
