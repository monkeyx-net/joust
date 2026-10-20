/*
 * The game's images, decoded from the user's ROM at run time (decision 1:
 * nothing ROM-derived is compiled in or shipped).
 */
#ifndef JOUST_IMAGES_H
#define JOUST_IMAGES_H

#include <stdint.h>
#include "assets/png.h"
#include "assets/rom.h"

#define ROM_PALETTE 0xE5FA /* the in-game palette, 16 bytes (found by matching MAME's palette RAM) */
#define MAX_ROM_IMAGES 256

typedef struct {
    uint16_t addr; /* first data byte */
    uint8_t w, h;  /* width in bytes (2 pixels each), height in rows */
} RomImage;

extern const RomImage ROM_IMAGES[];
extern const int ROM_IMAGE_COUNT;

typedef struct {
    int x, y, w, h; /* in pixels on the sheet */
} ImageFrame;

typedef struct {
    Image image;                     /* RGBA, palette index 0 transparent */
    ImageFrame frames[MAX_ROM_IMAGES]; /* one per ROM_IMAGES entry */
} ImageSheet;

/* Decode every catalogued image through the blitter and the ROM's palette. */
int rom_images_decode(const RomSet *rom, ImageSheet *out);
void rom_images_free(ImageSheet *s);

#endif
