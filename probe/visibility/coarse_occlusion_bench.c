/* Offline kernel experiment. Input is already clipped/projected/selected.
 * No game hook; no shader/clear/float32 safety claim or hardware FPS inference. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
typedef struct {uint32_t draw;double v[9];} Triangle;
typedef struct {uint32_t draw,x0,y0,x1,y1;double near_z;uint32_t expected;} Query;
static double depth[64*48];
static int clampi(int x,int max){return x<0?0:x>max?max:x;}
static void raster(const Triangle *t,unsigned w,unsigned h){
 const double *v=t->v;double ax=v[0],ay=v[1],ux=v[3]-ax,uy=v[4]-ay,vx=v[6]-ax,vy=v[7]-ay,det=ux*vy-vx*uy;
 if(!isfinite(det)||fabs(det)<1e-8)return;
 double i00=vy/det,i01=-vx/det,i10=-uy/det,i11=ux/det;
 int x0=clampi((int)floor((fmin(ax,fmin(v[3],v[6]))+1)*w*.5),w-1);
 int x1=clampi((int)floor((fmax(ax,fmax(v[3],v[6]))+1)*w*.5),w-1);
 int y0=clampi((int)floor((fmin(ay,fmin(v[4],v[7]))+1)*h*.5),h-1);
 int y1=clampi((int)floor((fmax(ay,fmax(v[4],v[7]))+1)*h*.5),h-1);
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
  double maximum=-INFINITY;int inside=1;
  for(unsigned c=0;c<4;c++){
   double px=(x+(c&1))*2.0/w-1-ax,py=(y+(c>>1))*2.0/h-1-ay;
   double b=px*i00+py*i01,k=px*i10+py*i11,a=1-b-k;
   if(a<=1e-5||b<=1e-5||k<=1e-5){inside=0;break;}
   double z=a*v[2]+b*v[5]+k*v[8];if(z>maximum)maximum=z;
  }
  if(inside&&maximum+1e-4<depth[y*w+x])depth[y*w+x]=maximum+1e-4;
 }
}
static unsigned run(unsigned w,unsigned h,const Triangle *t,unsigned nt,const Query *q,unsigned nq,int verify){
 for(unsigned i=0;i<w*h;i++)depth[i]=INFINITY;
 unsigned next=0,rejected=0;
 for(unsigned i=0;i<nq;i++){
  while(next<nt&&t[next].draw<q[i].draw)raster(t+next++,w,h);
  int hidden=1;
  for(unsigned y=q[i].y0;y<=q[i].y1&&hidden;y++)for(unsigned x=q[i].x0;x<=q[i].x1;x++)if(!(depth[y*w+x]<q[i].near_z)){hidden=0;break;}
  if(verify&&hidden!=(int)q[i].expected){fprintf(stderr,"Mismatch draw %u expected %u got %d\n",q[i].draw,q[i].expected,hidden);exit(2);}
  rejected+=hidden;
 }
 return rejected;
}
static void read_exact(FILE *f,void *p,size_t n){if(fread(p,1,n,f)!=n){fprintf(stderr,"Truncated fixture\n");exit(2);}}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc,char **argv){
 if(argc<2)return 2;
 unsigned iterations=argc>2?(unsigned)strtoul(argv[2],0,10):1000;if(!iterations)return 2;
 FILE *f=fopen(argv[1],"rb");if(!f)return 2;
 uint32_t header[5];read_exact(f,header,sizeof header);
 unsigned w=header[1],h=header[2],nt=header[3],nq=header[4];
 if(header[0]!=0x514f4343||!w||w>64||!h||h>48||nt>10000||nq>10000)return 2;
 Triangle *t=calloc(nt,sizeof *t);Query *q=calloc(nq,sizeof *q);if(!t||!q)return 2;
 for(unsigned i=0;i<nt;i++){read_exact(f,&t[i].draw,4);read_exact(f,t[i].v,72);if(i&&t[i].draw<t[i-1].draw)return 2;for(unsigned j=0;j<9;j++)if(!isfinite(t[i].v[j]))return 2;}
 for(unsigned i=0;i<nq;i++){read_exact(f,&q[i].draw,20);read_exact(f,&q[i].near_z,8);read_exact(f,&q[i].expected,4);if(q[i].x0>q[i].x1||q[i].y0>q[i].y1||q[i].x1>=w||q[i].y1>=h||!isfinite(q[i].near_z)||q[i].expected>1||(i&&q[i].draw<q[i-1].draw))return 2;}
 if(fgetc(f)!=EOF)return 2;
 fclose(f);
 unsigned count=run(w,h,t,nt,q,nq,1);volatile unsigned checksum=0;
 for(unsigned i=0;i<20;i++)checksum+=run(w,h,t,nt,q,nq,0);
 double start=now();for(unsigned i=0;i<iterations;i++)checksum+=run(w,h,t,nt,q,nq,0);double elapsed=now()-start;
 printf("PASS grid=%ux%u triangles=%u queries=%u rejected=%u kernel_us=%.3f iterations=%u checksum=%u\n",w,h,nt,nq,count,elapsed*1e6/iterations,iterations,checksum);
 free(t);free(q);return 0;
}
