/* Diagnostic-only recorder, included inside the D3D facade after rgba_pixels.
 * One labelled decal draw per file. Raw D8State is guarded by size/version;
 * this is a local test fixture, never a distribution containing game assets. */
static void decal_capture(unsigned primitive,unsigned count,const void *vertices,
 unsigned stride,unsigned vcount,const void *indices,unsigned isize,unsigned base,
 Object *vb,size_t voff,Object *ib,size_t ioff,Object *target){
 if(!diagnostic_frame||!thing_effect_draw_category||thing_effect_draw_category()!=3)return;
 unsigned n=d8_primitive_vertices(primitive,count);
 if(!vb||!ib||!indices||voff>vb->length||ioff>ib->length||
    (isize!=2&&isize!=4)||(uint64_t)n*isize>ib->length-ioff||!target->backbuffer)
  hle_fatal("decal capture: unsupported source/target");
 size_t vertex_bytes=vb->length-voff,index_bytes=(size_t)n*isize;
 if(vertex_bytes>(64u<<20)||index_bytes>(64u<<20))hle_fatal("decal capture: oversized draw");
 char path[PATH_MAX];const char *root=setting("DS_ROOT");
 if(!root||snprintf(path,sizeof path,"%s/decal-%06u-%04u.bin",root,frames+1,diagnostic_draw_count)>=(int)sizeof path)
  hle_fatal("decal capture: path");
 FILE*f=fopen(path,"wb");if(!f)hle_fatal("decal capture: open");
 uint32_t header[]={0x31434454,1,sizeof(D8State),primitive,count,stride,vcount,base,isize,
  vb->self,target->height,(uint32_t)vertex_bytes,(uint32_t)index_bytes,2};
#define DC_WRITE(p,n) do{if(fwrite((p),1,(n),f)!=(n))hle_fatal("decal capture: write");}while(0)
 DC_WRITE(header,sizeof header);DC_WRITE(&state,sizeof state);
 DC_WRITE(vertices,vertex_bytes);DC_WRITE(indices,index_bytes);
 for(unsigned t=0;t<2;t++){
  Object*o=object(bound_texture[t]);uint32_t meta[4]={0};
  if(o){
   if(o->kind!=TEXTURE||o->gpu_dirty||o->width>2048||o->height>2048||o->levels>MAX_LEVELS)
    hle_fatal("decal capture: unsupported texture");
   meta[0]=o->self;meta[1]=o->width;meta[2]=o->height;meta[3]=o->levels;
  }
  DC_WRITE(meta,sizeof meta);
  if(o)for(unsigned l=0;l<o->levels;l++){
   if(!o->memory[l])hle_fatal("decal capture: missing texture backing");
   uint8_t*rgba=rgba_pixels(o,l);if(!rgba)hle_fatal("decal capture: decode");
   size_t bytes=(size_t)mipdim(o->width,l)*mipdim(o->height,l)*4;
   DC_WRITE(rgba,bytes);free(rgba);
  }
 }
 if(fclose(f))hle_fatal("decal capture: close");
 fprintf(diagnostic_frame,"decal-record file=%s vertex_bytes=%zu index_bytes=%zu\n",path,vertex_bytes,index_bytes);
#undef DC_WRITE
}
