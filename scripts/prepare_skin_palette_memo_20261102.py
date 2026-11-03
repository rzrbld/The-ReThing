#!/usr/bin/env python3
"""Stage a bounded skin-palette memo; never edit generated baseline."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'build/skin-palette-memo-20261102';out.mkdir(exist_ok=True)
def function(path,addr):
 s=path.read_text();a=s.index('void recomp_thething_'+addr+'(');b=s.find('\nvoid ',a+1);return s[a:b if b!=-1 else None]
f=function(root/'build/translated-local-regs/thething/functions_0235.c','007AE710')
a=f.index('    PUSH32(esp, eax); /* 0x007AE8CD:');b=f.index('    eax = MEM32(ebx + 0x3C); /* 0x007AE90D:')
math=f[a:b];tail=f[b:f.index('    goto L_007AE8A3; /* 0x007AE95B:')]
leaf=function(root/'build/translated-local-regs/thething/functions_0120.c','005B7F80')
prefix='#define RECOMP_GENERATED_CODE 1\n#define RECOMP_LOCAL_REGS 1\n#include "recomp_types.h"\n'
(out/'fpstate.h').write_text((root/'scripts/skin_palette_fpstate_20261102.h').read_text())
header=(root/'scripts/skin_palette_memo_20261102.h').read_text();(out/'memo.h').write_text(header)
for name in ('reference','candidate'):
 body=math
 if name=='candidate':
  body='''uint32_t memo_sp=esp;
struct skin_memo_entry *memo_pending=0;
if(skin_memo_begin(ebx,ebp,eax,esi+ebp,esp,&memo_pending)) {
 eax=esp+0xD0;ecx=esp+0x90;edx=esp+0x110;esi=esp+0x190;edi=esp+0x150;
 MEM32(esp-4)=esp+0xD0;MEM32(esp-8)=RECOMP_RETADDR;
} else {
'''+math+'''skin_memo_end(memo_pending,memo_sp);
}
'''
 body+=tail
 text=prefix+('#include "memo.h"\n' if name=='candidate' else '')+leaf.replace('recomp_thething_005B7F80',name+'_mul')+'\nvoid '+name+'0(void){\nint _df=1;uint32_t _cf=0,_flag_a=0,_flag_b=0,_flag_k=0;RECOMP_REGS_LOCALS\nRECOMP_ENTER(0x007AE710u);\n'+body.replace('recomp_thething_005B7F80',name+'_mul')+'RECOMP_FLAGS_OUT();RECOMP_REGS_OUT();\n}\n'
 (out/(name+'.c')).write_text(text)
(out/'original-span.txt').write_text(math)
(out/'candidate-span.txt').write_text(body[:body.index('    eax = MEM32(ebx + 0x3C);')])
print(out)
