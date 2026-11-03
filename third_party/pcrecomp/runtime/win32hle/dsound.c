/*
 * dsound.c - DirectSound on SDL2 audio: IDirectSound and IDirectSoundBuffer,
 * with a software mixer.
 *
 * A secondary buffer is guest memory the game Locks and writes, as it would
 * be on Windows; the mixer (SDL's audio thread) reads it at the buffer's
 * frequency, volume and pan and moves its play cursor in real time, so a
 * game that streams into a looping buffer by watching GetCurrentPosition
 * keeps doing so. The primary buffer is only its format: everything is mixed
 * at the device's rate. One mutex covers the buffers' state, between the
 * mixer and the shims.
 *
 * With no audio device (SDL_AUDIODRIVER=dummy, a headless box) SDL's dummy
 * driver still runs the callback in real time, so the cursors still move.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <SDL2/SDL.h>
#include "win32hle.h"

#define DS_OK               0u
#define DSERR_INVALIDPARAM  0x80070057u
#define DSERR_NODRIVER      0x88780078u
#define DSERR_INVALIDCALL   0x88780032u
#define DSERR_PRIOLEVELNEEDED 0x88780046u
#define DSERR_BUFFERLOST    0x88780096u
#define DSBCAPS_PRIMARYBUFFER 0x00000001u
#define DSBPLAY_LOOPING     0x00000001u
#define DSBSTATUS_PLAYING   0x00000001u
#define DSBSTATUS_LOOPING   0x00000004u

#define OUT_RATE 44100
#define DS_NOTIFY_MAX 100000u
typedef struct { uint32_t offset,event; int pending; } dsnotify;

typedef struct {
    uint8_t *data;              /* the samples, guest-visible */
    int *data_refs;             /* shared with duplicates */
    uint32_t bytes, flags;
    int channels, bits, rate, block;
    int playing, looping, primary;
    double pos;                 /* the play cursor, in frames */
    int volume, pan;            /* hundredths of a dB; -10000..10000 */
    float gl, gr;
    dsnotify *notifications;
    uint32_t notification_count;
} dsbuf;

static pthread_mutex_t g_mix = PTHREAD_MUTEX_INITIALIZER;
static dsbuf **g_buf;
static uint32_t g_buf_capacity,g_buf_live,g_buf_peak,g_buf_created,g_buf_released;
static SDL_AudioDeviceID g_dev;
static int g_audio_ok = -1;
static double g_audio_period;
static int g_notify_pending;
static float g_master = 1.0f;
void hle_dsound_set_master(float gain) { g_master = gain; }

static void gains(dsbuf *b) {
    float v = b->volume <= -10000 ? 0.0f : powf(10.0f, (float)b->volume / 2000.0f);
    float l = 1.0f, r = 1.0f;
    if (b->pan < 0) r = b->pan <= -10000 ? 0.0f : powf(10.0f, (float)b->pan / 2000.0f);
    if (b->pan > 0) l = b->pan >= 10000 ? 0.0f : powf(10.0f, (float)-b->pan / 2000.0f);
    b->gl = v * l, b->gr = v * r;
}

static inline float sample(const dsbuf *b, uint32_t frame, int ch) {
    const uint8_t *p = b->data + (size_t)frame * (uint32_t)b->block + (b->channels > 1 ? (size_t)ch * (uint32_t)(b->bits / 8) : 0);
    return b->bits == 16 ? (float)*(const int16_t *)p / 32768.0f : ((float)*p - 128.0f) / 128.0f;
}

/* The SDL callback marks position crossings under g_mix. It never enters
 * the emulated machine or accesses guest kernel handles from the audio thread. */
static void notify_mark(dsnotify *n) {
    n->pending = 1;
    __atomic_store_n(&g_notify_pending, 1, __ATOMIC_RELEASE);
}
static void notify_advance(dsbuf *b, double start, double distance) {
    if (!b->bytes || distance <= 0) return;
    for (uint32_t i = 0; i < b->notification_count; i++) {
        dsnotify *n = &b->notifications[i];
        if (n->offset == UINT32_MAX) continue;
        double delta = n->offset - start;
        if (delta <= 0 && !b->looping) continue;
        if (delta <= 0) delta += b->bytes;
        if (distance >= delta) notify_mark(n);
    }
}
static void notify_stop(dsbuf *b) {
    for (uint32_t i = 0; i < b->notification_count; i++)
        if (b->notifications[i].offset == UINT32_MAX) notify_mark(&b->notifications[i]);
}

