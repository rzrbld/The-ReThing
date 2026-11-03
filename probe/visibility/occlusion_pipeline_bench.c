/* Combined offline replay. Live queue/resource/state extraction is excluded. */
#define main coverage_component_main
#include "coarse_occlusion_bench.c"
#undef main
typedef struct {uint32_t id,bits,cull,n,expected,segment;double a[54],*xyz;} Mesh;
typedef struct {double low[3],high[3],score;int valid;} Box;
typedef struct {unsigned index;uint32_t id;double score;} Ranked;
static int project(const double *xyz,const double *m,double *out){
 double p[4]={xyz[0],xyz[1],xyz[2],1};
 for(unsigned stage=0;stage<3;stage++){
  double q[4];const double *v=m+stage*16;
  for(unsigned j=0;j<4;j++)q[j]=p[0]*v[j]+p[1]*v[j+4]+p[2]*v[j+8]+p[3]*v[j+12];
  memcpy(p,q,sizeof p);
 }
 if(!isfinite(p[3])||p[3]<=1e-4)return 0;
 for(unsigned j=0;j<3;j++){out[j]=p[j]/p[3];if(!isfinite(out[j]))return 0;}
 return 1;
}
static Box box(const Mesh *m){
 Box b={{INFINITY,INFINITY,INFINITY},{-INFINITY,-INFINITY,-INFINITY},-1,0};
 for(unsigned c=0;c<8;c++){
  double v[3]={m->a[c&1?3:0],m->a[c&2?4:1],m->a[c&4?5:2]},q[3];if(!project(v,m->a+6,q))return b;
  for(unsigned j=0;j<3;j++){if(q[j]<b.low[j])b.low[j]=q[j];if(q[j]>b.high[j])b.high[j]=q[j];}
 }
 b.score=fmax(0,fmin(1,b.high[0])-fmax(-1,b.low[0]))*fmax(0,fmin(1,b.high[1])-fmax(-1,b.low[1]))*fmax(1e-6,1-b.low[2]);b.valid=1;return b;
}
static unsigned clip_triangle(const double *tri,double polygon[16][3]){
 memcpy(polygon,tri,9*sizeof(double));unsigned n=3;
 const unsigned axis[6]={0,0,1,1,2,2};const double sign[6]={1,-1,1,-1,1,-1},bound[6]={1,1,1,1,1,0};
 for(unsigned plane=0;plane<6;plane++){
  double next[16][3];unsigned count=0;
  for(unsigned i=0;i<n;i++){
   const double *a=polygon[i],*b=polygon[(i+1)%n];double da=bound[plane]-sign[plane]*a[axis[plane]],db=bound[plane]-sign[plane]*b[axis[plane]];
   if(da>=0){if(count==16)return 0;memcpy(next[count++],a,24);}
   if((da>=0)!=(db>=0)){if(count==16)return 0;for(unsigned j=0;j<3;j++)next[count][j]=a[j]+(b[j]-a[j])*(da/(da-db));count++;}
  }
  if(count<3)return 0;
  memcpy(polygon,next,count*3*sizeof(double));n=count;
 }
 return n;
}
static int by_draw(const void *a,const void *b){const Triangle *x=a,*y=b;return x->draw<y->draw?-1:x->draw>y->draw;}
static unsigned pipeline(const Mesh *m,unsigned n,Box *boxes,double *scratch,int verify,unsigned mesh_budget,unsigned triangle_budget){
 Ranked rank[32];unsigned nr=0;
 for(unsigned i=0;i<n;i++){
  boxes[i].valid=0;if(!(m[i].bits&1))continue;boxes[i]=box(m+i);if(!boxes[i].valid)continue;
  double score=boxes[i].score;unsigned at=0;
  while(at<nr&&(rank[at].score>score||(rank[at].score==score&&rank[at].id>m[i].id)))at++;
  if(at==mesh_budget)continue;
  if(nr<mesh_budget)nr++;
  for(unsigned j=nr-1;j>at;j--)rank[j]=rank[j-1];
  rank[at]=(Ranked){i,m[i].id,score};
 }
 unsigned chosen[32];for(unsigned i=0;i<nr;i++)chosen[i]=rank[i].index;
 /* Stable original draw order before score ties, matching Python reference. */
 for(unsigned i=1;i<nr;i++){unsigned v=chosen[i],j=i;while(j&&chosen[j-1]>v){chosen[j]=chosen[j-1];j--;}chosen[j]=v;}
 Triangle triangles[16];double scores[16];unsigned nt=0;
 for(unsigned k=0;k<nr;k++){
  const Mesh *a=m+chosen[k];if(!(a->bits&2))continue;int valid=1;
  for(unsigned i=0;i<a->n;i++)if(!project(a->xyz+i*3,a->a+6,scratch+i*3)){valid=0;break;}
  if(!valid)continue;
  for(unsigned i=0;i<a->n;i+=3){
   double poly[16][3];unsigned points=clip_triangle(scratch+i*3,poly);
   for(unsigned j=1;j+1<points;j++){
    Triangle t={a->id,{0}};memcpy(t.v,poly[0],24);memcpy(t.v+3,poly[j],24);memcpy(t.v+6,poly[j+1],24);
    double ux=t.v[3]-t.v[0],uy=t.v[4]-t.v[1],vx=t.v[6]-t.v[0],vy=t.v[7]-t.v[1],area=ux*vy-vx*uy;
    if((a->cull==2&&area<0)||(a->cull==3&&area>0)||fabs(area)<.02)continue;
    double score=fabs(area)*fmax(1e-6,1-fmin(t.v[2],fmin(t.v[5],t.v[8])));unsigned at=0;
    while(at<nt&&scores[at]>=score)at++;
    if(at==triangle_budget)continue;
    if(nt<triangle_budget)nt++;
    for(unsigned z=nt-1;z>at;z--){triangles[z]=triangles[z-1];scores[z]=scores[z-1];}triangles[at]=t;scores[at]=score;
   }
  }
 }
 qsort(triangles,nt,sizeof *triangles,by_draw);for(unsigned i=0;i<64*48;i++)depth[i]=INFINITY;
 unsigned next=0,rejected=0,segment=0;
 for(unsigned i=0;i<n;i++){
  if(segment!=m[i].segment){for(unsigned j=0;j<64*48;j++)depth[j]=INFINITY;segment=m[i].segment;}
  int hidden=0;const Box *b=boxes+i;
  if((m[i].bits&4)&&b->valid&&b->low[2]>=0){
   double x0=fmax(-1,b->low[0]-1e-4),y0=fmax(-1,b->low[1]-1e-4),x1=fmin(1,b->high[0]+1e-4),y1=fmin(1,b->high[1]+1e-4);
   if(x0<=x1&&y0<=y1){
    int left=(int)floor((x0+1)*32),top=(int)floor((y0+1)*24),right=clampi((int)floor((x1+1)*32),63),bottom=clampi((int)floor((y1+1)*24),47);
    hidden=left<=right&&top<=bottom;
    for(int y=top;y<=bottom&&hidden;y++)for(int x=left;x<=right;x++)if(!(depth[y*64+x]<b->low[2]-1e-4)){hidden=0;break;}
   }
  }
  if(verify&&hidden!=(int)m[i].expected){fprintf(stderr,"Decision mismatch draw=%u got=%d expected=%u\n",m[i].id,hidden,m[i].expected);exit(2);}rejected+=hidden;
  while(next<nt&&triangles[next].draw==m[i].id)raster(triangles+next++,64,48);
 }
 return rejected;
}
#ifndef THING_PIPELINE_MAIN
#define THING_PIPELINE_MAIN main
#endif
int THING_PIPELINE_MAIN(int argc,char **argv){
 if(argc<2)return 2;
 unsigned reps=argc>2?(unsigned)strtoul(argv[2],0,10):1000;if(!reps)return 2;
 FILE *f=fopen(argv[1],"rb");if(!f)return 2;uint32_t h[4];read_exact(f,h,16);if(h[0]!=0x504c4332||!h[1]||h[1]>10000||!h[2]||h[2]>32||!h[3]||h[3]>16)return 2;
 unsigned n=h[1],maxref=1;Mesh *m=calloc(n,sizeof *m);Box *b=calloc(n,sizeof *b);if(!m||!b)return 2;
 for(unsigned i=0;i<n;i++){
  read_exact(f,&m[i].id,24);read_exact(f,m[i].a,432);if(m[i].n>1000000||m[i].n%3||m[i].expected>1||!m[i].segment||(i&&m[i].id<=m[i-1].id))return 2;
  if(m[i].n){m[i].xyz=malloc(m[i].n*24);if(!m[i].xyz)return 2;read_exact(f,m[i].xyz,m[i].n*24);}
  if(m[i].n>maxref)maxref=m[i].n;
 }
 if(fgetc(f)!=EOF)return 2;
 fclose(f);double *scratch=malloc(maxref*24);if(!scratch)return 2;
 unsigned rejected=pipeline(m,n,b,scratch,1,h[2],h[3]);volatile unsigned checksum=0;
 for(unsigned i=0;i<20;i++)checksum+=pipeline(m,n,b,scratch,0,h[2],h[3]);
 double start=now();for(unsigned i=0;i<reps;i++)checksum+=pipeline(m,n,b,scratch,0,h[2],h[3]);
 printf("PASS meshes=%u rejected=%u combined_us=%.3f iterations=%u checksum=%u\n",n,rejected,(now()-start)*1e6/reps,reps,checksum);
 for(unsigned i=0;i<n;i++)free(m[i].xyz);
 free(m);free(b);free(scratch);return 0;
}
