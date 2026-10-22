#include "assets/extract.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "emu/board.h"
#include "emu/sound.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

#define PRIME_FRAMES 1800 /* blank CMOS: the game restores its factory settings */
#define SOUND_RATE 48000
#define SOUND_BOOT_CYCLES 20000
#define SOUND_STEP_CYCLES 64
#define SOUND_QUIET_SECONDS 1 /* no DAC change this long: the sound has ended */
#define SOUND_MAX_SECONDS 20
#define SOUND_LOOP_SECONDS 4 /* what a looping sound's WAV keeps */
#define SOUND_TAIL (SOUND_RATE / 20)

/* ---- files ----------------------------------------------------------------- */

static int mkdir_p(const char *path)
{
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(tmp, 0755) && errno != EEXIST) return -1;
        *p = '/';
    }
    return mkdir(tmp, 0755) && errno != EEXIST ? -1 : 0;
}

/* Write `size` bytes to <dir>/<name> through a temp file, so a failure leaves no partial file. */
static int write_file(const char *dir, const char *name, const void *data, size_t size, char *err, int errlen)
{
    char path[1200], tmp[1210];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f || fwrite(data, 1, size, f) != size || fclose(f)) {
        if (f) fclose(f);
        remove(tmp);
        snprintf(err, (size_t)errlen, "cannot write %s", path);
        return -1;
    }
#ifdef _WIN32
    remove(path); /* Windows rename() does not replace */
#endif
    if (rename(tmp, path)) {
        remove(tmp);
        snprintf(err, (size_t)errlen, "cannot replace %s", path);
        return -1;
    }
    return 0;
}

/* A growing text buffer. */
typedef struct {
    char *s;
    size_t len, cap;
} Text;

