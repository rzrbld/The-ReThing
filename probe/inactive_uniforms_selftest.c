/* Uniform-location semantics, cache independence and cache/GL suppression.
 * GL image equivalence is tested separately using the existing GLES suite. */
#include <GLES2/gl2.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned calls[7];
static void track1i(GLint l,GLint a){assert(l>=0);(void)a;calls[0]++;}
static void track4i(GLint l,GLint a,GLint b,GLint c,GLint d){assert(l>=0);(void)a;(void)b;(void)c;(void)d;calls[1]++;}
static void track1f(GLint l,GLfloat a){assert(l>=0);(void)a;calls[2]++;}
static void track2f(GLint l,GLfloat a,GLfloat b){assert(l>=0);(void)a;(void)b;calls[3]++;}
static void track3f(GLint l,GLfloat a,GLfloat b,GLfloat c){assert(l>=0);(void)a;(void)b;(void)c;calls[4]++;}
static void track4f(GLint l,GLfloat a,GLfloat b,GLfloat c,GLfloat d){assert(l>=0);(void)a;(void)b;(void)c;(void)d;calls[5]++;}
static void track4fv(GLint l,GLsizei n,const GLfloat *v){assert(l>=0&&n==1&&v);calls[6]++;}
#define glUniform1i track1i
#define glUniform4i track4i
#define glUniform1f track1f
#define glUniform2f track2f
#define glUniform3f track3f
#define glUniform4f track4f
#define glUniform4fv track4fv
#include THING_TEST_RENDERER_SOURCE
static void all(D8SmallUniformCache *c,GLint loc,float x){
 small1i(c,0,loc,(int)x);small4i(c,1,loc,(int)x,2,3,4);small1f(c,2,loc,x);
 small2f(c,3,loc,x,2);small3f(c,4,loc,x,2,3);small4f(c,5,loc,x,2,3,4);
 float v[4]={x,2,3,4};small4fv(c,6,loc,v);
}
int main(void){
 D8SmallUniformCache a={0},b={0},before=a;
 all(&a,-1,1);assert(!memcmp(&a,&before,sizeof a));for(unsigned i=0;i<7;i++)assert(!calls[i]);
 /* Inactive uniform must not touch payload, even when there is no valid data. */
 small4fv(&a,6,-1,NULL);assert(!memcmp(&a,&before,sizeof a));
 all(&a,17,1);for(unsigned i=0;i<7;i++)assert(calls[i]==1);before=a;
 all(&a,-1,42);assert(!memcmp(&a,&before,sizeof a));for(unsigned i=0;i<7;i++)assert(calls[i]==1);
 all(&a,17,1);for(unsigned i=0;i<7;i++)assert(calls[i]==1);
 all(&a,17,2);for(unsigned i=0;i<7;i++)assert(calls[i]==2);
 all(&b,23,2);for(unsigned i=0;i<7;i++)assert(calls[i]==3);
 all(&a,17,2);for(unsigned i=0;i<7;i++)assert(calls[i]==3);
 /* A location can be absent in one cache/program and active in another. */
 memset(&b,0,sizeof b);before=b;all(&b,-1,3);assert(!memcmp(&b,&before,sizeof b));
 all(&b,23,3);for(unsigned i=0;i<7;i++)assert(calls[i]==4);
 puts("PASS: all seven inactive uniforms leave cache/payload/GL untouched; active changes upload exactly once; unchanged values and program-local caches behave correctly");
}
