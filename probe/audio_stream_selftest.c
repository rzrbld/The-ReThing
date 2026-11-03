/* Decode real supplied MPEG audio into the existing SDL DirectSound mixer.
 * The dummy driver runs callbacks/cursors; this does not test audible output.
 */
#include <SDL.h>
#include <math.h>
#include "guest_test_harness.h"
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

static int decoded_pcm(const char *path,int16_t *pcm,int *rate){
    plm_t *decoder=plm_create_with_filename(path);assert(decoder);plm_set_video_enabled(decoder,0);
    assert(plm_has_headers(decoder)&&plm_get_num_audio_streams(decoder)>0);
    *rate=plm_get_samplerate(decoder);assert(*rate>0);
    int total=0,started=0;
    for(int block=0;block<512&&total<8*PLM_AUDIO_SAMPLES_PER_FRAME;block++){
        plm_samples_t *samples=plm_decode_audio(decoder);if(!samples)break;
        for(unsigned i=0;i<samples->count*2;i++)if(fabsf(samples->interleaved[i])>.0001f)started=1;
        if(!started)continue;
        for(unsigned i=0;i<samples->count*2;i++){
            float v=samples->interleaved[i]*32767;
            pcm[total*2+i]=(int16_t)(v>32767?32767:v< -32768?-32768:v);
        }
        total+=(int)samples->count;
    }
    plm_destroy(decoder);assert(started&&total>0);return total;
}
static uint32_t buffer(uint32_t ds,uint32_t *cursor){
    uint32_t desc=hle_scratch(36),out=hle_scratch(4),format=hle_scratch(18);
    MEM16(format)=1;MEM16(format+2)=1;MEM32(format+4)=22050;MEM32(format+8)=44100;
    MEM16(format+12)=2;MEM16(format+14)=16;MEM32(desc)=36;MEM32(desc+4)=0x80;MEM32(desc+8)=16384;MEM32(desc+16)=format;
    uint32_t create[]={desc,out,0};assert(!invoke(ds,3,3,create));uint32_t object=MEM32(out);assert(object);
    uint32_t locked=hle_scratch(16),lock[]={0,16384,locked,locked+4,locked+8,locked+12,0};assert(!invoke(object,11,7,lock));
    assert(MEM32(locked)&&MEM32(locked+4)==16384);int16_t *data=hle_ptr(MEM32(locked));
    for(unsigned i=0;i<8192;i++)data[i]=(int16_t)(6000*sin(i*.1));
    uint32_t unlock[]={MEM32(locked),16384,0,0};assert(!invoke(object,19,4,unlock));uint32_t play[]={0,0,1};assert(!invoke(object,12,3,play));
    *cursor=hle_scratch(8);return object;
}
static uint32_t cursor(uint32_t object,uint32_t at){uint32_t get[]={at,at+4};assert(!invoke(object,4,2,get));return MEM32(at);}
int main(int argc,char **argv){
    assert(argc==2);int16_t pcm[8*PLM_AUDIO_SAMPLES_PER_FRAME*2];int rate;
    int frames=decoded_pcm(argv[1],pcm,&rate);
    g_mem_base=(ptrdiff_t)calloc(1,0x400000);assert(g_mem_base);SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    win32hle_register(win32hle_dsound);uint32_t out=hle_scratch(4),args[]={0,out,0};
    assert(!hle_call_guest(hle_resolve("DirectSoundCreate"),3,args));uint32_t ds=MEM32(out),at;assert(ds);
    uint32_t game=buffer(ds,&at);
    int stream=hle_audio_stream_open(rate,2);assert(stream>=0);assert(!hle_audio_stream_pause(stream,1));
    double period=hle_audio_stream_period(stream);assert(period>0&&period<1);
    hle_audio_stream_push(stream,pcm,frames);assert(hle_audio_stream_queued(stream)==frames);
    uint32_t before=cursor(game,at);SDL_Delay(100);
    assert(hle_audio_stream_queued(stream)==frames);assert(cursor(game,at)!=before);
    assert(!hle_audio_stream_pause(stream,0));SDL_Delay(60);assert(hle_audio_stream_queued(stream)<frames);
    assert(!hle_audio_stream_pause(stream,1));int frozen=hle_audio_stream_queued(stream);SDL_Delay(60);assert(hle_audio_stream_queued(stream)==frozen);
    hle_audio_stream_clear(stream);assert(!hle_audio_stream_queued(stream));
    hle_audio_stream_push(stream,pcm,frames);assert(hle_audio_stream_queued(stream)==frames);
    SDL_Delay(60);assert(hle_audio_stream_queued(stream)==frames); /* clear preserves pause */
    assert(!hle_audio_stream_pause(stream,0));
    Uint32 deadline=SDL_GetTicks()+2000;while(hle_audio_stream_queued(stream)&&SDL_GetTicks()<deadline)SDL_Delay(5);
    assert(!hle_audio_stream_queued(stream));hle_audio_stream_close(stream);
    assert(hle_audio_stream_pause(stream,1)==-1);assert(hle_audio_stream_period(stream)==0);
    hle_audio_stream_push(stream,pcm,frames);hle_audio_stream_clear(stream);assert(!hle_audio_stream_queued(stream));
    before=cursor(game,at);SDL_Delay(100);assert(cursor(game,at)!=before);
    uint32_t status[]={out};assert(!invoke(game,9,1,status));assert(MEM32(out)&1);
    assert(invoke(game,2,0,NULL)==0);assert(invoke(ds,2,0,NULL)==0);SDL_Quit();free((void*)g_mem_base);
    printf("Decoded MPEG PCM %dframes/%dHz: shared mixer queue/drain, pause freeze, clear, callback period and surviving DirectSound game buffer: PASS\n",frames,rate);
}