static void put(Text *t, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void put(Text *t, const char *fmt, ...)
{
    va_list ap;
    for (;;) {
        va_start(ap, fmt);
        int n = vsnprintf(t->s ? t->s + t->len : NULL, t->s ? t->cap - t->len : 0, fmt, ap);
        va_end(ap);
        if (n < 0) return;
        if (t->s && t->len + (size_t)n < t->cap) {
            t->len += (size_t)n;
            return;
        }
        size_t cap = t->cap ? t->cap * 2 : 4096;
        while (cap < t->len + (size_t)n + 1) cap *= 2;
        char *s = realloc(t->s, cap);
        if (!s) abort();
        t->s = s;
        t->cap = cap;
    }
}

static int write_text(const char *dir, const char *name, Text *t, char *err, int errlen)
{
    int rc = write_file(dir, name, t->s ? t->s : "", t->len, err, errlen);
    free(t->s);
    memset(t, 0, sizeof *t);
    return rc;
}

/* ---- the program and the factory settings ---------------------------------- */

static int extract_program(const RomSet *rom, const char *dir, char *err, int errlen)
{
    static uint8_t image[0x10000];
    for (unsigned a = 0; a < 0x10000; a++) {
        int v = (a < 0x9000 || a >= 0xD000) ? rom_byte(rom, a) : -1;
        image[a] = v < 0 ? 0 : (uint8_t)v;
    }
    return write_file(dir, EXTRACT_PROGRAM, image, sizeof image, err, errlen);
}

static Board board;

static int extract_cmos(const RomSet *rom, const char *dir, char *err, int errlen)
{
    board_init(&board, rom, NULL);
    for (int i = 0; i < PRIME_FRAMES; i++) board_run_frame(&board, NULL);
    return write_file(dir, EXTRACT_CMOS, board.cmos, sizeof board.cmos, err, errlen);
}

/* ---- sounds ---------------------------------------------------------------- */

static void le16(uint8_t *p, unsigned v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void le32(uint8_t *p, unsigned long v)
{
    le16(p, (unsigned)(v & 0xFFFF));
    le16(p + 2, (unsigned)(v >> 16));
}

static int write_wav(const char *dir, const char *name, const int16_t *s, long n, char *err, int errlen)
{
    size_t size = 44 + (size_t)n * 2;
    uint8_t *w = malloc(size);
    if (!w) abort();
    memcpy(w, "RIFF", 4);
    le32(w + 4, (unsigned long)size - 8);
    memcpy(w + 8, "WAVEfmt ", 8);
    le32(w + 16, 16);
    le16(w + 20, 1); /* PCM */
    le16(w + 22, 1); /* mono */
    le32(w + 24, SOUND_RATE);
    le32(w + 28, SOUND_RATE * 2);
    le16(w + 32, 2);
    le16(w + 34, 16);
    memcpy(w + 36, "data", 4);
    le32(w + 40, (unsigned long)n * 2);
    for (long i = 0; i < n; i++) le16(w + 44 + i * 2, (unsigned)(uint16_t)s[i]);
    int rc = write_file(dir, name, w, size, err, errlen);
    free(w);
    return rc;
}

static Sound snd;
static int16_t samples[SOUND_RATE * SOUND_MAX_SECONDS + SOUND_MAX_SAMPLES];

/* Each command as the frame loop sends it ($FF, then the command, the top
   two lines pulled up), from a freshly started sound board. */
static int extract_sounds(const char *dir, const RomSet *rom, char *err, int errlen)
{
    Text t = { 0 };
    put(&t, "# Sound commands, recorded from the emulated sound board (extracted; do not edit)\n"
            "# <command> <once|loop|none> <samples at %d Hz>\n", SOUND_RATE);
    for (int cmd = 0; cmd < EXTRACT_SOUND_COMMANDS; cmd++) {
        sound_init(&snd, rom, SOUND_CPU_HZ / SOUND_RATE);
        sound_run(&snd, SOUND_BOOT_CYCLES);
        sound_command(&snd, 0xFF);
        snd.out_len = 0;
        sound_command(&snd, (uint8_t)(0xC0 | cmd));
        long n = 0, last_change = -1;
        uint8_t dac = snd.dac;
        const long max = (long)SOUND_RATE * SOUND_MAX_SECONDS;
        while (n < max && (last_change < 0 ? n < SOUND_RATE * SOUND_QUIET_SECONDS
                                           : n - last_change < SOUND_RATE * SOUND_QUIET_SECONDS)) {
            sound_run(&snd, snd.cpu.cycles + SOUND_STEP_CYCLES);
            if (snd.out_len >= SOUND_MAX_SAMPLES / 2 || snd.dac != dac) {
                memcpy(samples + n, snd.out, (size_t)snd.out_len * sizeof snd.out[0]);
                n += snd.out_len;
                snd.out_len = 0;
            }
            if (snd.dac != dac) {
                dac = snd.dac;
                last_change = n;
            }
        }
        char name[32];
        snprintf(name, sizeof name, "sounds/%02X.wav", cmd);
        if (last_change < 0) {
            put(&t, "%02X none 0\n", cmd);
            continue;
        }
        const char *kind = "once";
        if (n >= max) {
            kind = "loop";
            n = (long)SOUND_RATE * SOUND_LOOP_SECONDS;
        } else {
            n = last_change + SOUND_TAIL;
        }
        if (write_wav(dir, name, samples, n, err, errlen)) {
            free(t.s);
            return -1;
        }
        put(&t, "%02X %s %ld\n", cmd, kind, n);
    }
    return write_text(dir, "sounds/sounds.txt", &t, err, errlen);
}

int extract_all(const RomSet *rom, const char *out_dir, char *err, int errlen)
{
    char path[1100];
    if (!rom->loaded) {
        snprintf(err, (size_t)errlen, "no ROM set loaded");
        return -1;
    }
    snprintf(path, sizeof path, "%s/%s", out_dir, EXTRACT_MANIFEST);
    remove(path); /* until the end, the folder is incomplete */
    const char *dirs[] = { "data", "sounds" };
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) {
        snprintf(path, sizeof path, "%s/%s", out_dir, dirs[i]);
        if (mkdir_p(path)) {
            snprintf(err, (size_t)errlen, "cannot create %s", path);
            return -1;
        }
    }
    if (extract_program(rom, out_dir, err, errlen) || extract_cmos(rom, out_dir, err, errlen) ||
        extract_sounds(out_dir, rom, err, errlen))
        return -1;
    Text t = { 0 };
    put(&t, "joust assets %d\n", EXTRACT_VERSION);
    return write_text(out_dir, EXTRACT_MANIFEST, &t, err, errlen);
}