static int audio_open(void);
static int audio_open_once(void) { return audio_open(); }

/* Host streams: PCM a host pushes (a movie's sound), mixed with the
 * buffers. A ring of interleaved s16 at the stream's rate, played at that
 * rate. */
#define MAX_STREAMS 4
#define STREAM_FRAMES (1 << 17)
static struct { int used, channels, rate, paused; float gain; int16_t *ring; uint32_t rd, wr; double pos; } g_stream[MAX_STREAMS];
int hle_audio_stream_open(int rate, int channels) {
    if (rate <= 0 || channels < 1 || channels > 2) return -1;
    if (!audio_open_once()) return -1;
    pthread_mutex_lock(&g_mix);
    int id = -1;
    for (int i = 0; i < MAX_STREAMS && id < 0; i++)
        if (!g_stream[i].used) {
            int16_t *ring = (int16_t *)calloc(STREAM_FRAMES * 2, sizeof(*ring));
            if (!ring) break;
            id = i;
            g_stream[i].ring = ring;
            g_stream[i].used = 1, g_stream[i].channels = channels, g_stream[i].rate = rate, g_stream[i].gain = 1.0f;
            g_stream[i].rd = g_stream[i].wr = 0, g_stream[i].pos = 0, g_stream[i].paused = 0;
        }
    pthread_mutex_unlock(&g_mix);
    return id;
}
void hle_audio_stream_push(int id, const int16_t *pcm, int frames) {
    if (id < 0 || id >= MAX_STREAMS || !pcm || frames <= 0) return;
    pthread_mutex_lock(&g_mix);
    if (!g_stream[id].used) { pthread_mutex_unlock(&g_mix); return; }
    for (int f = 0; f < frames && g_stream[id].wr - g_stream[id].rd < STREAM_FRAMES - 1; f++, g_stream[id].wr++) {
        uint32_t k = g_stream[id].wr % STREAM_FRAMES;
        g_stream[id].ring[2 * k] = pcm[f * g_stream[id].channels];
        g_stream[id].ring[2 * k + 1] = pcm[f * g_stream[id].channels + (g_stream[id].channels > 1)];
    }
    pthread_mutex_unlock(&g_mix);
}
int hle_audio_stream_queued(int id) {                    /* frames not yet played */
    if (id < 0 || id >= MAX_STREAMS) return 0;
    pthread_mutex_lock(&g_mix);
    int n = (int)(g_stream[id].wr - g_stream[id].rd);
    pthread_mutex_unlock(&g_mix);
    return n;
}
void hle_audio_stream_gain(int id, float gain) {
    if (id < 0 || id >= MAX_STREAMS) return;
    pthread_mutex_lock(&g_mix);
    if (g_stream[id].used) g_stream[id].gain = gain;
    pthread_mutex_unlock(&g_mix);
}
int hle_audio_stream_pause(int id, int paused) {
    if (id < 0 || id >= MAX_STREAMS) return -1;
    pthread_mutex_lock(&g_mix);
    int result = g_stream[id].used ? 0 : -1;
    if (!result) g_stream[id].paused = paused != 0;
    pthread_mutex_unlock(&g_mix);
    return result;
}
void hle_audio_stream_clear(int id) {
    if (id < 0 || id >= MAX_STREAMS) return;
    pthread_mutex_lock(&g_mix);
    if (g_stream[id].used) {
        memset(g_stream[id].ring, 0, STREAM_FRAMES * 2 * sizeof(*g_stream[id].ring));
        g_stream[id].rd = g_stream[id].wr = 0;
        g_stream[id].pos = 0;
    }
    pthread_mutex_unlock(&g_mix);
}
double hle_audio_stream_period(int id) {
    if (id < 0 || id >= MAX_STREAMS) return 0;
    pthread_mutex_lock(&g_mix);
    double result = g_stream[id].used ? g_audio_period : 0;
    pthread_mutex_unlock(&g_mix);
    return result;
}
void hle_audio_stream_close(int id) {
    if (id < 0 || id >= MAX_STREAMS) return;
    pthread_mutex_lock(&g_mix);
    free(g_stream[id].ring);
    memset(&g_stream[id], 0, sizeof g_stream[id]);
    pthread_mutex_unlock(&g_mix);
}

