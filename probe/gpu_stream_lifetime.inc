/* Included after DrawCase in the isolated GPU-stream test. */
static void stream_lifetime_check(const DrawCase *c,const char *images_dir) {
    if(strcmp(c->name,"unlit_textured"))return;
    unsigned char images[2][WIDTH*HEIGHT*4],vertices[4096];
    uint32_t indices32[1024];
    unsigned count=d8_primitive_vertices(c->primitive,c->primitive_count);
    size_t bytes=(size_t)c->vertex_count*c->stride;
    if(c->base||c->state->fvf!=0x142||bytes>sizeof vertices||count>1024)exit(2);
    for(unsigned i=0;i<count;i++)indices32[i]=c->indices[i];
    size_t old_limit=g_gpu.gpu_stream_limit;
    g_gpu.gpu_stream_limit=512;
    for(unsigned backend=0;backend<2;backend++) {
        D8Renderer *renderer=backend?&g_gpu:&g_cpu;
        Target *target=backend?&g_target_gpu:&g_target_cpu;
        gl_extra_reset();target_clear(target);
        for(unsigned tile=0;tile<16;tile++) {
            if(tile%7==0)d8_renderer_begin_frame(renderer);
            D8State state=*c->state;
            state.viewport[0]=(tile%4)*(WIDTH/4);
            state.viewport[1]=(tile/4)*(HEIGHT/4);
            state.viewport[2]=WIDTH/4;state.viewport[3]=HEIGHT/4;
            memcpy(vertices,c->vertices,bytes);
            for(unsigned v=0;v<c->vertex_count;v++) {
                uint32_t color;memcpy(&color,vertices+v*c->stride+12,4);
                color^=((tile+1)*0x131b2du)&0x00ffffffu;
                memcpy(vertices+v*c->stride+12,&color,4);
            }
            const void *indices=(tile&1)?(const void*)indices32:(const void*)c->indices;
            int rc=d8_renderer_draw_native(renderer,&state,HEIGHT,c->primitive,c->primitive_count,
                    vertices,c->stride,c->vertex_count,indices,(tile&1)?4:2,0,c->textures,NULL);
            if(rc)exit(2);
        }
        /* No readback or finish between the differently colored queued draws. */
        if(glGetError()!=GL_NO_ERROR)exit(2);
        target_read(target,images[backend]);
    }
    g_gpu.gpu_stream_limit=old_limit;
    CaseResult result={0};result.tolerance=2;
    compare_images(images[0],images[1],&result);
    printf("queued_storage_lifetime max=%.0f over=%u covered=%u\n",
            result.max_any,result.over_tolerance,result.covered_pixels);
    for(unsigned i=0;i<2;i++) {
        char path[1024];snprintf(path,sizeof path,"%s/queued-storage-%s.png",images_dir,i?"gpu":"cpu");
        save_png_flipped(path,images[i],WIDTH,HEIGHT);
    }
    if(result.over_tolerance||result.covered_pixels<500)exit(2);
}
