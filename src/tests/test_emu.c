/*
 * The emulated board against MAME (label mame):
 *   test_emu <joust.zip|folder> <primed CMOS> <prng_probe output> [frame:snapshot.png ...]
 * Runs the same scripted game as the probes (coin at frame 1200, 1 player
 * start at 1300) from power-on with the same CMOS, then checks that
 *   - every change of the random state ($A00D:$A00E) matches MAME's, in order
 *     (a sharp test of timing: the IRQ's stirs and the game's steps interleave)
 *   - the given frames equal MAME's snapshots pixel for pixel.
 * Inputs a MAME probe sets in its frame callback reach the game at the start
 * of the frame after next (MAME updates its ports once per frame).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/png.h"
#include "assets/rom.h"
#include "emu/board.h"

static RomSet rom;
static Board board;
static uint8_t cmos[BOARD_CMOS_SIZE];

static char *want;   /* MAME's transitions: one "K hilo" per entry */
static long want_n, got_n, bad;

static void trace(Board *b, uint16_t addr, uint8_t v)
{
    if (addr != 0xA00E || bad) return;
    char k = b->cpu.pc == 0xE399 ? 'S' : b->cpu.pc == 0xE4CC ? 'R' : 'W';
    char line[16];
    snprintf(line, sizeof line, "%c %02X%02X", k, b->ram[0xA00D], v);
    if (got_n >= want_n) return;
    if (memcmp(line, want + got_n * 7, 6)) {
        fprintf(stderr, "random state change %ld: %s, MAME %.6s (frame %ld, line %d)\n", got_n + 1, line,
                want + got_n * 7, b->frame, b->scanline);
        bad = 1;
    }
    got_n++;
}

static void script(Board *b, int line)
{
    if (line != BOARD_FIRST_LINE) return;
    long f = b->frame - 1; /* the probe's frame number whose inputs apply now */
    if (f == 1200) b->in2 |= IN2_COIN1;
    if (f == 1215) b->in2 &= (uint8_t)~IN2_COIN1;
    if (f == 1300) b->in0 |= IN0_START1;
    if (f == 1315) b->in0 &= (uint8_t)~IN0_START1;
}

static int load_trace(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char k;
    unsigned v;
    long cap = 1 << 16;
    want = malloc((size_t)cap * 7);
    int first = 1;
    while (fscanf(f, " %c %4x", &k, &v) == 2) {
        if (first) { /* the probe's "I": the state when it started, before power-on code */
            first = 0;
            continue;
        }
        if (want_n == cap) want = realloc(want, (size_t)(cap *= 2) * 7);
        snprintf(want + want_n * 7, 8, "%c %04X", k, v);
        want_n++;
    }
    fclose(f);
    return 0;
}

static long compare_shot(const Framebuffer *fb, const char *path)
{
    Image shot;
    const char *err;
    if (png_load(path, &shot, &err)) {
        fprintf(stderr, "%s: %s\n", path, err);
        return -1;
    }
    long diff = 0;
    for (int y = 0; y < FB_HEIGHT && y < shot.height; y++)
        for (int x = 0; x < FB_WIDTH && x < shot.width; x++) {
            const uint8_t *p = shot.rgba + ((size_t)y * (size_t)shot.width + (size_t)x) * 4;
            diff += (fb_row(fb, y)[x] & 0xFFFFFF) != ((uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2]);
        }
    image_free(&shot);
    return diff;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: test_emu <rom> <primed cmos> <prng trace> [frame:snapshot.png ...]\n");
        return 2;
    }
    char err[512];
    if (rom_load(&rom, argv[1], err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    FILE *f = fopen(argv[2], "rb");
    if (!f || fread(cmos, 1, sizeof cmos, f) != sizeof cmos) {
        fprintf(stderr, "%s: not a 1 KB CMOS image\n", argv[2]);
        return 1;
    }
    fclose(f);
    if (load_trace(argv[3])) {
        fprintf(stderr, "cannot read %s\n", argv[3]);
        return 1;
    }
    board_init(&board, &rom, cmos);
    board.write_hook = trace;
    board.line_hook = script;
    Framebuffer fb = { 0 };
    fb_init(&fb, 1);

    long last_shot = 0;
    for (int i = 4; i < argc; i++) {
        long fr = strtol(argv[i], NULL, 10);
        if (fr > last_shot) last_shot = fr;
    }
    int failed = 0;
    while (!bad && (got_n < want_n || board.frame < last_shot) && board.frame < 100000) {
        board_run_frame(&board, &fb);
        for (int i = 4; i < argc; i++) {
            char *colon = strchr(argv[i], ':');
            if (!colon || strtol(argv[i], NULL, 10) != board.frame) continue;
            long d = compare_shot(&fb, colon + 1);
            printf("frame %ld: %ld pixels differ from MAME\n", board.frame, d);
            if (d) failed = 1;
        }
    }
    printf("random state: %ld of MAME's %ld changes matched%s\n", got_n, want_n, bad ? " before a mismatch" : "");
    fb_free(&fb);
    free(want);
    return bad || failed || got_n < want_n ? 1 : 0;
}