static void mix(void *ud, Uint8 *stream, int len) {
    (void)ud;
    int frames = len / 4;
    static float acc[2 * 8192];
    if (frames > 8192) frames = 8192;
    memset(acc, 0, sizeof(float) * 2 * (size_t)frames);
    pthread_mutex_lock(&g_mix);
    for (int i = 0; i < g_buf_capacity; i++) {
        dsbuf *b = g_buf[i];
        if (!b || !b->playing || b->primary || !b->block) continue;
        uint32_t nframes = b->bytes / (uint32_t)b->block;
        if (!nframes) continue;
        double step = (double)b->rate / OUT_RATE;
        double first = b->pos, advanced = 0;
        int stopped = 0;
        for (int f = 0; f < frames && b->playing; f++) {
            uint32_t at = (uint32_t)b->pos;
            if (at >= nframes) {
                if (b->looping) { b->pos -= nframes; at = (uint32_t)b->pos; if (at >= nframes) b->pos = 0, at = 0; }
                else { b->playing = 0; b->pos = 0; stopped = 1; break; }
            }
            float l = sample(b, at, 0), r = b->channels > 1 ? sample(b, at, 1) : l;
            acc[2 * f] += l * b->gl;
            acc[2 * f + 1] += r * b->gr;
            b->pos += step;
            advanced += step;
        }
        notify_advance(b, first * b->block, advanced * b->block);
        if (b->pos >= nframes) {
            if (b->looping) b->pos = fmod(b->pos, nframes);
            else { b->playing = 0; b->pos = 0; stopped = 1; }
        }
        if (stopped) notify_stop(b);
    }
    for (int i = 0; i < MAX_STREAMS; i++) {
        if (!g_stream[i].used || g_stream[i].paused) continue;
        double step = (double)g_stream[i].rate / OUT_RATE;
        for (int f = 0; f < frames && g_stream[i].rd < g_stream[i].wr; f++) {
            uint32_t k = g_stream[i].rd % STREAM_FRAMES;
            acc[2 * f] += g_stream[i].ring[2 * k] / 32768.0f * g_stream[i].gain;
            acc[2 * f + 1] += g_stream[i].ring[2 * k + 1] / 32768.0f * g_stream[i].gain;
            g_stream[i].pos += step;
            while (g_stream[i].pos >= 1.0 && g_stream[i].rd < g_stream[i].wr) g_stream[i].pos -= 1.0, g_stream[i].rd++;
        }
    }
    pthread_mutex_unlock(&g_mix);
    int16_t *out = (int16_t *)stream;
    for (int k = 0; k < 2 * frames; k++) {
        float v = acc[k] * g_master * 32767.0f;
        out[k] = (int16_t)(v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : v);
    }
}

static int audio_open(void) {
    if (g_audio_ok >= 0) return g_audio_ok;
    g_audio_ok = 0;
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");      /* SIGINT and SIGTERM end the process, not WM_CLOSE */
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = OUT_RATE, want.format = AUDIO_S16SYS, want.channels = 2, want.samples = 1024, want.callback = mix;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!g_dev) {
        /* No sound card (a server, a container): a silent device whose
         * cursors still move, rather than DirectSound failing. A game whose
         * sound DLL takes the failure for granted (Civilization III's
         * sound.dll) would fault on it. */
        fprintf(stderr, "[dsound] no audio device (%s): silent\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (!g_dev) { fprintf(stderr, "[dsound] no audio: %s\n", SDL_GetError()); return 0; }
    }
    g_audio_period = have.freq > 0 ? have.samples / (double)have.freq : 0;
    SDL_PauseAudioDevice(g_dev, 0);
    fprintf(stderr, "[dsound] %s, %d Hz\n", SDL_GetCurrentAudioDriver(), have.freq);
    return g_audio_ok = 1;
}

/* the object: vtable, refs, then the buffer slot */
#define SLOT(self) MEM32((self) + 8)
static dsbuf *buf_of(uint32_t self) { uint32_t i = SLOT(self); return i < g_buf_capacity ? g_buf[i] : NULL; }

