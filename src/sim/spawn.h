/*
 * A rider appearing on a pad ($87B0-$883D, $8B2D-$8D22). Four pads, the
 * records at $8A5D-$8A75 (8 bytes: [the pad's node template, through a
 * word at its first address][x][line][its use count <$6E-<$71]).
 *   - $87B0: <$6A-<$6D = the use counts, then each rider (type bit 7; a
 *     type $82 only with +$2B set) whose predicted x (+7 plus the table
 *     at the given table's first word by its speed index, wrapped) falls
 *     in a pad's zone for its line (the 7-byte zones after it: [outside?]
 *     [low][high][the pad's flag]) marks that pad busy.
 *   - $8B2D: a free pad, from one picked by two random bits, in turn; none
 *     free: a frame's wait ($8B15). Its count up, the rider put on it
 *     (+$20 the pad's record), <$39 or <$3B up (type 9 or not), the sound
 *     from the table's +$2C, +$10 = $1E frames, +$1A the rider's frames.
 *   - $8BAF, each frame: the pad drawn ($65EE) in the solid colour of
 *     the table's +$2E; then (from frame $14 down) the rider's nodes grown
 *     down a line or two at a time ($8BCB-$8C15) in that colour, and
 *     ($8C1C) its mount's node a solid erase over the pad's column.
 *   - Then on the pad ($8C56): the brain; the pad's rows shimmer through
 *     the colours at the table's +$C ($8CA5, +$26 the colour, +$15-16 the
 *     pace) until the controls move it (with the sound at +$2A) or +$35
 *     runs out; the pad drawn plain again, its count down, the table's
 *     +$14 routine (a player's: <$73 or <$75), and on to the ground
 *     ($8CF1, $8D50).
 */
#ifndef JOUST_SPAWN_H
#define JOUST_SPAWN_H

#include "sim/game.h"

#define SPAWN_GROW 0x8C1C
#define SPAWN_WAIT 0x8C56

int spawn_ported(uint16_t at);
StepResult spawn_step(Game *g, uint16_t task, uint16_t at);
/* $8B30: pick a pad (zones from `table`) and start appearing on it */
StepResult spawn_choose(Game *g, uint16_t u, uint16_t table);
/* $8B40-$8B78: the first free pad from `first` (0-3, round to 3 twice) */
StepResult spawn_from(Game *g, uint16_t u, int first);

#endif
