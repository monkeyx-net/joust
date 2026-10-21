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
    /* optional, for tests: called before each RAM write the C game makes */
    void (*on_write)(void *user, uint16_t addr);
    void *user;
} Game;

typedef enum {
    STEP_UNPORTED = 0, /* the task (or the path it took) is not ported: the state is unspecified */
    STEP_DONE = 1,     /* it slept or ended */
} StepResult;

/* The whole state from a RAM image of $0000-$BFFF and the ROM select. */
void game_load(Game *g, const uint8_t *ram, uint8_t romsel);
/* A byte as the main CPU reads it: ROM under $0000-$8FFF when selected,
   RAM below $C000, ROM from $D000. (I/O reads are not modelled.) */
uint8_t game_read(const Game *g, uint16_t addr);
uint16_t game_read_word(const Game *g, uint16_t addr);
/* A RAM byte or big-endian word. */
uint8_t game_peek(const Game *g, uint16_t addr);
void game_poke(Game *g, uint16_t addr, uint8_t value);
uint16_t game_word(const Game *g, uint16_t addr);
void game_set_word(Game *g, uint16_t addr, uint16_t value);
/* `STA $BFFF; STA $C900`: the ROM select and its copy in RAM. */
void game_select_rom(Game *g, uint8_t value);

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