uint32_t hle_dsound_set_notifications(uint32_t object, uint32_t count, const uint32_t *pairs) {
    dsbuf *b = buf_of(object);
    if (!b || b->primary || !(b->flags & 0x100u) || count > DS_NOTIFY_MAX || (count && !pairs))
        return DSERR_INVALIDPARAM;
    dsnotify *list = count ? (dsnotify *)calloc(count, sizeof(*list)) : NULL;
    if (count && !list) return 0x8007000eu;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t offset = pairs[i * 2], event = pairs[i * 2 + 1];
        if ((offset != UINT32_MAX && offset >= b->bytes) ||
            (offset == UINT32_MAX && i != count - 1) || hle_handle_type(event) != HLE_H_EVENT) {
            free(list); return DSERR_INVALIDPARAM;
        }
        list[i].offset = offset; list[i].event = event;
    }
    pthread_mutex_lock(&g_mix);
    if (b->playing) { pthread_mutex_unlock(&g_mix); free(list); return DSERR_INVALIDCALL; }
    free(b->notifications); b->notifications = list; b->notification_count = count;
    pthread_mutex_unlock(&g_mix);
    return DS_OK;
}
/* Call only while owning the emulated machine, e.g. before checking a guest
 * wait handle. Coalescing repeated crossings before delivery matches events'
 * signaled state. Chunking keeps stack use bounded even for large arrays. */
void hle_dsound_poll_notifications(void) {
    if (!__atomic_load_n(&g_notify_pending, __ATOMIC_ACQUIRE)) return;
    for (;;) {
        uint32_t deliver[256]; unsigned count = 0; int more = 0;
        pthread_mutex_lock(&g_mix);
        for (unsigned i = 0; i < g_buf_capacity; i++) if (g_buf[i]) {
            dsbuf *b = g_buf[i];
            for (uint32_t n = 0; n < b->notification_count; n++) if (b->notifications[n].pending) {
                if (count == 256) { more = 1; continue; }
                deliver[count++] = b->notifications[n].event; b->notifications[n].pending = 0;
            }
        }
        __atomic_store_n(&g_notify_pending, more, __ATOMIC_RELEASE);
        pthread_mutex_unlock(&g_mix);
        for (unsigned i = 0; i < count; i++) hle_set_event(deliver[i]);
        if (!more) break;
    }
}

static uint32_t g_vt_ds, g_vt_buf;
void hle_dsound_buffer_stats(uint32_t out[5]) {
    if (!out) return;
    pthread_mutex_lock(&g_mix);
    out[0]=g_buf_capacity;out[1]=g_buf_live;out[2]=g_buf_peak;out[3]=g_buf_created;out[4]=g_buf_released;
    pthread_mutex_unlock(&g_mix);
}
static uint32_t new_buffer(dsbuf *b) {
    uint32_t object=hle_com_new(g_vt_buf,12);
    if (!object) return 0;
    pthread_mutex_lock(&g_mix);
    uint32_t slot=0;
    while (slot<g_buf_capacity && g_buf[slot]) ++slot;
    if (slot==g_buf_capacity) {
        uint32_t capacity=g_buf_capacity?g_buf_capacity*2u:256u;
        dsbuf **grown=capacity>g_buf_capacity && capacity<=SIZE_MAX/sizeof(*grown)
            ? realloc(g_buf,(size_t)capacity*sizeof(*grown)):NULL;
        if (!grown) {
            fprintf(stderr,"[dsound-pool] allocation failed capacity=%u live=%u created=%u released=%u request_bytes=%u\n",g_buf_capacity,g_buf_live,g_buf_created,g_buf_released,b->bytes);
            pthread_mutex_unlock(&g_mix);hle_com_free(object);return 0;
        }
        memset(grown+g_buf_capacity,0,(capacity-g_buf_capacity)*sizeof(*grown));
        g_buf=grown;g_buf_capacity=capacity;
        unsigned long long bytes=0;for(uint32_t i=0;i<g_buf_capacity;i++)if(g_buf[i])bytes+=g_buf[i]->bytes;
        fprintf(stderr,"[dsound-pool] grew capacity=%u live=%u created=%u released=%u buffer_bytes_sum=%llu request_bytes=%u\n",g_buf_capacity,g_buf_live,g_buf_created,g_buf_released,bytes,b->bytes);
    }
    g_buf[slot]=b;SLOT(object)=slot;++g_buf_live;++g_buf_created;if(g_buf_live>g_buf_peak)g_buf_peak=g_buf_live;
    pthread_mutex_unlock(&g_mix);
    return object;
}

