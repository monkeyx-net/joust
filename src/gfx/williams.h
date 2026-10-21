/*
 * Williams first-generation video, as MAME emulates it (williams_v.cpp):
 *   video RAM $0000-$BFFF: 192 columns of 256 bytes; byte (x/2)*256 + y holds
 *     pixels x (high nibble) and x+1 (low nibble), 4 bits per pixel
 *   palette $C000-$C00F: one byte per entry, BBGGGRRR through resistor
 *     networks (R, G: 1200/560/330 ohms; B: 560/330 ohms)
 *   visible area: x 6..297, y 7..246 (292 x 240, the framebuffer)
 *   blitter $CA00-$CA07 (SC1): control, solid colour, source, destination,
 *     width, height; width and height are written XOR 4 (the SC1 quirk)
 */
#ifndef JOUST_WILLIAMS_H
#define JOUST_WILLIAMS_H

#include <stdint.h>
#include "assets/rom.h"
#include "gfx/fb.h"

#define WILLIAMS_VRAM_SIZE 0xC000
#define WILLIAMS_VISIBLE_X 6
#define WILLIAMS_VISIBLE_Y 7

/* Blitter control bits */
#define BLIT_SRC_STRIDE_256 0x01 /* source in screen layout (columns of 256) */
#define BLIT_DST_STRIDE_256 0x02 /* destination in screen layout */
#define BLIT_SLOW 0x04
#define BLIT_FOREGROUND 0x08     /* skip zero (transparent) source pixels */
#define BLIT_SOLID 0x10          /* draw the solid colour instead of the source */
#define BLIT_SHIFT 0x20          /* shift right one pixel (odd x) */
#define BLIT_NO_ODD 0x40         /* keep the destination's odd (low-nibble) pixels */
#define BLIT_NO_EVEN 0x80        /* keep the destination's even (high-nibble) pixels */

/* What the blitter sees: RAM below $C000 (writes always land there) and
   the program ROM, under RAM at $0000-$8FFF when romsel is set. */
typedef struct {
    uint8_t *ram;        /* WILLIAMS_VRAM_SIZE bytes */
    const RomSet *rom;
    int romsel;          /* $C900 bit 0 */
    /* optional: told of each byte the blitter writes, before it lands (tests) */
    void (*on_write)(void *ctx, unsigned addr, uint8_t value);
    void *ctx;
    /* optional: program ROM by address, read instead of `rom` (the C game's) */
    const uint8_t *rom_image;
} WilliamsBus;

/* One blit as a write to $CA00 starts it: registers as the game writes them
   (w_reg and h_reg before the XOR 4). Same algorithm as MAME's blitter_core. */
void williams_blit(WilliamsBus *bus, uint8_t ctrl, uint8_t solid, uint16_t src, uint16_t dst, uint8_t w_reg,
                   uint8_t h_reg);

/* A palette byte as 0xRRGGBB. */
uint32_t williams_rgb(uint8_t entry);
/* The visible part of video RAM through the palette into fb (any scale). */
void williams_draw_vram(Framebuffer *fb, const uint8_t *vram, const uint8_t palette[16]);

#endif
