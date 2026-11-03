/* Real MPEG-1 video / MP2 audio playback, on the SDL/main thread.
 * pl_mpeg: c871f2be022ece7ef4f64230b4fb8e1fb9eb6023, MIT.
 * Completion follows decoder EOF plus queued PCM drain, never a duration timer.
 */
#include "movie_decoder.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PL_MPEG_IMPLEMENTATION
#include "../third_party/pl_mpeg/pl_mpeg.h"

extern int thing_video_present_rgba(const uint8_t *,int,int) __attribute__((weak));
/* Weak hooks keep the standalone codec selftest independent of the Win32 HLE.
 * The linked game uses its existing DirectSound mixer, not another SDL device. */
extern int hle_audio_stream_open(int,int) __attribute__((weak));
extern void hle_audio_stream_push(int,const int16_t *,int) __attribute__((weak));
extern int hle_audio_stream_queued(int) __attribute__((weak));
extern void hle_audio_stream_close(int) __attribute__((weak));
extern int hle_audio_stream_pause(int,int) __attribute__((weak));
extern void hle_audio_stream_clear(int) __attribute__((weak));
extern double hle_audio_stream_period(int) __attribute__((weak));

enum MovieState { MOVIE_STOPPED, MOVIE_RUNNING, MOVIE_PAUSED };
struct ThingMovie {
    plm_t *decoder;
    SDL_AudioDeviceID audio;
    int mixer_stream;
    uint8_t *rgba;
    plm_frame_t *pending_frame;
    Uint64 last_counter;
    Uint64 audio_empty_counter;
    double frequency, pending_seconds, position, duration, last_video_time;
    double audio_period;
    enum MovieState state;
    int failed, decoded_eof, resync_audio;
    Uint32 queue_limit_frames;
    ThingMovieStats stats;
};

static int audio_available(const ThingMovie *m){return m->mixer_stream>=0 || m->audio!=0;}
static Uint32 audio_queued_frames(const ThingMovie *m) {
    if(m->mixer_stream>=0) {
        int queued=hle_audio_stream_queued(m->mixer_stream);
        return queued>0?(Uint32)queued:0;
    }
    return m->audio?SDL_GetQueuedAudioSize(m->audio)/(2u*sizeof(float)):0;
}
static int audio_pause(ThingMovie *m,int paused) {
    if(m->mixer_stream>=0)return hle_audio_stream_pause(m->mixer_stream,paused);
    if(m->audio)SDL_PauseAudioDevice(m->audio,paused);
    return 0;
}
static void audio_clear(ThingMovie *m) {
    if(m->mixer_stream>=0)hle_audio_stream_clear(m->mixer_stream);
    else if(m->audio)SDL_ClearQueuedAudio(m->audio);
}

static void movie_fail(ThingMovie *m,const char *reason) {
    if(!m->failed)fprintf(stderr,"[movie] playback error: %s\n",reason);
    m->failed=1;
    (void)audio_pause(m,1);
}
static void video_decoded(plm_t *decoder,plm_frame_t *frame,void *user) {
    (void)decoder;
    ThingMovie *m=user;
    ++m->stats.decoded_video_frames;
    m->last_video_time=frame->time;
    m->pending_frame=frame; /* Present the last decoded frame once per tick. */
}
static void audio_decoded(plm_t *decoder,plm_samples_t *samples,void *user) {
    (void)decoder;
    ThingMovie *m=user;
    ++m->stats.decoded_audio_frames;
    m->stats.decoded_audio_samples+=samples->count;
    for(unsigned i=0;i<samples->count*2u;++i)
        if(fabsf(samples->interleaved[i])>0.000001f)++m->stats.nonzero_audio_samples;
    if(!audio_available(m) || m->failed)return;
    Uint32 queued=audio_queued_frames(m);
    if(samples->count>m->queue_limit_frames || queued>m->queue_limit_frames-samples->count) {
        movie_fail(m,"PCM queue capacity exceeded");
        return;
    }
    if(m->mixer_stream>=0) {
        int16_t pcm[PLM_AUDIO_SAMPLES_PER_FRAME*2];
        for(unsigned i=0;i<samples->count*2u;++i) {
            float f=samples->interleaved[i];
            if(!isfinite(f))f=0;
            f=fmaxf(-1.0f,fminf(1.0f,f));
            int value=(int)(f*32768.0f);
            pcm[i]=(int16_t)(value>32767?32767:value<-32768?-32768:value);
        }
        hle_audio_stream_push(m->mixer_stream,pcm,(int)samples->count);
    }
    else if(SDL_QueueAudio(m->audio,samples->interleaved,samples->count*2u*sizeof(float))) {
        movie_fail(m,SDL_GetError());
        return;
    }
    queued=audio_queued_frames(m)*(Uint32)m->stats.audio_bytes_per_frame;
    if(queued>m->stats.max_queued_audio_bytes)m->stats.max_queued_audio_bytes=queued;
}
static int present_pending(ThingMovie *m) {
    if(!m->pending_frame)return 0;
    if(!thing_video_present_rgba) {
        movie_fail(m,"no native video presentation backend");
        return -1;
    }
    plm_frame_to_rgba(m->pending_frame,m->rgba,m->stats.width*4);
    m->pending_frame=NULL;
    if(thing_video_present_rgba(m->rgba,m->stats.width,m->stats.height)) {
        movie_fail(m,"native video presentation failed");
        return -1;
    }
    ++m->stats.presented_video_frames;
    return 0;
}