/* ---- IDirectSoundBuffer ---- */
static void b_Release(void) {
    uint32_t self = A32(0), n = --MEM32(self + 4);
    if (!n) {
        pthread_mutex_lock(&g_mix);
        dsbuf *b = buf_of(self);
        g_buf[SLOT(self)] = NULL;
        if(b){--g_buf_live;++g_buf_released;}
        pthread_mutex_unlock(&g_mix);
        if (b) {
            if (--*b->data_refs == 0) { hle_free(b->data); free(b->data_refs); }
            free(b->notifications);
            free(b);
        }
        hle_com_free(self);
    }
    RET(n, 1);
}
static void b_GetCaps(void) {                            /* (this, &DSBCAPS) */
    dsbuf *b = buf_of(A32(0));
    uint32_t c = A32(1);
    if (!b || !c) RET(DSERR_INVALIDPARAM, 2);
    MEM32(c + 4) = b->flags | 0x8u /* LOCSOFTWARE */, MEM32(c + 8) = b->bytes, MEM32(c + 12) = 0, MEM32(c + 16) = 0;
    RET(DS_OK, 2);
}
static void b_GetCurrentPosition(void) {                 /* (this, &play, &write) */
    dsbuf *b = buf_of(A32(0));
    if (!b) RET(DSERR_INVALIDPARAM, 3);
    pthread_mutex_lock(&g_mix);
    uint32_t play = b->block ? (uint32_t)b->pos * (uint32_t)b->block : 0;
    uint32_t lead = b->block ? (uint32_t)(b->rate / 100) * (uint32_t)b->block : 0;   /* 10 ms ahead */
    pthread_mutex_unlock(&g_mix);
    if (b->bytes) play %= b->bytes;
    if (A32(1)) MEM32(A32(1)) = play;
    if (A32(2)) MEM32(A32(2)) = b->bytes ? (play + (b->playing ? lead : 0)) % b->bytes : 0;
    RET(DS_OK, 3);
}
static void b_GetFormat(void) {                          /* (this, &wfx, size, &written) */
    dsbuf *b = buf_of(A32(0));
    uint32_t w = A32(1);
    if (!b) RET(DSERR_INVALIDPARAM, 4);
    if (w && A32(2) >= 16) {
        MEM16(w) = 1, MEM16(w + 2) = (uint16_t)b->channels, MEM32(w + 4) = (uint32_t)b->rate;
        MEM32(w + 8) = (uint32_t)(b->rate * b->block), MEM16(w + 12) = (uint16_t)b->block, MEM16(w + 14) = (uint16_t)b->bits;
        if (A32(2) >= 18) MEM16(w + 16) = 0;
    }
    if (A32(3)) MEM32(A32(3)) = 18;
    RET(DS_OK, 4);
}
static void b_GetVolume(void)    { dsbuf *b = buf_of(A32(0)); if (A32(1) && b) MEM32(A32(1)) = (uint32_t)b->volume; RET(b ? DS_OK : DSERR_INVALIDPARAM, 2); }
static void b_GetPan(void)       { dsbuf *b = buf_of(A32(0)); if (A32(1) && b) MEM32(A32(1)) = (uint32_t)b->pan; RET(b ? DS_OK : DSERR_INVALIDPARAM, 2); }
static void b_GetFrequency(void) { dsbuf *b = buf_of(A32(0)); if (A32(1) && b) MEM32(A32(1)) = (uint32_t)b->rate; RET(b ? DS_OK : DSERR_INVALIDPARAM, 2); }
static void b_GetStatus(void) {
    dsbuf *b = buf_of(A32(0));
    if (A32(1) && b) MEM32(A32(1)) = (b->playing ? DSBSTATUS_PLAYING : 0u) | (b->playing && b->looping ? DSBSTATUS_LOOPING : 0u);
    RET(b ? DS_OK : DSERR_INVALIDPARAM, 2);
}
static void b_Initialize(void) { RET(DSERR_INVALIDCALL, 3); }   /* already initialized */
static void b_Lock(void) {                               /* (this, offset, bytes, &p1, &n1, &p2, &n2, flags) */
    dsbuf *b = buf_of(A32(0));
    if (!b || b->primary) RET(b ? DSERR_PRIOLEVELNEEDED : DSERR_INVALIDPARAM, 8);
    uint32_t off = A32(1), n = A32(2), fl = A32(7);
    if (fl & 2u) off = 0, n = b->bytes;                  /* DSBLOCK_ENTIREBUFFER */
    else if (fl & 1u) {                                  /* DSBLOCK_FROMWRITECURSOR */
        pthread_mutex_lock(&g_mix);
        off = b->block ? ((uint32_t)b->pos * (uint32_t)b->block + (uint32_t)(b->rate / 100) * (uint32_t)b->block) % b->bytes : 0;
        pthread_mutex_unlock(&g_mix);
    }
    if (off >= b->bytes || n > b->bytes) RET(DSERR_INVALIDPARAM, 8);
    uint32_t n1 = off + n <= b->bytes ? n : b->bytes - off;
    if (A32(3)) MEM32(A32(3)) = hle_va(b->data + off);
    if (A32(4)) MEM32(A32(4)) = n1;
    if (A32(5)) MEM32(A32(5)) = n1 < n ? hle_va(b->data) : 0;
    if (A32(6)) MEM32(A32(6)) = n - n1;
    RET(DS_OK, 8);
}
static void b_Unlock(void) { RET(DS_OK, 5); }            /* (this, p1, n1, p2, n2): the memory is the buffer */
static void b_Play(void) {                               /* (this, reserved, priority, flags) */
    dsbuf *b = buf_of(A32(0));
    if (!b) RET(DSERR_INVALIDPARAM, 4);
    pthread_mutex_lock(&g_mix);
    if (!b->playing) for (uint32_t i = 0; i < b->notification_count; i++)
        if (b->notifications[i].offset == (uint32_t)(b->pos * b->block)) notify_mark(&b->notifications[i]);
    b->playing = 1, b->looping = (A32(3) & DSBPLAY_LOOPING) != 0;
    pthread_mutex_unlock(&g_mix);
    hle_dsound_poll_notifications();
    RET(DS_OK, 4);
}
static void b_SetCurrentPosition(void) {
    dsbuf *b = buf_of(A32(0));
    if (!b || !b->block) RET(DSERR_INVALIDPARAM, 2);
    pthread_mutex_lock(&g_mix);
    b->pos = (double)((A32(1) % (b->bytes ? b->bytes : 1)) / (uint32_t)b->block);
    pthread_mutex_unlock(&g_mix);
    RET(DS_OK, 2);
}
static void b_SetFormat(void) {                          /* the primary's: the mix is at the device rate anyway */
    dsbuf *b = buf_of(A32(0));
    uint32_t w = A32(1);
    if (b && w) b->channels = MEM16(w + 2), b->rate = (int)MEM32(w + 4), b->bits = MEM16(w + 14), b->block = MEM16(w + 12);
    RET(DS_OK, 2);
}
static void b_SetVolume(void) {
    dsbuf *b = buf_of(A32(0));
    if (!b) RET(DSERR_INVALIDPARAM, 2);
    pthread_mutex_lock(&g_mix);
    b->volume = (int)A32(1);
    gains(b);
    pthread_mutex_unlock(&g_mix);
    RET(DS_OK, 2);
}
static void b_SetPan(void) {
    dsbuf *b = buf_of(A32(0));
    if (!b) RET(DSERR_INVALIDPARAM, 2);
    pthread_mutex_lock(&g_mix);
    b->pan = (int)A32(1);
    gains(b);
    pthread_mutex_unlock(&g_mix);
    RET(DS_OK, 2);
}
static void b_SetFrequency(void) {
    dsbuf *b = buf_of(A32(0));
    if (!b) RET(DSERR_INVALIDPARAM, 2);
    pthread_mutex_lock(&g_mix);
    if (A32(1)) b->rate = (int)A32(1);
    pthread_mutex_unlock(&g_mix);
    RET(DS_OK, 2);
}
static void b_Stop(void) {
    dsbuf *b = buf_of(A32(0));
    if (!b) RET(DSERR_INVALIDPARAM, 1);
    pthread_mutex_lock(&g_mix);
    b->playing = 0;
    notify_stop(b);
    pthread_mutex_unlock(&g_mix);
    hle_dsound_poll_notifications();
    RET(DS_OK, 1);
}
static void b_Restore(void) { RET(DS_OK, 1); }

