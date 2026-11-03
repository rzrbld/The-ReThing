#!/usr/bin/env python3
"""Vectorize independent skinning outputs while retaining guest instruction order."""
import hashlib,json,re,runpy,sys
from pathlib import Path
p=Path(__file__).resolve().parents[1]
inline_guards='--inline-guards' in sys.argv
variant='skin-simd-inline' if inline_guards else 'skin-simd'
old=runpy.run_path(str(p/'scripts/probe_skin_locals.py'))
text=old['text'];blocks=old['blocks'];metadata=[];patches=[]
vectors='''#if defined(__aarch64__)
#include <arm_neon.h>
typedef float64x2_t Pair;
static Pair pair(double a,double b){double v[2]={a,b};return vld1q_f64(v);}
#define MUL(a,b) vmulq_f64(a,b)
#define ADD(a,b) vaddq_f64(a,b)
static void unpack(double *out,Pair v){vst1q_f64(out,v);}
#else
typedef double Pair __attribute__((vector_size(16)));
static Pair pair(double a,double b){return (Pair){a,b};}
#define MUL(a,b) ((a)*(b))
#define ADD(a,b) ((a)+(b))
static void unpack(double *out,Pair v){memcpy(out,&v,16);}
#endif
'''
text=text.replace('uint32_t g_eax',vectors+'uint32_t g_eax',1)
finite_matrix="""static int finite_matrix(uint32_t addr){
#if defined(__aarch64__)
 uint32x4_t bad=vdupq_n_u32(0),mask=vdupq_n_u32(0x7f800000u);
 for(unsigned i=0;i<4;i++)bad=vorrq_u32(bad,vceqq_u32(vandq_u32(vld1q_u32((const uint32_t*)ADDR(addr+i*16)),mask),mask));
 return vmaxvq_u32(bad)==0;
#else
 typedef uint32_t Words __attribute__((vector_size(16)));Words bad={0,0,0,0},mask={0x7f800000u,0x7f800000u,0x7f800000u,0x7f800000u};
 for(unsigned i=0;i<4;i++){Words v;memcpy(&v,(void*)ADDR(addr+i*16),16);bad|=(Words)((v&mask)==mask);}
 uint32_t lanes[4];memcpy(lanes,&bad,16);return !(lanes[0]|lanes[1]|lanes[2]|lanes[3]);
#endif
}
"""
text=text.replace('static int finite_inputs',finite_matrix+'static int finite_inputs')
text=text.replace('for(unsigned k=0;k<16;k++)if((MEM32(matrix+k*4)&0x7f800000u)==0x7f800000u)return 0;','if(!finite_matrix(matrix))return 0;')

def scalar(a):
 if a[0]=='load':return f'((double)*(float*)ADDR({a[1]}))'
 return '('+scalar(a[1])+('+' if a[0]=='add' else '*')+scalar(a[2])+')'
def paired(a,b):
 assert a[0]==b[0]
 if a[0]=='load':return f'pair({scalar(a)},{scalar(b)})'
 return a[0].upper()+'('+paired(a[1],b[1])+','+paired(a[2],b[2])+')'
