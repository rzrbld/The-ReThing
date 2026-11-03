#!/usr/bin/env python3
"""Stage order-preserving alpha batching experiment; never mutate runtime."""
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'build/ordered-alpha-batch-20261102';out.mkdir(exist_ok=True)
s=(root/'runtime/adjacent_batch.c').read_text()
assert s.count('||d->state->rs[27]')==1
s=s.replace('||d->state->rs[27]','')
(out/'adjacent_batch.c').write_text(s)
s=(root/'probe/batch/adjacent_batch_selftest.c').read_text()
s=s.replace('static unsigned emitted;', 'static unsigned emitted;\nstatic unsigned reverse_order;')
s=s.replace(' for(unsigned i=0;i<N;i++){', ''' if(scene>=12){
  state.rs[27]=1;state.rs[19]=5;state.rs[20]=6;state.rs[14]=scene==15;
  state.tss[0][1]=2;state.tss[0][2]=0;state.tss[0][4]=2;state.tss[0][5]=0;
  if(scene==13)state.rs[20]=2;
  if(scene==14){state.rs[19]=9;state.rs[20]=1;}
  if(scene==17){state.rs[15]=1;state.rs[25]=5;state.rs[24]=90;}
 }
 for(unsigned i=0;i<N;i++){''',1)
s=s.replace('  vertices[i*6+1].p[0]=x;', '''  if(scene>=12)for(unsigned j=0;j<6;j++)vertices[i*6+j].color=((64+i*12)<<24)|((30+i*17)<<16)|((220-i*13)<<8)|(40+i*11);
  vertices[i*6+1].p[0]=x;''',1)
# Only the draw loop, not geometry setup, is reversed for sensitivity check.
a=s.index('static unsigned render(');s=s[:a]+s[a:].replace(' for(unsigned i=0;i<N;i++){',' for(unsigned step=0;step<N;step++){\n  unsigned i=reverse_order?N-1-step:step;\n  if(scene==16&&i==6)state.rs[20]=2;',1)
s=s.replace('state.rs[27]=1;assert(thing_batch_add(b,&d)==1);state.rs[27]=0;', 'state.rs[27]=1;assert(!thing_batch_add(b,&d));assert(!thing_batch_flush(b));state.rs[27]=0;')
s=s.replace('scene<12','scene<18').replace('covered+=a[i*4]>30;', 'covered+=a[i*4]>6||a[i*4+1]>9||a[i*4+2]>12||a[i*4+3]<250;')
s=s.replace(' validation();puts(', ''' render(12,0,a);reverse_order=1;render(12,0,b);reverse_order=0;
 assert(memcmp(a,b,W*H*4));puts("PASS: alpha test is sensitive to reversed primitive order");
 validation();puts(''')
s=s.replace('PASS: 12 exact640x480','PASS: 18 exact640x480')
(out/'selftest.c').write_text(s)
print('Staged alpha batch and 18-scene differential test; runtime unchanged')
