/*
 * The system program (the Williams system ROM and $F000-$FFFF, beside the
 * game): the reset's self-test, the CMOS's check and factory restore,
 * and the operator's test mode with its bookkeeping and adjustments. The
 * ROM runs these as straight code with busy waits and the interrupts off;
 * here they are a state machine stepped once a frame, the waits counted
 * in frames (a delay of $F945 with A = n is (n + 1) x $300 loops of 13
 * cycles, about 0.6 frames per unit; $F1D0's unit is about 25.7 frames),
 * the screens and the CMOS written as the ROM writes them.
 *
 *   - The reset ($F025): the PIAs, the palette from $F015, the RAM test
 *     (two passes of the generator over $0000-$BFFF: write, then check),
 *     the ROM test (each 4 KB block's sum against $F320's), "INITIAL TESTS
 *     INDICATE: OPERATIONAL" (blocks 5, 6) for 7 x $F1D0, then the CMOS's
 *     check ($3D36).
 *   - The CMOS's check ($3D36): the operator's message (its sum + $37 at
 *     $CC8E, else the default from $3C23) and the adjustments (their sum +
 *     $37 at $CC8C); bad adjustments are restored from $3C11, the audits
 *     cleared if 4 of them are not BCD, the tables reset ($3E09) if 8
 *     entries are bad, and a message (block $B, or $C) waits for the
 *     advance. Every time: the tables' bad entries deleted ($3FC1),
 *     today's table reset from $3F0C, the best one restored if bad. Then
 *     the game ($E000: sim/cgame.c's power-on).
 *   - The test mode (the advance switch, $F342; with the auto-up switch
 *     straight to the bookkeeping): the ROM test, the RAM test (until the
 *     advance), the CMOS test, the sound test (each command in turn), the
 *     switch test, the colour RAM test, the crosshatch, the screen in red,
 *     green and blue, the colour bars, the bookkeeping (the audits and
 *     their averages), the adjustments (player 1's stick picks one, player
 *     2's changes it; the coinage's presets set the rest), then their
 *     special entries ($3C57: clear the audits or the high scores, the
 *     operator's message and the best one's name entered with the
 *     initials input, the factory settings) and the CMOS's check.
 */
#ifndef JOUST_OPERATOR_H
#define JOUST_OPERATOR_H

#include "sim/game.h"

typedef struct {
    int pc;        /* where the program goes on */
    long wait;     /* frames left to wait */
    int count, n;  /* the ROM's counters for the step at pc */
    uint16_t seed; /* the RAM test's generator */
    uint16_t x, y, u;
    uint8_t a, b;
    int done;      /* 1: back to the game (cgame_power_on) */
} Operator;

/* The program from the reset (self-test, CMOS check). */
void operator_reset(Operator *o);
/* The test mode, from the advance switch. */
void operator_test_mode(Operator *o);
/* The CMOS's check alone (a reboot): then the game. */
void operator_cmos_check(Operator *o);
/* A frame of it. */
void operator_frame(Operator *o, Game *g);

/* Its routines, each as the ROM's (the tests call them alone): */
int operator_cmos_check_now(Game *g);     /* $3D36: 1 if a message waits for the advance */
void operator_tables_validate(Game *g);   /* $3FC1 */
void operator_tables_reset(Game *g);      /* $3E09 */
int operator_cmos_test(Game *g);          /* $FE4A: 1 if the CMOS held */
void operator_sound_show(Game *g);        /* $FDDC's page */
void operator_sound_step(Game *g);        /* $FDDC whole, the advance not pressed */
void operator_switches(Game *g);          /* $F781-$F797: one look */
void operator_colour_bars(Game *g);       /* $F715 */
void operator_bars(Game *g);              /* $F4EA */
void operator_crosshatch(Game *g);        /* $F53A */
uint8_t operator_average(Game *g);        /* $FA27 */
void operator_bookkeeping(Game *g);       /* $F962-$FA20 */
void operator_adjust_value(Game *g, uint16_t u, uint16_t y); /* $FCBA */
void operator_adjust_page(Game *g);       /* $FB17-$FB6F */
void operator_cursor_move(Game *g, uint8_t a, uint16_t *y, uint16_t *u); /* $FBCD-$FC13 */
void operator_value_change(Game *g, uint8_t a, uint16_t u, uint16_t y); /* $FC51-$FC92 */

#endif
