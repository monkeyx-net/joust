/*
 * The ROM set loader against the user's joust set (label rom):
 *   test_rom <joust.zip or folder> <scratch dir>
 * Loads it as given, then from a folder of the same ROMs under other names
 * (matching is by CRC), then checks an empty folder fails with a message.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "assets/rom.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir(dir, mode) _mkdir(dir)
#endif

static int failures;
#define CHECK(cond, ...)                                                    \
    do {                                                                    \
        if (!(cond)) {                                                      \
            failures++;                                                     \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);            \
            fprintf(stderr, __VA_ARGS__);                                   \
            fputc('\n', stderr);                                            \
        }                                                                   \
    } while (0)

static RomSet a, b;

static void write_rom(const char *dir, const char *name, const uint8_t *data)
{
    char path[1200];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    CHECK(f && fwrite(data, 1, ROM_SIZE, f) == ROM_SIZE, "cannot write %s", path);
    if (f) fclose(f);
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: test_rom <joust.zip|folder> <scratch dir>\n");
        return 2;
    }
    char err[512];
    CHECK(!rom_load(&a, argv[1], err, sizeof err), "load %s: %s", argv[1], err);
    CHECK(rom_byte(&a, 0x9000) == -1 && rom_byte(&a, 0xCFFF) == -1, "RAM and I/O addresses read as ROM");
    CHECK(rom_byte(&a, 0xD000) == a.prog[9][0] && rom_byte(&a, 0xFFFF) == a.prog[11][0xFFF], "bank 10-12 mapping");
    CHECK(rom_byte(&a, 0x8FFF) == a.prog[8][0xFFF], "bank 9 mapping");

    /* the same ROMs, renamed and shuffled into a folder */
    mkdir(argv[2], 0755);
    char dir[1100];
    snprintf(dir, sizeof dir, "%s/renamed", argv[2]);
    mkdir(dir, 0755);
    for (int i = 0; i < ROM_PROG_BANKS; i++) {
        char name[32];
        snprintf(name, sizeof name, "chip%02d.bin", (i * 7) % ROM_PROG_BANKS);
        write_rom(dir, name, a.prog[i]);
    }
    write_rom(dir, "noise.snd", a.sound);
    uint8_t junk[ROM_SIZE];
    memset(junk, 0x5A, sizeof junk);
    write_rom(dir, "unrelated.bin", junk);
    CHECK(!rom_load(&b, dir, err, sizeof err), "load renamed folder: %s", err);
    CHECK(!memcmp(a.prog, b.prog, sizeof a.prog) && !memcmp(a.sound, b.sound, sizeof a.sound),
          "renamed folder loaded different ROMs");

    snprintf(dir, sizeof dir, "%s/empty", argv[2]);
    mkdir(dir, 0755);
    err[0] = '\0';
    CHECK(rom_load(&b, dir, err, sizeof err) == -1 && strstr(err, "missing joust_rom_1b"),
          "empty folder: rc/err '%s'", err);
    CHECK(!b.loaded && rom_byte(&b, 0xD000) == -1, "failed load left the set usable");

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    puts("rom: all checks passed");
    return 0;
}