/* ---- IDirectSound ---- */
static void d_Release(void) {
    uint32_t self = A32(0), n = --MEM32(self + 4);
    if (!n) hle_com_free(self);
    RET(n, 1);
}
static void d_CreateSoundBuffer(void) {                  /* (this, &DSBUFFERDESC, &out, outer) */
    uint32_t d = A32(1), out = A32(2);
    if (!d || !out) RET(DSERR_INVALIDPARAM, 4);
    uint32_t flags = MEM32(d + 4), bytes = MEM32(d + 8), wfx = MEM32(d + 16);
    dsbuf *b = (dsbuf *)calloc(1, sizeof *b);
    if(!b)RET(0x8007000eu,4);
    b->flags = flags, b->primary = (flags & DSBCAPS_PRIMARYBUFFER) != 0;
    b->channels = 2, b->bits = 16, b->rate = 22050, b->block = 4;
    if (wfx) {
        b->channels = MEM16(wfx + 2), b->rate = (int)MEM32(wfx + 4), b->block = MEM16(wfx + 12), b->bits = MEM16(wfx + 14);
        if (MEM16(wfx) != 1 || (b->bits != 8 && b->bits != 16) || b->channels < 1 || b->channels > 2) {
            fprintf(stderr, "[dsound] buffer format tag %u, %d bits, %d channels: not PCM this mixer plays\n", MEM16(wfx), b->bits, b->channels);
            free(b);
            RET(DSERR_INVALIDPARAM, 4);
        }
    }
    if (b->primary) bytes = 4096 * 4;
    b->bytes = bytes;
    b->data = (uint8_t *)hle_alloc(bytes ? bytes : 4);
    if(!b->data){free(b);RET(0x8007000eu,4);}
    if (b->bits == 8) memset(b->data, 128, bytes);
    b->data_refs = (int *)malloc(sizeof(int));
    if(!b->data_refs){hle_free(b->data);free(b);RET(0x8007000eu,4);}
    *b->data_refs = 1;
    gains(b);
    uint32_t o = new_buffer(b);
    if (!o) { hle_free(b->data); free(b->data_refs); free(b); RET(0x8007000eu, 4); }
    MEM32(out) = o;
    RET(DS_OK, 4);
}
static void d_GetCaps(void) {                            /* (this, &DSCAPS) */
    uint32_t c = A32(1);
    if (c) {
        uint32_t size = MEM32(c);
        memset(hle_ptr(c + 4), 0, size > 4 ? size - 4 : 0);
        MEM32(c + 4) = 0x0F5Fu;                          /* the primary/secondary 8/16-bit mono/stereo, continuous rate */
        MEM32(c + 8) = 100, MEM32(c + 12) = 100000;      /* min/max secondary rate */
        MEM32(c + 16) = 1;                               /* primary buffers */
        MEM32(c + 20) = 64, MEM32(c + 24) = 64, MEM32(c + 28) = 64;   /* max hw mixing: all, static, streaming */
        MEM32(c + 32) = 64, MEM32(c + 36) = 64, MEM32(c + 40) = 64;   /* free hw */
    }
    RET(DS_OK, 2);
}
static void d_DuplicateSoundBuffer(void) {               /* (this, original, &out) */
    dsbuf *src = buf_of(A32(1));
    if (!src || !A32(2)) RET(DSERR_INVALIDPARAM, 3);
    dsbuf *b = (dsbuf *)malloc(sizeof *b);
    if(!b)RET(0x8007000eu,3);
    pthread_mutex_lock(&g_mix);
    *b = *src;
    b->notifications = src->notification_count ? calloc(src->notification_count, sizeof(*b->notifications)) : NULL;
    if (src->notification_count && !b->notifications) {
        pthread_mutex_unlock(&g_mix); free(b); RET(0x8007000eu, 3);
    }
    for (uint32_t i = 0; i < src->notification_count; i++) {
        b->notifications[i] = src->notifications[i]; b->notifications[i].pending = 0;
    }
    (*b->data_refs)++;
    pthread_mutex_unlock(&g_mix);
    b->playing = 0, b->pos = 0;
    uint32_t o = new_buffer(b);
    if (!o) { pthread_mutex_lock(&g_mix); --*b->data_refs; pthread_mutex_unlock(&g_mix); free(b->notifications); free(b); }
    MEM32(A32(2)) = o;
    RET(o ? DS_OK : 0x8007000eu, 3);
}
static void d_SetCooperativeLevel(void) { RET(DS_OK, 3); }
static void d_Compact(void) { RET(DS_OK, 1); }
static void d_GetSpeakerConfig(void) { if (A32(1)) MEM32(A32(1)) = 4; RET(DS_OK, 2); }   /* DSSPEAKER_STEREO */
static void d_SetSpeakerConfig(void) { RET(DS_OK, 2); }
static void d_Initialize(void) { RET(DS_OK, 2); }

