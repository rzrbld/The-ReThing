#!/usr/bin/env python3
"""Prepare identical reference/candidate math bodies and an optional CPU timer."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];base=root/'build/leaf-registers-20261102';out=root/'build/guest-alias-20261102';out.mkdir(exist_ok=True)
s=(base/'probe.c').read_text()
for i,p in enumerate(json.loads((base/'patches.json').read_text())):
 rename='__attribute__((noinline)) static void candidate'+str(i)
 old=p['candidate'].replace('void recomp_thething_'+p['address'],rename)
 new=p['original'].replace('void recomp_thething_'+p['address'],rename)
 assert s.count(old)==1;s=s.replace(old,new,1)
s=s.replace('#include <assert.h>','#include <assert.h>\n#include <time.h>')
bench='''
static int benchmark(int which,int index,unsigned iterations){
 void(*refs[])(void)={reference0,reference1,reference2,reference3,reference4};
 void(*opts[])(void)={candidate0,candidate1,candidate2,candidate3,candidate4};
 if(index<0||index>=5||which<0||which>1||!iterations)return 3;
 for(unsigned j=0;j<sizeof initial;j+=4){float v=((int)(j%17)-8)*0.125f;memcpy(initial+j,&v,4);}
 setup(1);if(index==1){float scalar=0.75f;memcpy(mem+2052,&scalar,4);}
 void(*fn)(void)=which?opts[index]:refs[index];
 struct timespec a,b;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&a);
 for(unsigned j=0;j<iterations;j++){g_esp=2048;g_fp_top=3;fn();}
 clock_gettime(CLOCK_THREAD_CPUTIME_ID,&b);
 double ns=(b.tv_sec-a.tv_sec)*1e9+(b.tv_nsec-a.tv_nsec);
 printf("mode=%d function=%d iterations=%u cpu_ns_per_call=%.6f checksum=%u\\n",which,index,iterations,ns/iterations,MEM32(768)^g_eax);
 return 0;
}
'''
s=s.replace('int main(void){',bench+'\nint main(int argc,char **argv){\n if(argc==4)return benchmark(atoi(argv[1]),atoi(argv[2]),(unsigned)strtoul(argv[3],NULL,10));')
(out/'probe.c').write_text(s)
print('Prepared identical reference/candidate bodies; benchmark optional')
