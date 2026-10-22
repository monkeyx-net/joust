/*
 * The C game's state: the arcade's RAM, as the ROM lays it out ($0000-$BFFF,
 * raw bytes named in the code by their address until they get types of
 * their own), and the ROM select latch ($C900 bit 0, which puts program ROM
 * under $0000-$8FFF for reads). `rom` is the program ROM by main-CPU
 * address, the data the ported routines read; the tests fill it from the
 * ROM set (the game will from assets/, plan decision 1).
 *
 * Task steps: game_step() runs the C version of the task whose block is
 * `task` from where it resumes, until it sleeps or ends, as the ROM's
 * dispatcher would ($E152 `STU <$2E; JMP [5,U]`).
 */
#ifndef JOUST_GAME_H
#define JOUST_GAME_H

#include <stdint.h>

#define GAME_RAM 0xC000

typedef struct {
    uint8_t ram[GAME_RAM];
    const uint8_t *rom; /* 64 KB by address: $0000-$8FFF and $D000-$FFFF hold program ROM */
    uint8_t romsel;     /* $C900 as last written */
    /* the controls as the board's PIA 0 shows them: IN0's start buttons
       (bits 4-5 of $C804), and each player's (bits 0-3, by the
       multiplexer: CB2, set through $C807, high for player 1) */
    uint8_t in0, inp1, inp2;
    uint8_t pia0_crb;   /* $C807 */
    int mux;            /* CB2 as $C807 last set it */
    uint8_t cmos[1024]; /* $CC00-$CFFF: nibbles, | $F0 as the board keeps them */
    /* the CPU's Y as a task step starts: what the step before left; one
       step reads it ($81C4, a platform's burn), the tests copy the board's */
    uint16_t y;
    /* optional, for tests: called before each RAM write the C game makes,
       and on each RAM read */
    void (*on_write)(void *user, uint16_t addr);
    void (*on_read)(void *user, uint16_t addr);
    void *user;
} Game;

typedef enum {
    STEP_UNPORTED = 0, /* the task (or the path it took) is not ported: the state is unspecified */
    STEP_DONE = 1,     /* it slept or ended */
} StepResult;

/* The whole state from a RAM image of $0000-$BFFF and the ROM select. */
void game_load(Game *g, const uint8_t *ram, uint8_t romsel);
/* A byte as the main CPU reads it: ROM under $0000-$8FFF when selected,
   RAM below $C000, ROM from $D000; of the I/O, the controls ($C804), PIA
   0's control register B ($C807) and the CMOS; other I/O reads give 0. */
uint8_t game_read(const Game *g, uint16_t addr);
uint16_t game_read_word(const Game *g, uint16_t addr);
/* A RAM byte or big-endian word. A poke at $C807 sets PIA 0's control
   register B (and the multiplexer), at $CC00-$CFFF the CMOS; other I/O
   is not modelled. */
uint8_t game_peek(const Game *g, uint16_t addr);
void game_poke(Game *g, uint16_t addr, uint8_t value);
uint16_t game_word(const Game *g, uint16_t addr);
void game_set_word(Game *g, uint16_t addr, uint16_t value);
/* `STA $BFFF; STA $C900`: the ROM select and its copy in RAM. */
void game_select_rom(Game *g, uint8_t value);

/* A blit as a write to $CA00 starts it (registers as the game writes them,
   w_reg and h_reg before the XOR 4), on the C game's RAM: gfx/williams.c's
   blitter, which the board's is too. Each byte goes through on_write. */
void game_blit(Game *g, uint8_t ctrl, uint8_t solid, uint16_t src, uint16_t dst, uint8_t w_reg, uint8_t h_reg);

/* An 8-bit add with carry in, as ADDA/ADCA: the sum, and *c and *h its
   carry and half carry (for DAA). */
uint8_t game_add(uint8_t a, uint8_t b, int cin, int *c, int *h);
/* DAA after an add with carry c and half carry h: *c gets the new carry. */
uint8_t game_daa(uint8_t a, int *c, int h);
/* $4110 (via $3B2B): CMOS audit counter n (3 BCD bytes as 6 nibbles at
   $CCFC + 6n) plus 1. */
void game_audit(Game *g, uint8_t n);

/* $40E1 (through $3B31): a byte from the two CMOS nibbles at x */
uint8_t game_cmos_byte(const Game *g, uint16_t x);

/* $6BA6: both players' controls through the multiplexer: A player 1's
   and the start buttons (& $37), B player 2's (& $07). */
uint16_t game_controls(Game *g);

/* $E4C0 (through $E018): the random generator's step on <$0D-<$0E (sim/prng.h).
   Returns A; *carry (if not NULL) gets the carry the callers use. */
uint8_t game_random(Game *g, int *carry);
/* $E4CD (through $E02D): ask for the sound whose script is at `script`: its
   first byte is the priority (bit 7: always); the frame loop sends it. */
void game_sound(Game *g, uint16_t script);

/* 1 if the task at `task` resumes at a ported step. */
int game_step_ported(const Game *g, uint16_t task);
/* Run the task at `task` from its resume address until it yields. */
StepResult game_step(Game *g, uint16_t task);

#endif