static void vtables(void) {
    if (g_vt_ds) return;
    static const win32hle_shim ds[] = {
        { "IDirectSound::QueryInterface", hle_com_QueryInterface }, { "IDirectSound::AddRef", hle_com_AddRef },
        { "IDirectSound::Release", d_Release }, { "IDirectSound::CreateSoundBuffer", d_CreateSoundBuffer },
        { "IDirectSound::GetCaps", d_GetCaps }, { "IDirectSound::DuplicateSoundBuffer", d_DuplicateSoundBuffer },
        { "IDirectSound::SetCooperativeLevel", d_SetCooperativeLevel }, { "IDirectSound::Compact", d_Compact },
        { "IDirectSound::GetSpeakerConfig", d_GetSpeakerConfig }, { "IDirectSound::SetSpeakerConfig", d_SetSpeakerConfig },
        { "IDirectSound::Initialize", d_Initialize }, { 0, 0 } };
    static const win32hle_shim buf[] = {
        { "IDirectSoundBuffer::QueryInterface", hle_com_QueryInterface }, { "IDirectSoundBuffer::AddRef", hle_com_AddRef },
        { "IDirectSoundBuffer::Release", b_Release }, { "IDirectSoundBuffer::GetCaps", b_GetCaps },
        { "IDirectSoundBuffer::GetCurrentPosition", b_GetCurrentPosition }, { "IDirectSoundBuffer::GetFormat", b_GetFormat },
        { "IDirectSoundBuffer::GetVolume", b_GetVolume }, { "IDirectSoundBuffer::GetPan", b_GetPan },
        { "IDirectSoundBuffer::GetFrequency", b_GetFrequency }, { "IDirectSoundBuffer::GetStatus", b_GetStatus },
        { "IDirectSoundBuffer::Initialize", b_Initialize }, { "IDirectSoundBuffer::Lock", b_Lock },
        { "IDirectSoundBuffer::Play", b_Play }, { "IDirectSoundBuffer::SetCurrentPosition", b_SetCurrentPosition },
        { "IDirectSoundBuffer::SetFormat", b_SetFormat }, { "IDirectSoundBuffer::SetVolume", b_SetVolume },
        { "IDirectSoundBuffer::SetPan", b_SetPan }, { "IDirectSoundBuffer::SetFrequency", b_SetFrequency },
        { "IDirectSoundBuffer::Stop", b_Stop }, { "IDirectSoundBuffer::Unlock", b_Unlock },
        { "IDirectSoundBuffer::Restore", b_Restore }, { 0, 0 } };
    g_vt_ds = hle_com_vtable(ds);
    g_vt_buf = hle_com_vtable(buf);
}

