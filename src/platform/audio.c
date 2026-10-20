#include "platform/audio.h"

#include <SDL.h>
#include <string.h>

#include "assets/assets.h"

#define CROSSFADE_MS 140
#define MAX_VOICES 6       /* tracked (not fading) voices */
#define CHANNELS 16        /* physical voices, fading ones included */
#define MAX_CLIPS 64

typedef struct {
    Sint16 *samples;       /* stereo, device rate */
    Uint32 frames;
    int priority;
    float volume;
} Clip;

typedef struct {
    int active, fading, id, priority;
    Uint32 pos;
    float gain, fade_step;
    Uint32 started;        /* play order, for "oldest" */
} Voice;

static SDL_AudioDeviceID dev;
static SDL_AudioSpec have;
static Clip clips[MAX_CLIPS];
static int clip_count;
static char sounds_dir[1024]; /* holds sounds/, or "" */
static Voice voices[CHANNELS];
static Uint32 play_counter;

static void mix(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    Sint16 *out = (Sint16 *)stream;
    int frames = len / 4;
    memset(stream, 0, (size_t)len);
    for (int c = 0; c < CHANNELS; c++) {
        Voice *v = &voices[c];
        if (!v->active) continue;
        const Clip *clip = &clips[v->id];
        float vol = clip->volume;
        for (int i = 0; i < frames; i++) {
            if (v->pos >= clip->frames || v->gain <= 0) {
                v->active = 0;
                break;
            }
            for (int ch = 0; ch < 2; ch++) {
                int s = out[i * 2 + ch] + (int)(clip->samples[v->pos * 2 + ch] * vol * v->gain);
                out[i * 2 + ch] = (Sint16)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
            }
            v->pos++;
            if (v->fading) v->gain -= v->fade_step;
        }
    }
}

int audio_load(const char *file, int priority, float volume)
{
    if (!dev || !sounds_dir[0] || clip_count >= MAX_CLIPS) return -1;
    int id = clip_count;
    char path[1200];
    snprintf(path, sizeof path, "%s/sounds/%s", sounds_dir, file);
    SDL_AudioSpec spec;
    Uint8 *buf;
    Uint32 len;
    if (!SDL_LoadWAV(path, &spec, &buf, &len)) return -1;
    SDL_AudioCVT cvt;
    if (SDL_BuildAudioCVT(&cvt, spec.format, spec.channels, spec.freq, AUDIO_S16SYS, 2, have.freq) < 0) {
        SDL_FreeWAV(buf);
        return -1;
    }
    cvt.len = (int)len;
    cvt.buf = SDL_malloc((size_t)len * (size_t)(cvt.len_mult > 0 ? cvt.len_mult : 1));
    if (!cvt.buf) {
        SDL_FreeWAV(buf);
        return -1;
    }
    memcpy(cvt.buf, buf, len);
    SDL_FreeWAV(buf);
    if (cvt.needed && SDL_ConvertAudio(&cvt) < 0) {
        SDL_free(cvt.buf);
        return -1;
    }
    clips[id].samples = (Sint16 *)cvt.buf;
    clips[id].frames = (Uint32)(cvt.needed ? cvt.len_cvt : cvt.len) / 4;
    clips[id].priority = priority;
    clips[id].volume = volume;
    clip_count++;
    return id;
}

int audio_init(const char *argv0)
{
    if (assets_find(argv0, "sounds", sounds_dir, sizeof sounds_dir)) sounds_dir[0] = '\0';
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL audio: %s", SDL_GetError());
        return 0;
    }
    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = mix;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!dev) {
        SDL_Log("SDL_OpenAudioDevice: %s", SDL_GetError());
        return 0;
    }
    SDL_PauseAudioDevice(dev, 0);
    return 1;
}

static void fade_out(Voice *v)
{
    v->fading = 1;
    v->fade_step = v->gain / (have.freq * CROSSFADE_MS / 1000.0f);
}

void audio_play(int id)
{
    if (!dev || id < 0 || id >= clip_count || !clips[id].samples) return;
    int prio = clips[id].priority;
    SDL_LockAudioDevice(dev);
    /* voices of equal or lower priority fade out; higher ones play on underneath */
    int tracked = 0;
    for (int c = 0; c < CHANNELS; c++) {
        Voice *v = &voices[c];
        if (!v->active || v->fading) continue;
        if (v->priority > prio) tracked++;
        else fade_out(v);
    }
    while (tracked >= MAX_VOICES) {
        Voice *oldest = NULL;
        for (int c = 0; c < CHANNELS; c++)
            if (voices[c].active && !voices[c].fading && (!oldest || voices[c].started < oldest->started))
                oldest = &voices[c];
        if (!oldest) break;
        fade_out(oldest);
        tracked--;
    }
    Voice *slot = NULL;
    for (int c = 0; c < CHANNELS && !slot; c++)
        if (!voices[c].active) slot = &voices[c];
    if (!slot) { /* every channel busy: take the quietest fading one */
        for (int c = 0; c < CHANNELS; c++)
            if (voices[c].fading && (!slot || voices[c].gain < slot->gain)) slot = &voices[c];
    }
    if (slot) {
        slot->active = 1;
        slot->fading = 0;
        slot->id = id;
        slot->priority = prio;
        slot->pos = 0;
        slot->gain = 1;
        slot->started = ++play_counter;
    }
    SDL_UnlockAudioDevice(dev);
}

void audio_stop(int id)
{
    if (!dev || id < 0) return;
    SDL_LockAudioDevice(dev);
    for (int c = 0; c < CHANNELS; c++)
        if (voices[c].id == id) voices[c].active = 0;
    SDL_UnlockAudioDevice(dev);
}

void audio_stop_all(void)
{
    if (!dev) return;
    SDL_LockAudioDevice(dev);
    for (int c = 0; c < CHANNELS; c++) voices[c].active = 0;
    SDL_UnlockAudioDevice(dev);
}

void audio_shutdown(void)
{
    if (dev) SDL_CloseAudioDevice(dev);
    dev = 0;
    for (int i = 0; i < clip_count; i++) {
        SDL_free(clips[i].samples);
        clips[i].samples = NULL;
    }
    clip_count = 0;
}
