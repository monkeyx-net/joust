/*
 * The Joust main board (Williams first generation), as MAME's williams.cpp
 * models it: a 1 MHz 6809, 48 KB RAM ($0000-$BFFF, the first 38 KB shown on
 * screen), program ROM under it at $0000-$8FFF when $C900 bit 0 is set and
 * at $D000-$FFFF, palette $C000-$C00F, PIAs at $C804 (inputs) and $C80C
 * (coins, interrupts, sound commands), the blitter at $CA00, the video
 * counter at $CB00, 1 KB of 4-bit CMOS at $CC00-$CFFF.
 *
 * A frame is 260 lines of 64 CPU cycles, from line 247 (vertical blank) to
 * line 246. VA11 (scanline bit 5) drives CB1 of the second PIA, interrupting
 * 4 times a frame; COUNT240 drives its CA1. Each visible line is drawn into
 * the framebuffer as the beam passes it. Instruction for instruction and
 * cycle for cycle this matches MAME (the mame_emu test).
 */
#ifndef JOUST_BOARD_H
#define JOUST_BOARD_H

#include <stdint.h>

#include "assets/rom.h"
#include "emu/m6809.h"
#include "emu/pia6821.h"
#include "gfx/fb.h"
#include "gfx/williams.h"

#define BOARD_LINES 260
#define BOARD_CYCLES_PER_LINE 64
#define BOARD_FIRST_LINE 247 /* frames start at vertical blank, as MAME's do */
#define BOARD_CMOS_SIZE 1024

/* Inputs, active high, as MAME's ports */
#define IN0_START2 0x10
#define IN0_START1 0x20
#define INP_LEFT 0x01
#define INP_RIGHT 0x02
#define INP_FLAP 0x04
#define IN2_AUTO_UP 0x01
#define IN2_ADVANCE 0x02
#define IN2_COIN3 0x04
#define IN2_HIGH_SCORE_RESET 0x08
#define IN2_COIN1 0x10
#define IN2_COIN2 0x20
#define IN2_TILT 0x40

typedef struct Board Board;

struct Board {
    M6809 cpu;
    const RomSet *rom;
    uint8_t ram[WILLIAMS_VRAM_SIZE];
    uint8_t palette[16];
    uint8_t cmos[BOARD_CMOS_SIZE];
    int cmos_dirty;
    uint8_t romsel;          /* $C900 */
    uint8_t blit[8];         /* $CA00-$CA07 as written */
    Pia pia[2];
    int mux_select;          /* PIA 0 CB2: which player's controls PA 0-3 shows */
    int scanline;
    long frame;
    uint8_t in0, inp1, inp2, in2;
    /* the sound board's command latch (PIA 1 port B, top bits set) */
    uint8_t sound_cmd;
    int sound_cmd_new;
    /* optional: called on every CPU write (tests trace RAM through this) */
    void (*write_hook)(Board *b, uint16_t addr, uint8_t value);
    /* optional: called at the start of each line (scripted input timing) */
    void (*line_hook)(Board *b, int line);
    /* optional: called before each CPU step (instruction traces) */
    void (*step_hook)(Board *b);
    void *user;
};

/* Power on; cmos = saved CMOS (1024 bytes) or NULL for a blank one. */
void board_init(Board *b, const RomSet *rom, const uint8_t *cmos);
/* Run one frame; draws the visible lines into fb (scale 1) if not NULL. */
void board_run_frame(Board *b, Framebuffer *fb);

#endif
