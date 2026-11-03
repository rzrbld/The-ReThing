/* Headless real-data decoder checks. SDL dummy output is NOT audible output.
 * Full source decode runs without wall-clock pacing; playback state/drain tests
 * use the production SDL performance clock and actual dummy audio queue.
 */
#include "movie_decoder.h"
#include <SDL.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../third_party/pl_mpeg/pl_mpeg.h"
extern int hle_audio_stream_open(int,int) __attribute__((weak));
extern void hle_audio_stream_push(int,const int16_t *,int) __attribute__((weak));
extern int hle_audio_stream_queued(int) __attribute__((weak));
extern int hle_audio_stream_pause(int,int) __attribute__((weak));
extern void hle_audio_stream_close(int) __attribute__((weak));

static uint64_t presented,rgba_hash=1469598103934665603ull;
int thing_video_present_rgba(const uint8_t *rgba,int width,int height) {
    assert(rgba && width>0 && height>0);
    ++presented;
    for(size_t pixel=0;pixel<(size_t)width*(size_t)height;pixel+=97) {
        assert(rgba[pixel*4+3]==255);
        rgba_hash^=(uint64_t)rgba[pixel*4] | (uint64_t)rgba[pixel*4+1]<<8 | (uint64_t)rgba[pixel*4+2]<<16;
        rgba_hash*=1099511628211ull;
    }
    return 0;
}
typedef struct {
    uint64_t frames,audio_frames,samples,nonzero;
    uint8_t *rgba;
    double last_video,last_audio;
} FullDecode;
static void full_video(plm_t *decoder,plm_frame_t *frame,void *user) {
    (void)decoder;FullDecode *d=user;
    ++d->frames;d->last_video=frame->time;
    plm_frame_to_rgba(frame,d->rgba,(int)frame->width*4);
    assert(!thing_video_present_rgba(d->rgba,(int)frame->width,(int)frame->height));
}
static void full_audio(plm_t *decoder,plm_samples_t *samples,void *user) {
    (void)decoder;FullDecode *d=user;
    ++d->audio_frames;d->samples+=samples->count;d->last_audio=samples->time;
    for(unsigned i=0;i<samples->count*2u;++i)
        if(fabsf(samples->interleaved[i])>0.000001f)++d->nonzero;
}

