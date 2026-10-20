/*
 * Images in the program ROM: data address, width in bytes (2 pixels each),
 * height. Found by logging the blitter in MAME (src/tests/mame/capture_probe.lua)
 * through the attract mode and a 30-minute game with unlimited lives, and by
 * walking the animation frame-record tables ($0DF9, $232A, $2F0B, $34A8).
 * Sprites keep a 2-byte header before their data (width and height XOR 4, as
 * the blitter takes them); sizes marked H come from it, C from the capture.
 * Addresses and sizes only: the pixels stay in the ROM (JOUST_PORT_PLAN.md, P1).
 */
#include "assets/images.h"

#include <stdlib.h>
#include <string.h>

#include "gfx/williams.h"

const RomImage ROM_IMAGES[] = {
    { 0x004A, 17,  7 }, /* C */
    { 0x00EF, 24,  7 }, /* C */
    { 0x01ED, 44,  9 }, /* C */
    { 0x03AF, 32,  8 }, /* C */
    { 0x04E1, 29, 11 }, /* H */
    { 0x062E, 24,  7 }, /* C */
    { 0x0728, 32,  8 }, /* C */
    { 0x0866,  8, 13 }, /* H */
    { 0x08CE,  8, 13 }, /* C */
    { 0x093E, 93,  2 }, /* C */
    { 0x0DAF, 14,  3 }, /* C */
    { 0x12BB,  9, 20 }, /* H */
    { 0x1371,  8, 20 }, /* H */
    { 0x1413,  9, 20 }, /* H */
    { 0x14C9,  9, 20 }, /* H */
    { 0x157F,  8, 19 }, /* H */
    { 0x1619,  9, 14 }, /* H */
    { 0x1699,  9, 13 }, /* H */
    { 0x1710,  9, 20 }, /* H */
    { 0x17C6,  8, 20 }, /* H */
    { 0x1868,  8, 20 }, /* H */
    { 0x190A,  8, 20 }, /* H */
    { 0x19AC,  7, 19 }, /* H */
    { 0x1A33,  8, 14 }, /* H */
    { 0x1AA5,  9, 13 }, /* H */
    { 0x1B1C,  8, 20 }, /* H */
    { 0x1BBE,  8, 20 }, /* H */
    { 0x1C60,  8, 20 }, /* H */
    { 0x1D02,  8, 20 }, /* H */
    { 0x1DA4,  8, 18 }, /* H */
    { 0x1E36,  9, 13 }, /* H */
    { 0x1ED5,  9, 13 }, /* H */
    { 0x1F4C,  8, 20 }, /* H */
    { 0x1FEE,  7, 20 }, /* H */
    { 0x207C,  7, 20 }, /* H */
    { 0x210A,  8, 20 }, /* H */
    { 0x21AC,  8, 18 }, /* H */
    { 0x223E,  9, 13 }, /* H */
    { 0x22B5,  9, 13 }, /* H */
    { 0x255C,  9, 14 }, /* H */
    { 0x25DC,  9, 14 }, /* H */
    { 0x265C,  9, 14 }, /* H */
    { 0x26DC,  9, 14 }, /* H */
    { 0x275C,  9, 13 }, /* H */
    { 0x27D3,  9,  8 }, /* H */
    { 0x281D,  9, 13 }, /* H */
    { 0x2894,  9, 14 }, /* H */
    { 0x2914,  8, 14 }, /* H */
    { 0x2986,  8, 14 }, /* H */
    { 0x29F8,  9, 14 }, /* H */
    { 0x2A78,  8, 13 }, /* H */
    { 0x2AE2,  9,  8 }, /* H */
    { 0x2B2C,  9, 13 }, /* H */
    { 0x2BBB,  7,  7 }, /* H */
    { 0x2BEE,  7,  7 }, /* H */
    { 0x2C39,  7,  7 }, /* H */
    { 0x2C6C,  7,  7 }, /* H */
    { 0x2CBD,  7,  7 }, /* H */
    { 0x2CF0,  7,  7 }, /* H */
    { 0x2D7F,  7,  7 }, /* H */
    { 0x2DDA,  7,  7 }, /* H */
    { 0x2E0D,  5, 12 }, /* H */
    { 0x2E69,  7,  7 }, /* H */
    { 0x2E9C,  7,  7 }, /* H */
    { 0x2ECF,  5, 12 }, /* H */
    { 0x2FC7,  4,  7 }, /* H */
    { 0x2FE5,  4,  6 }, /* H */
    { 0x2FFF,  4,  6 }, /* H */
    { 0x3019,  5,  6 }, /* H */
    { 0x3039,  8, 11 }, /* H */
    { 0x3093, 10, 12 }, /* H */
    { 0x3131,  3,  6 }, /* H */
    { 0x3145,  4,  9 }, /* H */
    { 0x316B,  6,  9 }, /* H */
    { 0x31A3,  6, 15 }, /* H */
    { 0x31FF,  7, 17 }, /* H */
    { 0x3278,  7, 18 }, /* H */
    { 0x3310,  4, 15 }, /* H */
    { 0x334E,  4, 16 }, /* H */
    { 0x3390,  5, 16 }, /* H */
    { 0x33E2,  4, 12 }, /* H */
    { 0x3426,  3,  5 }, /* H */
    { 0x3437,  5,  9 }, /* H */
    { 0x3466,  6, 11 }, /* H */
    { 0x3606, 13, 10 }, /* H */
    { 0x368A, 13,  7 }, /* H */
    { 0x36E7, 15, 11 }, /* H */
    { 0x378E, 13, 10 }, /* H */
    { 0x3812, 14,  7 }, /* H */
    { 0x3876, 15, 11 }, /* H */
    { 0x4C2A,  3,  7 }, /* C */
    { 0x4C41,  3,  7 }, /* C */
    { 0x4C58,  3,  7 }, /* C */
    { 0x4C6F,  3,  7 }, /* C */
    { 0x4C86,  3,  7 }, /* C */
    { 0x4C9D,  3,  7 }, /* C */
    { 0x4CB4,  3,  7 }, /* C */
    { 0x4CCB,  3,  7 }, /* C */
    { 0x4CE2,  3,  7 }, /* C */
    { 0x4CF9,  3,  7 }, /* C */
    { 0x4D10,  3,  7 }, /* C */
    { 0x4D27,  3,  7 }, /* C */
    { 0x4D3E,  3,  7 }, /* C */
    { 0x4D55,  3,  7 }, /* C */
    { 0x4D6C,  3,  7 }, /* C */
    { 0x4D83,  3,  7 }, /* C */
    { 0x4D9A,  3,  7 }, /* C */
    { 0x4DB1,  3,  7 }, /* C */
    { 0x4DC8,  3,  7 }, /* C */
    { 0x4DDF,  2,  7 }, /* C */
    { 0x4DEF,  3,  7 }, /* C */
    { 0x4E06,  3,  7 }, /* C */
    { 0x4E1D,  3,  7 }, /* C */
    { 0x4E34,  4,  7 }, /* C */
    { 0x4E52,  3,  7 }, /* C */
    { 0x4E69,  3,  7 }, /* C */
    { 0x4E80,  3,  7 }, /* C */
    { 0x4EAE,  3,  7 }, /* C */
    { 0x4EC5,  3,  7 }, /* C */
    { 0x4EDC,  3,  7 }, /* C */
    { 0x4EF3,  3,  7 }, /* C */
    { 0x4F0A,  3,  7 }, /* C */
    { 0x4F21,  4,  7 }, /* C */
    { 0x4F3F,  3,  7 }, /* C */
    { 0x4F56,  3,  7 }, /* C */
    { 0x4F6D,  3,  7 }, /* C */
    { 0x4FBA,  3,  7 }, /* C */
    { 0x4FD1,  1,  7 }, /* C */
    { 0x4FDA,  2,  7 }, /* C */
    { 0x4FEA,  2,  7 }, /* C */
    { 0x4FFA,  1,  2 }, /* C */
    { 0x4FFE,  1,  7 }, /* C */
    { 0x5007,  1,  7 }, /* C */
    { 0x503E,  2,  2 }, /* C */
    { 0x5044,  2,  7 }, /* C */
    { 0x5085,  2,  5 }, /* C */
    { 0x5091,  2,  5 }, /* C */
    { 0x509D,  2,  5 }, /* C */
    { 0x50A9,  2,  5 }, /* C */
    { 0x50B5,  2,  5 }, /* C */
    { 0x50C1,  2,  5 }, /* C */
    { 0x50CD,  2,  5 }, /* C */
    { 0x50D9,  2,  5 }, /* C */
    { 0x50E5,  2,  5 }, /* C */
    { 0x50F1,  2,  5 }, /* C */
    { 0x50FD,  2,  5 }, /* C */
    { 0x5109,  2,  5 }, /* C */
    { 0x5121,  2,  5 }, /* C */
    { 0x512D,  2,  5 }, /* C */
    { 0x5139,  2,  5 }, /* C */
    { 0x5145,  2,  5 }, /* C */
    { 0x515D,  2,  5 }, /* C */
    { 0x5169,  2,  5 }, /* C */
    { 0x518D,  2,  5 }, /* C */
    { 0x5199,  3,  5 }, /* C */
    { 0x51AA,  2,  5 }, /* C */
    { 0x51B6,  2,  5 }, /* C */
    { 0x51CE,  2,  5 }, /* C */
    { 0x51DA,  2,  5 }, /* C */
    { 0x51E6,  2,  5 }, /* C */
    { 0x51F2,  2,  5 }, /* C */
    { 0x5267,  2,  5 }, /* C */
    { 0x52BC,  2,  5 }, /* C */
    { 0x52DA,  6,  5 }, /* C */
    { 0xE80D,  3,  7 }, /* C */
    { 0xE822,  3,  7 }, /* C */
};
const int ROM_IMAGE_COUNT = (int)(sizeof ROM_IMAGES / sizeof ROM_IMAGES[0]);

