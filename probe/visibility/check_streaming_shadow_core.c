/* Verify the incremental runtime core against captured reference decisions. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static const double *input_xyz;static unsigned input_count;
static int shadow_vertex(unsigned i,double out[3]){if(i>=input_count)return 0;memcpy(out,input_xyz+i*3,24);return 1;}
#include "../../build/streaming-shadow-20261103/core.h"
static void read_exact(FILE *f,void *p,size_t n){if(fread(p,1,n,f)!=n)exit(2);}
int main(int argc,char **argv){
 if(argc!=2)return 2;
 FILE *f=fopen(argv[1],"rb");if(!f)return 2;uint32_t h[4];read_exact(f,h,16);
 if(h[0]!=0x504c4332||!h[1]||h[1]>10000)return 2;
 shadow_stream_reset();unsigned rejected=0;double scratch[16384*3];
 for(unsigned i=0;i<h[1];i++){
  shadow_Mesh m={0};read_exact(f,&m.id,24);read_exact(f,m.a,432);
  if(m.n>16384||m.n%3||m.expected>1||!m.shadow_segment)return 2;
  double *xyz=malloc((m.n?m.n:1)*24);if(!xyz)return 2;read_exact(f,xyz,m.n*24);
  input_xyz=xyz;input_count=m.n;int hidden=shadow_stream_step(&m,scratch);
  if(hidden!=(int)m.expected){fprintf(stderr,"Runtime core mismatch draw=%u got=%d expected=%u\n",m.id,hidden,m.expected);return 2;}
  rejected+=hidden;free(xyz);
 }
 if(fgetc(f)!=EOF)return 2;
 fclose(f);printf("PASS incremental_core meshes=%u rejected=%u\n",h[1],rejected);return 0;
}
