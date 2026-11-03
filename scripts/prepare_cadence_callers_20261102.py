#!/usr/bin/env python3
"""Stage per-caller math counts; preserve earlier diagnostic evidence."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1];src=root/'build/cadence-counts-20261102';out=root/'build/cadence-callers-20261102';out.mkdir(exist_ok=True)
leaves=['00414210','00414230','00412F00','005B7F80']
patches=json.loads((src/'patches.json').read_text())
for p in patches:
 address=p['name'].rsplit('_',1)[-1]
 if address in leaves:
  marker='RECOMP_ENTER(0x'+address+'u);'
  assert p['replacement'].count(marker)==1
  p['replacement']=p['replacement'].replace(marker,f'thing_cadence_call({leaves.index(address)},g_cur_func);\n    '+marker,1)
(out/'patches.json').write_text(json.dumps(patches,indent=2)+'\n')
s=(src/'counter.c').read_text()
s='static void cadence_call_begin(void);\nstatic void cadence_call_end(unsigned,unsigned,double,double);\n'+s
s=s.replace('memcpy(begin_counts,thing_cadence_counts,sizeof begin_counts);','memcpy(begin_counts,thing_cadence_counts,sizeof begin_counts);cadence_call_begin();')
s=s.replace('if(fclose(f))abort();phase=2;', 'if(fclose(f))abort();cadence_call_end(first,frame,start,elapsed);phase=2;')
s+='''
#define HIST_CAP 16384
static uint64_t hist_keys[HIST_CAP],hist_counts[HIST_CAP],hist_begin[HIST_CAP];
void thing_cadence_call(unsigned leaf,uint32_t caller){
 if(leaf>=4)abort();uint64_t key=((uint64_t)caller<<3)|(leaf+1);
 unsigned slot=(unsigned)((key*UINT64_C(11400714819323198485))>>50);
 for(unsigned probe=0;probe<HIST_CAP;probe++,slot=(slot+1)&(HIST_CAP-1)){
  if(!hist_keys[slot]||hist_keys[slot]==key){hist_keys[slot]=key;hist_counts[slot]++;return;}
 }
 abort();
}
static void cadence_call_begin(void){memcpy(hist_begin,hist_counts,sizeof hist_begin);}
static void cadence_call_end(unsigned first,unsigned frame,double start,double elapsed){
 const char*root=getenv("THING_ROOT");if(!root)abort();char path[PATH_MAX];
 if(snprintf(path,sizeof path,"%s/cadence-callers.csv",root)>=(int)sizeof path)abort();
 FILE*f=fopen(path,"w");if(!f)abort();
 const char *leaves[]={"00414210","00414230","00412F00","005B7F80"};
 fputs("leaf,caller,begin_frame,end_frame,begin_seconds,end_seconds,delta_calls\\n",f);
 for(unsigned i=0;i<HIST_CAP;i++)if(hist_keys[i]&&hist_counts[i]!=hist_begin[i])
  fprintf(f,"%s,%08x,%u,%u,%.9f,%.9f,%llu\\n",leaves[(hist_keys[i]&7)-1],(unsigned)(hist_keys[i]>>3),first,frame,start,elapsed,(unsigned long long)(hist_counts[i]-hist_begin[i]));
 if(fclose(f))abort();
}
'''
(out/'counter.c').write_text(s)
m=json.loads((src/'manifest.json').read_text());m['leaves']=leaves;m['note']='Counts BEFORE leaf RECOMP_ENTER attribute maintained g_cur_func caller; not a native stack trace or instruction count.'
(out/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print('Staged four math caller histograms with bounded exact-key table; runtime unchanged')
