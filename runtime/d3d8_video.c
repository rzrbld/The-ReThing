/* GLES2 movie output. This does not touch guest memory or guest D3D resources. */
#include "d3d8_video.h"
#include <GLES2/gl2.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {GLint enabled,size,type,normalized,stride,buffer;void *pointer;} Attribute;
typedef struct {
    GLint program,array_buffer,framebuffer,active_texture,texture0,viewport[4],unpack;
    GLboolean blend,depth,stencil,cull,scissor,color_mask[4];
    GLfloat clear_color[4];
    Attribute attribute[2];
} VideoState;
static struct {GLuint program,vbo,texture;GLint sampler;int width,height;SDL_GLContext owner;unsigned frames;} video;

static void state_save(VideoState *s){
    glGetIntegerv(GL_CURRENT_PROGRAM,&s->program);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&s->array_buffer);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING,&s->framebuffer);glGetIntegerv(GL_ACTIVE_TEXTURE,&s->active_texture);
    glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&s->texture0);
    glGetIntegerv(GL_VIEWPORT,s->viewport);glGetIntegerv(GL_UNPACK_ALIGNMENT,&s->unpack);
    s->blend=glIsEnabled(GL_BLEND);s->depth=glIsEnabled(GL_DEPTH_TEST);s->stencil=glIsEnabled(GL_STENCIL_TEST);
    s->cull=glIsEnabled(GL_CULL_FACE);s->scissor=glIsEnabled(GL_SCISSOR_TEST);
    glGetBooleanv(GL_COLOR_WRITEMASK,s->color_mask);glGetFloatv(GL_COLOR_CLEAR_VALUE,s->clear_color);
    for(unsigned i=0;i<2;i++){
        Attribute *a=&s->attribute[i];
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&a->enabled);
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_SIZE,&a->size);
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_TYPE,&a->type);
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&a->normalized);
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&a->stride);
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&a->buffer);
        glGetVertexAttribPointerv(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&a->pointer);
    }
}
static void enabled(GLenum cap,GLboolean value){if(value)glEnable(cap);else glDisable(cap);}
static void state_restore(const VideoState *s){
    for(unsigned i=0;i<2;i++){
        const Attribute *a=&s->attribute[i];glBindBuffer(GL_ARRAY_BUFFER,(GLuint)a->buffer);
        glVertexAttribPointer(i,a->size,(GLenum)a->type,(GLboolean)a->normalized,a->stride,a->pointer);
        if(a->enabled)glEnableVertexAttribArray(i);else glDisableVertexAttribArray(i);
    }
    glBindBuffer(GL_ARRAY_BUFFER,(GLuint)s->array_buffer);glUseProgram((GLuint)s->program);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,(GLuint)s->texture0);glActiveTexture((GLenum)s->active_texture);
    glBindFramebuffer(GL_FRAMEBUFFER,(GLuint)s->framebuffer);
    glViewport(s->viewport[0],s->viewport[1],s->viewport[2],s->viewport[3]);
    glPixelStorei(GL_UNPACK_ALIGNMENT,s->unpack);
    enabled(GL_BLEND,s->blend);enabled(GL_DEPTH_TEST,s->depth);enabled(GL_STENCIL_TEST,s->stencil);
    enabled(GL_CULL_FACE,s->cull);enabled(GL_SCISSOR_TEST,s->scissor);
    glColorMask(s->color_mask[0],s->color_mask[1],s->color_mask[2],s->color_mask[3]);
    glClearColor(s->clear_color[0],s->clear_color[1],s->clear_color[2],s->clear_color[3]);
}
static GLuint shader(GLenum kind,const char *source){
    GLuint object=glCreateShader(kind);if(!object)return 0;
    glShaderSource(object,1,&source,NULL);glCompileShader(object);GLint ok=0;glGetShaderiv(object,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[1024];glGetShaderInfoLog(object,sizeof log,NULL,log);fprintf(stderr,"[movie-gles] shader: %s\n",log);glDeleteShader(object);return 0;}
    return object;
}
void d8_video_close(void){
    if(video.owner==SDL_GL_GetCurrentContext()){
        if(video.texture)glDeleteTextures(1,&video.texture);
        if(video.vbo)glDeleteBuffers(1,&video.vbo);
        if(video.program)glDeleteProgram(video.program);
    }
    memset(&video,0,sizeof video);
}
static int initialize(void){
    SDL_GLContext owner=SDL_GL_GetCurrentContext();if(!owner)return -1;
    if(video.owner==owner&&video.program)return 0;
    /* Context teardown normally calls close. Never delete an old context's
     * numeric names in a replacement context, where they could name game data. */
    if(video.owner!=owner)memset(&video,0,sizeof video);
    video.owner=owner;
    const char *vs="attribute vec2 position;attribute vec2 texcoord;varying vec2 uv;void main(){gl_Position=vec4(position,0.,1.);uv=texcoord;}";
    const char *fs="precision mediump float;uniform sampler2D image;varying vec2 uv;void main(){gl_FragColor=vec4(texture2D(image,uv).rgb,1.);}";
    GLuint vertex=shader(GL_VERTEX_SHADER,vs),fragment=shader(GL_FRAGMENT_SHADER,fs);
    if(!vertex||!fragment){if(vertex)glDeleteShader(vertex);if(fragment)glDeleteShader(fragment);return -1;}
    video.program=glCreateProgram();glAttachShader(video.program,vertex);glAttachShader(video.program,fragment);
    glBindAttribLocation(video.program,0,"position");glBindAttribLocation(video.program,1,"texcoord");
    glLinkProgram(video.program);glDeleteShader(vertex);glDeleteShader(fragment);
    GLint ok=0;glGetProgramiv(video.program,GL_LINK_STATUS,&ok);
    if(!ok){char log[1024];glGetProgramInfoLog(video.program,sizeof log,NULL,log);fprintf(stderr,"[movie-gles] link: %s\n",log);d8_video_close();return -1;}
    video.sampler=glGetUniformLocation(video.program,"image");
    glGenBuffers(1,&video.vbo);glGenTextures(1,&video.texture);
    glBindTexture(GL_TEXTURE_2D,video.texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    if(!video.vbo||!video.texture){d8_video_close();return -1;}
    return 0;
}
int d8_video_draw_rgba(SDL_Window *window,const uint8_t *rgba,int width,int height,int present){
    if(!window||!rgba||width<=0||height<=0||width>8192||height>8192||!SDL_GL_GetCurrentContext())return -1;
    int dw=0,dh=0;SDL_GL_GetDrawableSize(window,&dw,&dh);if(dw<=0||dh<=0)return -1;
    GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);if(width>maximum||height>maximum)return -1;
    VideoState saved;state_save(&saved);int result=-1;
    if(initialize())goto done;
    glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,dw,dh);
    glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,video.texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    if(video.width!=width||video.height!=height){glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);video.width=width;video.height=height;}
    else glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    float sx=1,sy=1;double aspect=width/(double)height,output=dw/(double)dh;
    if(aspect>output)sy=(float)(output/aspect);else sx=(float)(aspect/output);
    /* The decoder's first row is the image top; GLES row0 is sampled at v0. */
    const GLfloat quad[]={-sx,-sy,0,1, sx,-sy,1,1, -sx,sy,0,0, sx,sy,1,0};
    glUseProgram(video.program);glUniform1i(video.sampler,0);glBindBuffer(GL_ARRAY_BUFFER,video.vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof quad,quad,GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(GLfloat),(void*)0);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(GLfloat),(void*)(2*sizeof(GLfloat)));
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    if(glGetError()!=GL_NO_ERROR){fprintf(stderr,"[movie-gles] upload/draw rejected\n");goto done;}
    ++video.frames;if(video.frames==1)fprintf(stderr,"[movie-gles] decoded RGBA %dx%d -> drawable %dx%d\n",width,height,dw,dh);
    result=0;
done:
    state_restore(&saved);
    if(!result&&present)SDL_GL_SwapWindow(window);
    return result;
}
