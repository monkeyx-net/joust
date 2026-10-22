/*
 * The extractor (label rom): test_extract <joust.zip|folder> <scratch dir>
 * extract_all writes the assets twice (repeatable: the same bytes);
 * data/program.bin is the main CPU's ROM by address, data/cmos.bin the
 * board's CMOS after it restores the factory settings, every sound command
 * the game sends has its WAV (or "none"), and the C game from the assets
 * alone (program.bin, cmos.bin) runs 3000 frames of the attract mode to
 * the same RAM as the C game from the ROM set.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/extract.h"
#include "assets/rom.h"
#include "emu/board.h"
#include "sim/cgame.h"
#include "sim/game.h"

static RomSet rom;
static uint8_t program[0x10000], program2[0x10000], from_rom[0x10000];
static uint8_t cmos[BOARD_CMOS_SIZE];
static Board board;

static int read_all(const char *dir, const char *name, void *buf, size_t size)
{
    char path[1200];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t n = fread(buf, 1, size, f);
    int extra = fgetc(f) != EOF;
    fclose(f);
    return n == size && !extra ? 0 : -1;
}

static int failures;

static void fail(const char *what)
{
    fprintf(stderr, "test_extract: %s\n", what);
    failures++;
}

static Game a, b;

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: test_extract <joust.zip|folder> <scratch dir>\n");
        return 2;
    }
    char err[512];
    if (rom_load(&rom, argv[1], err, sizeof err)) {
        fprintf(stderr, "test_extract: %s\n", err);
        return 1;
    }
    const char *dir = argv[2];
    if (extract_all(&rom, dir, err, sizeof err)) {
        fprintf(stderr, "test_extract: %s\n", err);
        return 1;
    }
    if (read_all(dir, EXTRACT_PROGRAM, program, sizeof program)) fail("no data/program.bin of 64 KB");
    for (unsigned x = 0; x < 0x10000; x++) {
        int v = (x < 0x9000 || x >= 0xD000) ? rom_byte(&rom, x) : 0;
        from_rom[x] = v < 0 ? 0 : (uint8_t)v;
    }
    if (memcmp(program, from_rom, sizeof program)) fail("data/program.bin is not the ROM by address");
    if (read_all(dir, EXTRACT_CMOS, cmos, sizeof cmos)) fail("no data/cmos.bin of 1 KB");
    board_init(&board, &rom, NULL);
    for (int i = 0; i < 1800; i++) board_run_frame(&board, NULL);
    if (memcmp(cmos, board.cmos, sizeof cmos)) fail("data/cmos.bin is not the factory settings");
    /* the sounds: a line per command, a WAV for each that sounds */
    char path[1200], line[128];
    snprintf(path, sizeof path, "%s/sounds/sounds.txt", dir);
    FILE *f = fopen(path, "r");
    int commands = 0, wavs = 0;
    while (f && fgets(line, sizeof line, f)) {
        unsigned cmd;
        char kind[16];
        if (line[0] == '#' || sscanf(line, "%x %15s", &cmd, kind) != 2) continue;
        commands++;
        if (strcmp(kind, "none")) {
            char name[32];
            snprintf(name, sizeof name, "sounds/%02X.wav", cmd);
            snprintf(path, sizeof path, "%s/%s", dir, name);
            FILE *w = fopen(path, "rb");
            if (!w) fail("a sound's WAV is missing");
            else {
                fclose(w);
                wavs++;
            }
        }
    }
    if (f) fclose(f);
    if (commands != EXTRACT_SOUND_COMMANDS) fail("sounds.txt does not list every command");
    if (wavs < 20) fail("too few sounds");
    /* repeatable */
    if (extract_all(&rom, dir, err, sizeof err) || read_all(dir, EXTRACT_PROGRAM, program2, sizeof program2) ||
        memcmp(program, program2, sizeof program))
        fail("a second extraction differs");
    /* the C game from the assets and from the ROM set */
    a.rom = program;
    b.rom = from_rom;
    memcpy(a.cmos, cmos, sizeof cmos);
    memcpy(b.cmos, board.cmos, sizeof cmos);
    cgame_power_on(&a);
    cgame_power_on(&b);
    CGame ca = { &a, NULL, NULL, 0 }, cb = { &b, NULL, NULL, 0 };
    for (int i = 0; i < 3000; i++) {
        uint16_t t;
        if (cgame_frame(&ca, &t) != FRAME_DONE || cgame_frame(&cb, &t) != FRAME_DONE) {
            fail("the C game reached a step not ported");
            break;
        }
    }
    if (memcmp(a.ram, b.ram, sizeof a.ram)) fail("the C game from the assets differs from the one from the ROM set");
    printf("test_extract: %d sound commands, %d WAVs; the C game from the assets matches\n", commands, wavs);
    if (failures) return 1;
    printf("test_extract: OK\n");
    return 0;
}
