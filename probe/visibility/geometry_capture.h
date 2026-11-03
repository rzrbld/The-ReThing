/* Single-frame diagnostic only; never suppresses a game draw. Native little-
 * endian uint32/float32 records. No GPU reads and no temporal visibility guesses. */
#ifndef THING_GEOMETRY_CAPTURE_H
#define THING_GEOMETRY_CAPTURE_H
static FILE *gc_file;static unsigned gc_draws;
extern void thing_geometry_identity(uint32_t out[5]);
static void gc_end(void){if(gc_file){fclose(gc_file);gc_file=NULL;}}
static void gc_begin(const char *root,unsigned frame){gc_end();char p[4096];if(snprintf(p,sizeof p,"%s/geometry-%u.bin",root,frame)>=(int)sizeof p)return;gc_file=fopen(p,"wb");gc_draws=0;if(gc_file){uint32_t h[]={0x31434754,1,frame};fwrite(h,4,3,gc_file);}}
static void gc_capture(unsigned primitive,unsigned count,const void *vertices,unsigned stride,unsigned vcount,const void *indices,unsigned isize,unsigned base,unsigned rt,unsigned depth){
 if(!gc_file)return;
 uint32_t identity[5];thing_geometry_identity(identity);
 unsigned n=d8_primitive_vertices(primitive,count),valid=1;
 if(!vertices||stride<12||n>1000000||gc_draws>=4096||(indices&&isize!=2&&isize!=4))valid=0;
 if(valid)for(unsigned j=0;j<n;j++){uint32_t ix=j;if(indices){ix=0;memcpy(&ix,(const char*)indices+(size_t)j*isize,isize);}if(ix>=vcount||ix>UINT32_MAX-base||(size_t)(ix+base)>(SIZE_MAX-12)/stride){valid=0;break;}}
 uint32_t h[]={0x57415244,++gc_draws,primitive,count,state.fvf,n,valid,stride,rt,depth,identity[0],identity[1],identity[2],identity[3],identity[4]};
 fwrite(h,4,15,gc_file);fwrite(state.rs,4,256,gc_file);fwrite(state.tss,4,256,gc_file);fwrite(state.viewport,4,6,gc_file);
 unsigned matrices[]={256,2,3};for(unsigned m=0;m<3;m++)fwrite(state.transform[matrices[m]],4,16,gc_file);
 if(valid)for(unsigned j=0;j<n;j++){uint32_t ix=j;if(indices){ix=0;memcpy(&ix,(const char*)indices+(size_t)j*isize,isize);}fwrite((const char*)vertices+(size_t)(ix+base)*stride,1,12,gc_file);}
}
#endif