ThingMovie *thing_movie_open(const char *path) {
    if(!path || !*path)return NULL;
    plm_t *decoder=plm_create_with_filename(path);
    if(!decoder)return NULL;
    if(!plm_has_headers(decoder) || plm_get_num_video_streams(decoder)<1) {
        fprintf(stderr,"[movie] unsupported/invalid MPEG stream: %s\n",path);
        plm_destroy(decoder);
        return NULL;
    }
    int w=plm_get_width(decoder),h=plm_get_height(decoder);
    double fps=plm_get_framerate(decoder);
    if(w<1 || h<1 || (w&1) || (h&1) || w>4096 || h>4096 || fps<=0 || fps>120 || !isfinite(fps)) {
        fprintf(stderr,"[movie] invalid/unsupported video dimensions or frame rate\n");
        plm_destroy(decoder);
        return NULL;
    }
    ThingMovie *m=calloc(1,sizeof *m);
    if(!m){plm_destroy(decoder);return NULL;}
    m->decoder=decoder;
    m->mixer_stream=-1;
    m->stats.width=w;m->stats.height=h;
    m->rgba=malloc((size_t)w*(size_t)h*4);
    if(!m->rgba){thing_movie_close(m);return NULL;}
    /* pl_mpeg's RGBA converter writes RGB only, preserving the alpha byte. */
    memset(m->rgba,255,(size_t)w*(size_t)h*4);
    m->frequency=(double)SDL_GetPerformanceFrequency();
    if(m->frequency<=0){thing_movie_close(m);return NULL;}
    m->duration=plm_get_duration(decoder);
    plm_set_loop(decoder,0);
    plm_set_video_decode_callback(decoder,video_decoded,m);
    if(plm_get_num_audio_streams(decoder)>0) {
        m->stats.samplerate=plm_get_samplerate(decoder);
        plm_set_audio_decode_callback(decoder,audio_decoded,m);
        plm_set_audio_lead_time(decoder,0.08);
        int shared_hooks=hle_audio_stream_open && hle_audio_stream_push && hle_audio_stream_queued &&
            hle_audio_stream_close && hle_audio_stream_pause && hle_audio_stream_clear;
        if(shared_hooks) {
            m->mixer_stream=hle_audio_stream_open(m->stats.samplerate,2);
            if(m->mixer_stream<0)fprintf(stderr,"[movie] shared audio mixer unavailable, MP2 decoded muted\n");
            else {
                if(audio_pause(m,1))movie_fail(m,"shared audio pause failed");
                m->stats.audio_backend=1;m->stats.audio_bytes_per_frame=2*sizeof(int16_t);
                m->audio_period=hle_audio_stream_period?hle_audio_stream_period(m->mixer_stream):1024.0/44100.0;
            }
        }
        else if(!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            fprintf(stderr,"[movie] audio initialization failed, MP2 decoded muted: %s\n",SDL_GetError());
        } else {
            SDL_AudioSpec want,got;SDL_zero(want);SDL_zero(got);
            want.freq=m->stats.samplerate;want.format=AUDIO_F32SYS;
            want.channels=2;want.samples=1024;
            m->audio=SDL_OpenAudioDevice(NULL,0,&want,&got,0);
            if(!m->audio)fprintf(stderr,"[movie] audio output unavailable, MP2 decoded muted: %s\n",SDL_GetError());
            else if(got.freq!=want.freq || got.format!=want.format || got.channels!=2) {
                fprintf(stderr,"[movie] unsupported audio output format, MP2 decoded muted\n");
                SDL_CloseAudioDevice(m->audio);m->audio=0;
            }
            else {
                m->audio_period=(double)got.samples/got.freq;
                m->stats.audio_backend=2;m->stats.audio_bytes_per_frame=2*sizeof(float);
            }
        }
    } else plm_set_audio_enabled(decoder,0);
    m->stats.audio_output_available=audio_available(m);
    m->queue_limit_frames=(Uint32)(m->stats.samplerate>0?m->stats.samplerate:44100)/2u;
    fprintf(stderr,"[movie] opened %s: %dx%d %.3f FPS, MP2 %d Hz, audio=%s, duration-estimate=%.3f\n",
        path,w,h,fps,m->stats.samplerate,m->mixer_stream>=0?"shared-mixer":m->audio?"SDL-queued":"unavailable",m->duration);
    return m;
}
void thing_movie_close(ThingMovie *m) {
    if(!m)return;
    if(m->mixer_stream>=0)hle_audio_stream_close(m->mixer_stream);
    if(m->audio){SDL_ClearQueuedAudio(m->audio);SDL_CloseAudioDevice(m->audio);}
    if(m->decoder)plm_destroy(m->decoder);
    free(m->rgba);free(m);
    /* Never SDL_QuitSubSystem here: DirectSound may own another audio device. */
}
int thing_movie_run(ThingMovie *m) {
    if(!m || m->failed)return -1;
    if(!thing_video_present_rgba){movie_fail(m,"no native video presentation backend");return -1;}
    if(m->resync_audio) {
        double position=m->position;
        m->resync_audio=0;
        if(thing_movie_seek(m,position))return -1;
    }
    m->last_counter=SDL_GetPerformanceCounter();
    m->audio_empty_counter=0;
    m->state=MOVIE_RUNNING;
    if(audio_pause(m,0)){movie_fail(m,"audio resume failed");return -1;}
    return 0;
}
void thing_movie_pause(ThingMovie *m) {
    if(!m || m->failed)return;
    m->state=MOVIE_PAUSED;
    if(audio_pause(m,1))movie_fail(m,"audio pause failed");
}
void thing_movie_stop(ThingMovie *m) {
    if(!m)return;
    m->state=MOVIE_STOPPED;m->pending_seconds=0;m->resync_audio=1;
    if(audio_pause(m,1))movie_fail(m,"audio stop failed");
    audio_clear(m);
}
int thing_movie_seek(ThingMovie *m,double seconds) {
    if(!m || m->failed || !isfinite(seconds))return -1;
    if(seconds<0)seconds=0;
    if(m->duration>0 && seconds>=m->duration) {
        double frame=1.0/plm_get_framerate(m->decoder);
        seconds=fmax(0,m->duration-frame);
    }
    if(audio_pause(m,1)){movie_fail(m,"audio seek pause failed");return -1;}
    audio_clear(m);
    m->pending_frame=NULL;m->pending_seconds=0;m->decoded_eof=0;m->resync_audio=0;m->audio_empty_counter=0;
    if(seconds==0)plm_rewind(m->decoder);
    else if(!plm_seek(m->decoder,seconds,1)){movie_fail(m,"MPEG seek failed");return -1;}
    m->position=plm_get_time(m->decoder);
    m->last_counter=SDL_GetPerformanceCounter();
    if(present_pending(m))return -1;
    if(m->state==MOVIE_RUNNING && audio_pause(m,0)){movie_fail(m,"audio seek resume failed");return -1;}
    return 0;
}
double thing_movie_duration(const ThingMovie *m){return m?m->duration:0;}
double thing_movie_position(const ThingMovie *m){return m?m->position:0;}
int thing_movie_tick(ThingMovie *m) {
    if(!m || m->failed)return -1;
    if(m->state!=MOVIE_RUNNING)return 0;
    Uint64 now=SDL_GetPerformanceCounter();
    double elapsed=(double)(now-m->last_counter)/m->frequency;
    m->last_counter=now;
    m->pending_seconds+=elapsed;
    if(!m->decoded_eof) {
        double step=fmin(m->pending_seconds,0.05);
        if(audio_available(m)) {
            Uint32 queued=audio_queued_frames(m);
            double free_seconds=queued<m->queue_limit_frames?
                (double)(m->queue_limit_frames-queued)/m->stats.samplerate:0;
            /* Leave room for lead time and the two frame-boundary round-ups. */
            double safe=free_seconds-0.08-2.0*PLM_AUDIO_SAMPLES_PER_FRAME/m->stats.samplerate;
            step=fmin(step,fmax(0,safe));
        }
        if(step>0) {
            plm_decode(m->decoder,step);
            m->position=plm_get_time(m->decoder);
            m->pending_seconds-=step;
            if(present_pending(m))return -1;
            if(plm_has_ended(m->decoder)) {
                if(!m->stats.decoded_video_frames){movie_fail(m,"MPEG ended without a decoded video frame");return -1;}
                m->decoded_eof=1;
            } else if(m->position-m->last_video_time>5.0) {
                movie_fail(m,"video decoder made no progress");return -1;
            }
        }
    }
    if(m->failed)return -1;
    if(m->decoded_eof) {
        if(!audio_available(m))return 1;
        if(audio_queued_frames(m)!=0)m->audio_empty_counter=0;
        else {
            /* SDL's queue excludes PCM already submitted to the audio driver.
             * Preserve two obtained device periods after observing queue-empty
             * so immediate graph destruction cannot chop the final callback. */
            if(!m->audio_empty_counter)m->audio_empty_counter=now;
            if((double)(now-m->audio_empty_counter)/m->frequency>=2.0*m->audio_period)return 1;
        }
    }
    return 0;
}
int thing_movie_get_stats(const ThingMovie *m,ThingMovieStats *stats) {
    if(!m || !stats)return -1;
    *stats=m->stats;
    stats->queued_audio_bytes=audio_queued_frames(m)*(Uint32)m->stats.audio_bytes_per_frame;
    stats->paused=m->state==MOVIE_PAUSED;stats->stopped=m->state==MOVIE_STOPPED;
    stats->decoded_eof=m->decoded_eof;stats->failed=m->failed;
    return 0;
}
