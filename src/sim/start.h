/*
 * The game's shell ($5EEB-$65ED): one task through the attract mode, the
 * start and the game, as the ROM chains it.
 *   - $6359/$6364 the start buttons: a coin (<$B3) shows its screen
 *     ($6301: the credits and "press start", 4 seconds, then back to the
 *     attract mode); a start with credits (<$F2, kept in the CMOS at
 *     $CD00; free play when $CC06 is 9) takes them, counts audits 8 or 9
 *     and 10, the lives from $CC02 (<$77), ends every other task and
 *     sets the game up ($63E5: the screen cleared, the palette, a line
 *     drawn from the ROM, the scores, <$97 the extra-life interval from
 *     $CC00, the settings by the difficulty's third from $DE11, the
 *     gravity <$48 = 4, the waves' counters, the platform maps from
 *     $E937/$EB98, the platforms' pictures ($E60A, a bit stream into video
 *     RAM), the pads, the wave task $6BC3, the collisions, the players
 *     ($8D23, their lives' icons), the lava, the ROM check, the scores, a
 *     sum of the program ROM ($6AC7: <$B2 up on a wrong one), the secret
 *     code), then redraws the platforms met (<$4B, $65AE) every 2 frames.
 *   - $61F0 game over: <$B3 = $80, the start buttons' task, the enemies
 *     to their first brains, "GAME OVER" 11 times 8 frames apart; then the
 *     high score entry ($3B61, the system ROM: not ported).
 *   - $627F the attract mode: the start buttons' task; with $CC05 bit 0
 *     the high score table ($3B43, not ported); a countdown ($62AD), held
 *     by player 1 right with player 2 left; then the attract pages ($D000,
 *     not ported) and a watch for those controls ($62DA).
 *   - $5EF8 the demo (from $5EEB: <$90 = $7F, 3 lives, two players): the
 *     game set up, then pages from $5FCA (5 bytes: [routine][the text
 *     block shown][its count of 30-frame turns][the block erased]),
 *     blocks of text ($4B2B-$4B5B, [$5D7B + 2n]) kept in +$18-+$1A and
 *     shown again every 2 frames; the routines start demo riders and set
 *     the demo players' brains (sim/brains.c).
 */
#ifndef JOUST_START_H
#define JOUST_START_H

#include "sim/game.h"

#define START_DEMO 0x5EEB
#define START_ATTRACT 0x624D /* (through $5ED3) after a game and its high scores */

int start_ported(uint16_t at);
StepResult start_step(Game *g, uint16_t task, uint16_t at);

/* $40BC (through $3B2E): video RAM ($0000-$9FFF) cleared from the top down */
void start_clear_screen(Game *g);
/* $E4B2 (through $E015): the palette's copy <$0F-<$1E from x */
void start_palette(Game *g, uint16_t x);
/* $6336 / $633A: the credits (<$F2) at the bottom, their box blitted
   with ctrl and solid ($12 $00, or as the caller's Y says) */
void start_credits(Game *g, uint8_t ctrl, uint8_t solid);

#endif