for block,code in enumerate(blocks):
 stack=[];outputs=[];skeleton=[];peak=0
 for line in code.splitlines():
  if 'fp_push(' in line:
   m=re.search(r'MEM32\(([^)]+)\)',line);assert m,line;stack.append(('load',m[1]));peak=max(peak,len(stack))
  elif 'FP_ST(0) *=' in line:
   m=re.search(r'ADDR\(([^)]+)\)',line);assert m,line;stack[-1]=('mul',stack[-1],('load',m[1]))
  elif 'fp_pop(); FP_ST(0) +=' in line:
   b=stack.pop();a=stack.pop();stack.append(('add',a,b))
  elif 'FP_ST(0) += (double)' in line:
   m=re.search(r'ADDR\(([^)]+)\)',line);assert m,line;stack[-1]=('add',stack[-1],('load',m[1]))
  elif 'fstp' in line:
   m=re.search(r'ADDR\(([^)]+)\)',line);assert m,line;outputs.append((m[1],stack.pop()));skeleton.append(('store',len(outputs)-1))
  else:
   assert not any(k in line for k in ['fp_', 'FP_ST','goto','RECOMP_CALL']),line
   skeleton.append(('line',line))
 assert len(outputs)==3 and not stack
 matrix='esi' if block else '((esi<<6)+MEM32(MEM32(esp+0x58)+0x3c))'
 candidate=f'__attribute__((noinline)) static void candidate{block}(void){{\nif(!finite_inputs(eax,{matrix},esp)){{reference{block}();return;}}\n'
 computed=False
 for kind,value in skeleton:
  if kind=='line':candidate+=value+'\n'
  else:
   if not computed:
    candidate+=' double skin_result[2];unpack(skin_result,'+paired(outputs[0][1],outputs[1][1])+');\n'
    candidate+=' double skin_third='+scalar(outputs[2][1])+';\n';computed=True
   expr='skin_third' if value==2 else f'skin_result[{value}]'
   candidate+=f' *(float*)ADDR({outputs[value][0]})=(float){expr};\n'
 candidate+=f' for(unsigned i=1;i<={peak};i++){{unsigned slot=(FP_TOS_SLOT-i)&7u;g_st[slot]=0.0;g_st_i64[slot]=0;}}\n}}\n'
 start=text.index(f'__attribute__((noinline)) static void candidate{block}')
 end=text.index('__attribute__((noinline)) static void reference1' if block==0 else 'static void setup',start)
 text=text[:start]+candidate+text[end:]
 patches.append({'original':code,'body':candidate.split('\n',2)[2].rsplit('}',1)[0],'condition':f'finite_inputs(eax,{matrix},esp)'})
 metadata.append({'block':block,'source_sha256':hashlib.sha256(code.encode()).hexdigest(),'outputs':outputs,'peak_stack':peak})
# Extend finite-edge coverage while retaining full-memory/reg/flag comparison.
text=text.replace('const int modes[]={','const uint32_t edges[]={0,0x80000000,1,0x80000001,0x007fffff,0x00800000,0x7f7fffff,0xff7fffff};\n const int modes[]={')
text=text.replace('if(trial%2)v=(v&0x807fffff)|((115+next()%25)<<23);','if(trial%3==1)v=(v&0x807fffff)|((115+next()%25)<<23);else if(trial%3==2)v=edges[next()%8];')
text=text.replace('candidate?"local":"ring"','candidate?"SIMD":"ring"')
if inline_guards:
 text=text.replace('static int finite_inputs','static inline __attribute__((always_inline)) int finite_inputs').replace('static int finite_matrix','static inline __attribute__((always_inline)) int finite_matrix')
b=p/'build'/variant;b.mkdir(exist_ok=True);(b/'probe.c').write_text(text)
e=p/'docs/evidence'/variant;e.mkdir(exist_ok=True);(e/'blocks.json').write_text(json.dumps(metadata,indent=2)+'\n')
(b/'patches.json').write_text(json.dumps(patches,indent=2)+'\n')
header='#include <stdlib.h>\n#include <stdio.h>\n#include <string.h>\n'+vectors+finite_matrix+text[re.search(r'static(?: inline __attribute__\(\(always_inline\)\))? int finite_inputs',text).start():text.index('__attribute__((noinline)) static void reference0')]
header+='static int thing_skin_simd_enabled(void){static int enabled=-1;if(enabled<0){const char *v=getenv("THING_SKIN_SIMD");enabled=v&&!strcmp(v,"1");if(enabled)fprintf(stderr,"[skin-simd] experimental paired-double skinning enabled\\n");}return enabled;}\n'
if inline_guards:
 header=header.replace('static int finite_inputs','static inline __attribute__((always_inline)) int finite_inputs').replace('static int finite_matrix','static inline __attribute__((always_inline)) int finite_matrix')
 start=header.index('static int thing_skin_simd_enabled')
 header=header[:start]+"""static int thing_skin_simd_value=-1;
static __attribute__((cold,noinline)) int thing_skin_simd_init(void){const char *v=getenv("THING_SKIN_SIMD");thing_skin_simd_value=v&&!strcmp(v,"1");if(thing_skin_simd_value)fprintf(stderr,"[skin-simd] experimental paired-double skinning enabled\\n");return thing_skin_simd_value;}
static inline __attribute__((always_inline)) int thing_skin_simd_enabled(void){int v=thing_skin_simd_value;return v>=0?v:thing_skin_simd_init();}
"""
(b/'inline.h').write_text(header)
print(b/'probe.c')
