#ifndef THING_MOVIE_DECODER_H
#define THING_MOVIE_DECODER_H
#include <stdint.h>

/* Main-thread-only MPEG-1/MP2 playback. Open parses real stream headers without
 * starting playback. Run/seek return 0 on success, -1 on error. Pause freezes
 * the playback clock and queued audio. Stop retains position, clears queued
 * audio, and Run resynchronizes from that position. Seek resets real EOS.
 * Tick: 0 running/paused/stopped, 1 only decoded EOF AND drained audio, -1 error.
 * A renderer implements thing_video_present_rgba (0 success/-1 failure).
 */
typedef struct ThingMovie ThingMovie;
ThingMovie *thing_movie_open(const char *host_path);
void thing_movie_close(ThingMovie *movie);
int thing_movie_run(ThingMovie *movie);
void thing_movie_pause(ThingMovie *movie);
void thing_movie_stop(ThingMovie *movie);
int thing_movie_seek(ThingMovie *movie, double seconds);
double thing_movie_duration(const ThingMovie *movie);
double thing_movie_position(const ThingMovie *movie);
int thing_movie_tick(ThingMovie *movie);

/* Diagnostics count real decoded/presented data. Audio samples are stereo
 * sample pairs; output availability does not assert physical audible output. */
typedef struct {
    uint64_t decoded_video_frames, presented_video_frames;
    uint64_t decoded_audio_frames, decoded_audio_samples, nonzero_audio_samples;
    uint32_t queued_audio_bytes, max_queued_audio_bytes;
    int width, height, samplerate, audio_output_available;
    int audio_backend; /* 0 unavailable, 1 shared DirectSound mixer, 2 SDL fallback */
    int audio_bytes_per_frame;
    int paused, stopped, decoded_eof, failed;
} ThingMovieStats;
int thing_movie_get_stats(const ThingMovie *movie, ThingMovieStats *stats);

int thing_video_present_rgba(const uint8_t *rgba, int width, int height);
#endif