static void s_DirectSoundCreate(void) {                  /* (guid, &out, outer) */
    vtables();
    if (!A32(1)) RET(DSERR_INVALIDPARAM, 3);
    if (!audio_open()) { MEM32(A32(1)) = 0; RET(DSERR_NODRIVER, 3); }
    MEM32(A32(1)) = hle_com_new(g_vt_ds, 12);
    RET(DS_OK, 3);
}
static void s_DirectSoundEnumerateA(void) {              /* (callback, context): the one device */
    static uint32_t desc, mod;                           /* the guest may keep them */
    if (!desc) desc = hle_guest_strdup("Primary Sound Driver"), mod = hle_guest_strdup("");
    uint32_t a[4] = { 0, desc, mod, A32(1) };
    if (A32(0)) hle_call_guest(A32(0), 4, a);
    RET(DS_OK, 2);
}

/* CoCreateInstance(CLSID_DirectSound): the object DirectSoundCreate makes;
 * the caller's IDirectSound::Initialize follows. */
static uint32_t ds_create(const uint8_t *iid, uint32_t *out) {
    (void)iid;
    vtables();
    if (!audio_open()) { *out = 0; return DSERR_NODRIVER; }
    *out = hle_com_new(g_vt_ds, 12);
    return DS_OK;
}
void hle_dsound_register_com(void) {
    static const uint8_t CLSID_DirectSound[16] = { 0x46, 0xD9, 0xD4, 0x47, 0xE8, 0x62, 0xCF, 0x11,
                                                   0x93, 0xBC, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 };
    hle_com_register_class(CLSID_DirectSound, ds_create);
}

const win32hle_shim win32hle_dsound[] = {
    { "dsound.dll#1", s_DirectSoundCreate }, { "DirectSoundCreate", s_DirectSoundCreate },
    { "dsound.dll#2", s_DirectSoundEnumerateA }, { "DirectSoundEnumerateA", s_DirectSoundEnumerateA },
    { 0, 0 }
};