int main(int argc,char **argv) {
    assert(argc==2);
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    assert(SDL_Init(SDL_INIT_TIMER|SDL_INIT_AUDIO)==0);
    assert(thing_movie_open("/nonexistent/thing-movie.mpg")==NULL);
    char bad[]="/tmp/thing-movie-invalid-XXXXXX";
    int fd=mkstemp(bad);assert(fd>=0);assert(write(fd,"not an MPEG stream",18)==18);close(fd);
    assert(thing_movie_open(bad)==NULL);unlink(bad);

    /* Decode the entire actual source, not synthetic frames or a timer. */
    plm_t *full=plm_create_with_filename(argv[1]);assert(full && plm_has_headers(full));
    FullDecode data={0};
    int w=plm_get_width(full),h=plm_get_height(full);
    data.rgba=malloc((size_t)w*(size_t)h*4);assert(data.rgba);
    memset(data.rgba,255,(size_t)w*(size_t)h*4);
    plm_set_video_decode_callback(full,full_video,&data);
    plm_set_audio_decode_callback(full,full_audio,&data);
    for(int iteration=0;!plm_has_ended(full) && iteration<3000;++iteration)plm_decode(full,1.0/30.0);
    assert(plm_has_ended(full));
    assert(data.frames>100 && data.audio_frames>100 && data.nonzero>0);
    printf("FULL_DECODE frames=%llu audio_frames=%llu sample_pairs=%llu nonzero_channel_samples=%llu last_video=%.6f last_audio=%.6f rgba_hash=%016llx\n",
        (unsigned long long)data.frames,(unsigned long long)data.audio_frames,(unsigned long long)data.samples,
        (unsigned long long)data.nonzero,data.last_video,data.last_audio,(unsigned long long)rgba_hash);
    free(data.rgba);plm_destroy(full);

    ThingMovie *movie=thing_movie_open(argv[1]);assert(movie);
    ThingMovieStats stats;assert(!thing_movie_get_stats(movie,&stats));
    assert(stats.stopped && stats.decoded_video_frames==0 && stats.audio_output_available);
    int shared_audio=stats.audio_backend==1;
    double duration=thing_movie_duration(movie);assert(duration>1 && isfinite(duration));
    assert(thing_movie_tick(movie)==0);
    assert(!thing_movie_run(movie));
    Uint64 start=SDL_GetTicks64();
    while(SDL_GetTicks64()-start<220){assert(thing_movie_tick(movie)==0);SDL_Delay(2);}
    assert(!thing_movie_get_stats(movie,&stats));
    assert(stats.decoded_video_frames>0 && stats.presented_video_frames>0);
    assert(stats.decoded_audio_samples>0 && stats.max_queued_audio_bytes>0);
    assert(stats.max_queued_audio_bytes<=(uint32_t)stats.samplerate*(uint32_t)stats.audio_bytes_per_frame/2u);
    thing_movie_pause(movie);
    double position=thing_movie_position(movie);
    assert(!thing_movie_get_stats(movie,&stats));Uint32 queued=stats.queued_audio_bytes;
    SDL_Delay(80);assert(thing_movie_tick(movie)==0);
    assert(thing_movie_position(movie)==position);
    assert(!thing_movie_get_stats(movie,&stats));assert(stats.paused && stats.queued_audio_bytes==queued);
    assert(!thing_movie_run(movie));
    thing_movie_stop(movie);position=thing_movie_position(movie);
    SDL_Delay(20);assert(thing_movie_tick(movie)==0);assert(thing_movie_position(movie)==position);
    assert(!thing_movie_get_stats(movie,&stats));assert(stats.stopped && stats.queued_audio_bytes==0);
    assert(!thing_movie_run(movie));
    assert(!thing_movie_seek(movie,duration-0.15));
    assert(thing_movie_position(movie)>=duration-0.2);
    int result=0;start=SDL_GetTicks64();
    while(result==0 && SDL_GetTicks64()-start<3000){result=thing_movie_tick(movie);SDL_Delay(2);}
    assert(result==1);assert(!thing_movie_get_stats(movie,&stats));
    assert(stats.decoded_eof && stats.queued_audio_bytes==0 && !stats.failed);
    printf("REAL_CLOCK_PLAYBACK frames=%llu presented=%llu audio_frames=%llu sample_pairs=%llu backend=%s max_queue=%u duration_estimate=%.6f position=%.6f EOF=decoded+PCM-drained\n",
        (unsigned long long)stats.decoded_video_frames,(unsigned long long)stats.presented_video_frames,
        (unsigned long long)stats.decoded_audio_frames,(unsigned long long)stats.decoded_audio_samples,
        shared_audio?"shared-mixer":"SDL-fallback",stats.max_queued_audio_bytes,duration,thing_movie_position(movie));
    assert(!thing_movie_seek(movie,0));assert(thing_movie_tick(movie)==0);
    thing_movie_close(movie);

    /* Closing a movie must not terminate an independently owned audio device. */
    if(shared_audio) {
        int owner=hle_audio_stream_open(44100,2);assert(owner>=0);
        assert(!hle_audio_stream_pause(owner,1));
        int16_t pcm[128];for(unsigned i=0;i<128;++i)pcm[i]=(int16_t)(i*50);
        hle_audio_stream_push(owner,pcm,64);assert(hle_audio_stream_queued(owner)==64);
        movie=thing_movie_open(argv[1]);assert(movie);thing_movie_close(movie);
        assert(SDL_WasInit(SDL_INIT_AUDIO));assert(hle_audio_stream_queued(owner)==64);
        hle_audio_stream_close(owner);
    } else {
        SDL_AudioSpec want;SDL_zero(want);want.freq=44100;want.channels=2;want.format=AUDIO_F32SYS;want.samples=1024;
        SDL_AudioDeviceID owner=SDL_OpenAudioDevice(NULL,0,&want,NULL,0);assert(owner);
        movie=thing_movie_open(argv[1]);assert(movie);thing_movie_close(movie);
        assert(SDL_WasInit(SDL_INIT_AUDIO));assert(SDL_GetAudioDeviceStatus(owner)==SDL_AUDIO_PAUSED);
        SDL_CloseAudioDevice(owner);
    }
    SDL_Quit();
    puts("PASS: real full-file decode; bounded PCM queue; real-clock pause/stop/seek; decoded/drained EOS; invalid input; independent audio ownership. Dummy audio is not physical output evidence.");
    return 0;
}
