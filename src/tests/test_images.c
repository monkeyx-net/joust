/*
 * The port's blitter against MAME's (label mame):
 *   test_images <joust.zip|folder> <image_probe output>
 * For every line the probe wrote ("IMG addr ctrl bytes"), run the same blit
 * through gfx/williams.c on the same pattern and compare what it wrote.
 * The catalogue's images are decoded this way, so a pass means they come
 * out of the ROM exactly as the arcade's blitter draws them.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/images.h"
#include "gfx/williams.h"

#define PATTERN 0x5A
#define SOLID 0x37

static RomSet rom;
static uint8_t ram[WILLIAMS_VRAM_SIZE];
static char line[1 << 16];

static int hex(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: test_images <joust.zip|folder> <image_probe output>\n");
        return 2;
    }
    char err[512];
    if (rom_load(&rom, argv[1], err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    FILE *f = fopen(argv[2], "r");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", argv[2]);
        return 1;
    }
    int lines = 0, bad = 0, seen[MAX_ROM_IMAGES] = { 0 };
    while (fgets(line, sizeof line, f)) {
        unsigned addr, ctrl;
        int off;
        if (sscanf(line, "IMG %x %x %n", &addr, &ctrl, &off) != 2) continue;
        int idx = -1;
        for (int i = 0; i < ROM_IMAGE_COUNT; i++)
            if (ROM_IMAGES[i].addr == addr) idx = i;
        if (idx < 0) {
            fprintf(stderr, "%04X: not in the catalogue\n", addr);
            bad++;
            continue;
        }
        seen[idx] = 1;
        const RomImage *im = &ROM_IMAGES[idx];
        int cols = im->w + 1;
        memset(ram, 0, sizeof ram);
        for (int c = 0; c < cols; c++)
            for (int y = 0; y < im->h; y++) ram[c * 256 + y] = PATTERN;
        WilliamsBus bus = { ram, &rom, 1, NULL, NULL, NULL };
        williams_blit(&bus, (uint8_t)ctrl, SOLID, (uint16_t)addr, 0x0000, (uint8_t)(im->w ^ 4), (uint8_t)(im->h ^ 4));
        const char *p = line + off;
        int diffs = 0;
        for (int c = 0; c < cols; c++)
            for (int y = 0; y < im->h; y++, p += 2) {
                int want = hex(p[0]) << 4 | hex(p[1]);
                if (hex(p[0]) < 0 || hex(p[1]) < 0) {
                    diffs = -1;
                    goto done;
                }
                diffs += ram[c * 256 + y] != want;
            }
    done:
        if (diffs) {
            fprintf(stderr, "%04X ctrl %02X: %d bytes differ from MAME\n", addr, ctrl, diffs);
            bad++;
        }
        lines++;
    }
    fclose(f);
    for (int i = 0; i < ROM_IMAGE_COUNT; i++)
        if (!seen[i]) {
            fprintf(stderr, "%04X: no MAME output\n", ROM_IMAGES[i].addr);
            bad++;
        }
    printf("images: %d blits compared, %d mismatched\n", lines, bad);
    return bad || !lines ? 1 : 0;
}
