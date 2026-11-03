/* Real GLES host pixel/state test. No MPEG/game/device performance claim. */
#include "d3d8_video.h"
#include <GLES2/gl2.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static GLuint shader(GLenum type,const char *text){
    GLuint s=glCreateShader(type);glShaderSource(s,1,&text,NULL);glCompileShader(s);
    GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);assert(ok);return s;
}
static GLuint program(void){
    GLuint p=glCreateProgram(),v=shader(GL_VERTEX_SHADER,"attribute vec2 p;void main(){gl_Position=vec4(p,0.,1.);}"),f=shader(GL_FRAGMENT_SHADER,"precision mediump float;void main(){gl_FragColor=vec4(1.,0.,1.,1.);}");
    glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);assert(ok);
    glDeleteShader(v);glDeleteShader(f);return p;
}
static int pixel(int x,int y,unsigned r,unsigned g,unsigned b){
    unsigned char p[4]={0};glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);
    return p[0]==r&&p[1]==g&&p[2]==b&&p[3]==255;
}
static void integer(GLenum name,GLint expected){GLint value=-1;glGetIntegerv(name,&value);assert(value==expected);}
int main(void){
    assert(!SDL_Init(SDL_INIT_VIDEO));SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_Window *w=SDL_CreateWindow("movie-test",0,0,8,6,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);
    SDL_GLContext context=SDL_GL_CreateContext(w);assert(context);
    int dw,dh;SDL_GL_GetDrawableSize(w,&dw,&dh);assert(dw==8&&dh==6);
    GLuint p=program(),vbo[2],texture[2],fbo;glGenBuffers(2,vbo);glGenTextures(2,texture);glGenFramebuffers(1,&fbo);
    const GLfloat data[8]={0};
    glBindBuffer(GL_ARRAY_BUFFER,vbo[0]);glBufferData(GL_ARRAY_BUFFER,sizeof data,data,GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,8,(void*)8);glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER,vbo[1]);glBufferData(GL_ARRAY_BUFFER,sizeof data,data,GL_STATIC_DRAW);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_TRUE,12,(void*)4);glDisableVertexAttribArray(1);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture[0]);
    glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,texture[1]);
    glUseProgram(p);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glViewport(1,2,3,4);glPixelStorei(GL_UNPACK_ALIGNMENT,8);
    glEnable(GL_SCISSOR_TEST);glScissor(2,2,1,1);glEnable(GL_BLEND);glEnable(GL_DEPTH_TEST);glEnable(GL_STENCIL_TEST);glEnable(GL_CULL_FACE);
    glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_TRUE);glClearColor(.2f,.3f,.4f,.5f);assert(glGetError()==GL_NO_ERROR);
    /* Top-down rows: red/green above blue/yellow. */
    const uint8_t rgba[]={255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,0,255};
    assert(!d8_video_draw_rgba(w,rgba,2,2,0));
    integer(GL_CURRENT_PROGRAM,(GLint)p);integer(GL_ARRAY_BUFFER_BINDING,(GLint)vbo[1]);integer(GL_FRAMEBUFFER_BINDING,(GLint)fbo);
    integer(GL_ACTIVE_TEXTURE,GL_TEXTURE3);integer(GL_TEXTURE_BINDING_2D,(GLint)texture[1]);integer(GL_UNPACK_ALIGNMENT,8);
    GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);const GLint expected[]={1,2,3,4};assert(!memcmp(viewport,expected,sizeof expected));
    assert(glIsEnabled(GL_BLEND)&&glIsEnabled(GL_DEPTH_TEST)&&glIsEnabled(GL_STENCIL_TEST)&&glIsEnabled(GL_CULL_FACE)&&glIsEnabled(GL_SCISSOR_TEST));
    GLboolean mask[4];glGetBooleanv(GL_COLOR_WRITEMASK,mask);assert(!mask[0]&&mask[1]&&!mask[2]&&mask[3]);
    GLfloat color[4];glGetFloatv(GL_COLOR_CLEAR_VALUE,color);assert(color[0]==.2f&&color[1]==.3f&&color[2]==.4f&&color[3]==.5f);
    for(unsigned i=0;i<2;i++){
        GLint got;void *pointer=NULL;glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&got);assert(got==(GLint)vbo[i]);
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&got);assert(got==(i==0));
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_SIZE,&got);assert(got==(i==0?2:3));
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&got);assert(got==(i==0?GL_FALSE:GL_TRUE));
        glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&got);assert(got==(i==0?8:12));
        glGetVertexAttribPointerv(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&pointer);assert(pointer==(i==0?(void*)8:(void*)4));
    }
    glActiveTexture(GL_TEXTURE0);integer(GL_TEXTURE_BINDING_2D,(GLint)texture[0]);glBindFramebuffer(GL_FRAMEBUFFER,0);
    assert(pixel(0,0,0,0,0)&&pixel(7,5,0,0,0));
    assert(pixel(1,5,255,0,0)&&pixel(6,5,0,255,0)&&pixel(1,0,0,0,255)&&pixel(6,0,255,255,0));
    assert(glGetError()==GL_NO_ERROR);
    /* A second image update exercises TexSubImage2D, not just allocation. */
    const uint8_t reverse[]={0,0,255,255, 255,255,0,255, 255,0,0,255, 0,255,0,255};
    assert(!d8_video_draw_rgba(w,reverse,2,2,0));glBindFramebuffer(GL_FRAMEBUFFER,0);assert(pixel(1,5,0,0,255)&&pixel(6,0,0,255,0));
    assert(!d8_video_draw_rgba(w,reverse,2,2,1));assert(glGetError()==GL_NO_ERROR);
    d8_video_close();glDeleteFramebuffers(1,&fbo);glDeleteTextures(2,texture);glDeleteBuffers(2,vbo);glDeleteProgram(p);
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(w);SDL_Quit();
    puts("Movie GLES2 RGBA orientation, aspect letterbox, texture update, swap and GL state restoration: PASS");
}
