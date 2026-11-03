#!/usr/bin/env python3
"""Forwarding/window/errno/thread tests; fake GL does not measure driver cost."""
import csv
import os
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='thing-glcost-') as tmp:
    p = Path(tmp)
    (p/'provider.c').write_text(r'''
#include <GLES2/gl2.h>
#include <errno.h>
int reads;
void glBufferData(GLenum target,GLsizeiptr size,const void *data,GLenum usage) {
    if(!target || size<0 || !data || usage!=GL_STREAM_DRAW) __builtin_trap();
    errno=ERANGE;
}
void glEnable(GLenum cap) { if(cap!=GL_BLEND) __builtin_trap(); }
void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if(mode!=GL_TRIANGLES || first!=3 || count!=6) __builtin_trap();
    glEnable(GL_BLEND); errno=EDOM;
}
GLenum glGetError(void) { ++reads; return GL_INVALID_ENUM; }
''')
    (p/'test.c').write_text(r'''
#include <GLES2/gl2.h>
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
extern int reads;
static int threaded;
int ds_rt_active(void) { return threaded; }
static void draw(void) { glDrawArrays(GL_TRIANGLES,3,6); assert(errno==EDOM); }
static void *worker(void *unused) { (void)unused; for(int i=0;i<1000;i++)draw(); return 0; }
int main(void) {
    int (*begin)(unsigned)=dlsym(RTLD_DEFAULT,"ds_gl_cost_begin");
    void (*present)(void)=dlsym(RTLD_DEFAULT,"ds_gl_cost_present");
    void (*flush)(void)=dlsym(RTLD_DEFAULT,"ds_gl_cost_flush");
    assert(begin && present && flush);
    if(getenv("BUFFER_TEST")) {
        assert(begin(1));
        const char data[64]={0};
        for(int i=0;i<1000;i++) {
            glBufferData(GL_ARRAY_BUFFER,32,data,GL_STREAM_DRAW);
            assert(errno==ERANGE);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,32,data,GL_STREAM_DRAW);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,33,data,GL_STREAM_DRAW);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,0,data,GL_STREAM_DRAW);
        }
        present();return 0;
    }
    if(getenv("AUTO_TEST")) {
        draw(); present(); draw(); present();
        for(int i=0;i<120;i++) { draw(); present(); }
        draw(); flush(); return 0;
    }
    draw(); assert(glGetError()==GL_INVALID_ENUM && reads==1);
    threaded=1; assert(!begin(2)); threaded=0;
    assert(!begin(0)); assert(begin(2));
    for(int i=0;i<16000;i++)draw();
    present();
    pthread_t t; assert(!pthread_create(&t,0,worker,0)); assert(!pthread_join(t,0));
    for(int i=0;i<16000;i++)draw();
    assert(glGetError()==GL_INVALID_ENUM && reads==2);
    present(); draw(); present(); flush();
    assert(begin(10)); draw(); present(); flush(); flush();
    assert(reads==2);
    return 0;
}
''')
    arm = '--aarch64' in sys.argv
    def run(args, **kw):
        if arm and args[0]=='cc':
            args=['docker','run','--rm','--network','none','--read-only','--tmpfs','/tmp/tool',
                  '--user',f'{os.getuid()}:{os.getgid()}', '--workdir',str(p), '-e','TMPDIR=/tmp/tool',
                  '-v',f'{ROOT}:{ROOT}:ro','-v',f'{p}:{p}:rw',
                  'sha256:5c811c6c025a934595e16a49bd9c8554eb7c7b33cfdd4d2fcc01672fd6f85e49',
                  'aarch64-linux-gnu-gcc',*args[1:]]
        elif arm and args[0]==str(p/'test'):
            env=kw['env'].copy()
            preload=env.pop('LD_PRELOAD')
            root=ROOT/'build/arm64-qemu-sysroot'
            args=[str(ROOT/'build/tools/qemu-user/usr/bin/qemu-aarch64'),
                  str(root/'lib/aarch64-linux-gnu/ld-linux-aarch64.so.1'),
                  '--library-path',f'{p}:{root}/lib/aarch64-linux-gnu:{root}/usr/lib/aarch64-linux-gnu',
                  '--preload',preload,*args]
            kw['env']=env
        subprocess.run(args, check=True, **kw)
    run(['cc','-shared','-fPIC',str(p/'provider.c'),'-o',str(p/'libfake.so')])
    run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-shared','-fPIC',
         str(ROOT/'probe/rt/gl_cost_preload.c'),'-ldl','-o',str(p/'libcost.so')])
    run(['cc',str(p/'test.c'),'-rdynamic','-L'+tmp,'-Wl,-rpath,'+tmp,
         '-lfake','-ldl','-pthread','-o',str(p/'test')])
    run([str(p/'test')],env={**os.environ,'LD_PRELOAD':str(p/'libcost.so'),
                            'THING_GLCOST_FILE':str(p/'cost.csv')})
    rows=list(csv.DictReader((p/'cost.csv').open()))
    assert len(rows)==3, rows
    draws=[r for r in rows if r['call']=='glDrawArrays']
    assert [(r['window'],r['frames'],r['calls']) for r in draws]==[
        ('1','2','32000'),('2','1','1')], rows
    assert 1500<int(draws[0]['samples'])<2500, rows
    assert int(draws[0]['sampled_wall_ns'])>=int(draws[0]['sampled_cpu_ns'])>0
    assert rows[1]['call']=='glGetError' and rows[1]['calls']=='1'
    run([str(p/'test')],env={**os.environ,'LD_PRELOAD':str(p/'libcost.so'),
        'THING_GLCOST_FILE':str(p/'auto.csv'), 'AUTO_TEST':'1',
        'THING_GLCOST_START_FRAME':'2'})
    auto=list(csv.DictReader((p/'auto.csv').open()))
    assert len(auto)==1 and auto[0]['calls']=='120' and auto[0]['frames']=='120', auto
    run([str(p/'test')],env={**os.environ,'LD_PRELOAD':str(p/'libcost.so'),
        'THING_GLCOST_FILE':str(p/'buffer.csv'), 'BUFFER_TEST':'1'})
    buffer=list(csv.DictReader((p/'buffer.csv').open()))
    assert {r['call']:int(r['calls']) for r in buffer}=={
        'glBufferData.vertex':1000, 'glBufferData.index_le32':1000,
        'glBufferData.index_gt32_or_zero':2000}, buffer
    print('PASS: vertex/small-index/large-or-zero index classification and errno.')
    print('PASS: recursion exclusion and automatic frame-triggered window.')
    print('PASS: exact forwarding, return values, error reads, errno, bounded windows,')
    print('inactive and foreign-thread exclusion, threaded-mode refusal, CSV output.')
    print('Timing is synthetic, not performance evidence.')