#define SHEET_W 512
#define GAP 2

int rom_images_decode(const RomSet *rom, ImageSheet *out)
{
    memset(out, 0, sizeof *out);
    uint32_t rgb[16];
    for (int i = 0; i < 16; i++) rgb[i] = williams_rgb((uint8_t)rom_byte(rom, ROM_PALETTE + (unsigned)i));

    /* shelf packing in catalogue order */
    int x = 0, y = 0, shelf = 0;
    for (int i = 0; i < ROM_IMAGE_COUNT; i++) {
        int w = ROM_IMAGES[i].w * 2, h = ROM_IMAGES[i].h;
        if (x + w > SHEET_W) {
            x = 0;
            y += shelf + GAP;
            shelf = 0;
        }
        out->frames[i] = (ImageFrame){ x, y, w, h };
        x += w + GAP;
        if (h > shelf) shelf = h;
    }
    out->image.width = SHEET_W;
    out->image.height = y + shelf;
    out->image.rgba = calloc((size_t)out->image.width * (size_t)out->image.height, 4);
    if (!out->image.rgba) return -1;

    /* each image through the blitter, as the game draws it, into scratch RAM */
    static uint8_t ram[WILLIAMS_VRAM_SIZE];
    WilliamsBus bus = { ram, rom, 1 };
    for (int i = 0; i < ROM_IMAGE_COUNT; i++) {
        const RomImage *im = &ROM_IMAGES[i];
        memset(ram, 0, sizeof ram);
        williams_blit(&bus, BLIT_DST_STRIDE_256, 0, im->addr, 0x0000, (uint8_t)(im->w ^ 4), (uint8_t)(im->h ^ 4));
        const ImageFrame *f = &out->frames[i];
        for (int py = 0; py < f->h; py++)
            for (int px = 0; px < f->w; px++) {
                uint8_t b = ram[(px >> 1) * 256 + py];
                int idx = px & 1 ? b & 0x0F : b >> 4;
                if (!idx) continue; /* 0 is transparent */
                uint8_t *o = out->image.rgba + ((size_t)(f->y + py) * SHEET_W + (size_t)(f->x + px)) * 4;
                o[0] = (uint8_t)(rgb[idx] >> 16);
                o[1] = (uint8_t)(rgb[idx] >> 8);
                o[2] = (uint8_t)rgb[idx];
                o[3] = 0xFF;
            }
    }
    return 0;
}

void rom_images_free(ImageSheet *s)
{
    image_free(&s->image);
}
