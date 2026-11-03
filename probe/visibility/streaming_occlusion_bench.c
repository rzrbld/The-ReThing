/* Sequential offline replay; original material/state execution remains untouched.
 * Uses prior opaque draws only, a size threshold and a hard scan budget. */
#define THING_PIPELINE_MAIN selected_pipeline_main
#include "occlusion_pipeline_bench.c"
#undef THING_PIPELINE_MAIN
static unsigned stream(const Mesh *m,unsigned n,double *scratch,int verify){
 unsigned segment=0,nr=0,scans=0,rejected=0;double rank[4];
 for(unsigned i=0;i<64*48;i++)depth[i]=INFINITY;
 for(unsigned k=0;k<n;k++){
  const Mesh *a=m+k;
  if(a->segment!=segment){for(unsigned i=0;i<64*48;i++)depth[i]=INFINITY;segment=a->segment;nr=0;}
  Box b=box(a);int hidden=0;
  if((a->bits&4)&&b.valid&&b.low[2]>=0){
   double x0=fmax(-1,b.low[0]-1e-4),y0=fmax(-1,b.low[1]-1e-4),x1=fmin(1,b.high[0]+1e-4),y1=fmin(1,b.high[1]+1e-4);
   if(x0<=x1&&y0<=y1){
    int left=(int)floor((x0+1)*32),top=(int)floor((y0+1)*24),right=clampi((int)floor((x1+1)*32),63),bottom=clampi((int)floor((y1+1)*24),47);
    hidden=left<=right&&top<=bottom;
    for(int y=top;y<=bottom&&hidden;y++)for(int x=left;x<=right;x++)if(!(depth[y*64+x]<b.low[2]-1e-4)){hidden=0;break;}
   }
  }
  if(verify&&hidden!=(int)a->expected){fprintf(stderr,"Streaming mismatch draw=%u got=%d expected=%u\n",a->id,hidden,a->expected);exit(2);}
  rejected+=hidden;if(hidden)continue;
  if(!(a->bits&2)||!b.valid||b.low[2]<0||b.score<=.1||scans>=16||(nr==4&&b.score<=rank[nr-1]))continue;
  unsigned at=0;while(at<nr&&rank[at]>=b.score)at++;if(nr<4)nr++;
  for(unsigned i=nr-1;i>at;i--)rank[i]=rank[i-1];
  rank[at]=b.score;scans++;
  int valid=1;for(unsigned i=0;i<a->n;i++)if(!project(a->xyz+i*3,a->a+6,scratch+i*3)){valid=0;break;}if(!valid)continue;
  Triangle selected[4];double scores[4];unsigned nt=0;
  for(unsigned i=0;i<a->n;i+=3){
   double polygon[16][3];unsigned points=clip_triangle(scratch+i*3,polygon);
   for(unsigned j=1;j+1<points;j++){
    Triangle t={a->id,{0}};memcpy(t.v,polygon[0],24);memcpy(t.v+3,polygon[j],24);memcpy(t.v+6,polygon[j+1],24);
    double ux=t.v[3]-t.v[0],uy=t.v[4]-t.v[1],vx=t.v[6]-t.v[0],vy=t.v[7]-t.v[1],area=ux*vy-vx*uy;
    if((a->cull==2&&area<0)||(a->cull==3&&area>0)||fabs(area)<.02)continue;
    double score=fabs(area)*fmax(1e-6,1-fmin(t.v[2],fmin(t.v[5],t.v[8])));unsigned at=0;
    while(at<nt&&scores[at]>=score)at++;
    if(at==4)continue;
    if(nt<4)nt++;
    for(unsigned z=nt-1;z>at;z--){selected[z]=selected[z-1];scores[z]=scores[z-1];}selected[at]=t;scores[at]=score;
   }
  }
  for(unsigned i=0;i<nt;i++)raster(selected+i,64,48);
 }
 return rejected;
}
int main(int argc,char **argv){
 if(argc<2)return 2;
 unsigned reps=argc>2?(unsigned)strtoul(argv[2],0,10):1000;if(!reps)return 2;
 FILE *f=fopen(argv[1],"rb");if(!f)return 2;uint32_t h[4];read_exact(f,h,16);if(h[0]!=0x504c4332||!h[1]||h[1]>10000)return 2;
 unsigned n=h[1],maxref=1;Mesh *m=calloc(n,sizeof *m);if(!m)return 2;
 for(unsigned i=0;i<n;i++){
  read_exact(f,&m[i].id,24);read_exact(f,m[i].a,432);if(m[i].n>1000000||m[i].n%3||m[i].expected>1||!m[i].segment||(i&&m[i].id<=m[i-1].id))return 2;
  if(m[i].n){m[i].xyz=malloc(m[i].n*24);if(!m[i].xyz)return 2;read_exact(f,m[i].xyz,m[i].n*24);}if(m[i].n>maxref)maxref=m[i].n;
 }
 if(fgetc(f)!=EOF)return 2;
 fclose(f);double *scratch=malloc(maxref*24);if(!scratch)return 2;
 unsigned rejected=stream(m,n,scratch,1);volatile unsigned checksum=0;
 for(unsigned i=0;i<20;i++)checksum+=stream(m,n,scratch,0);
 double start=now();for(unsigned i=0;i<reps;i++)checksum+=stream(m,n,scratch,0);
 printf("PASS meshes=%u rejected=%u streaming_us=%.3f iterations=%u checksum=%u\n",n,rejected,(now()-start)*1e6/reps,reps,checksum);
 for(unsigned i=0;i<n;i++)free(m[i].xyz);
 free(m);free(scratch);return 0;
}
