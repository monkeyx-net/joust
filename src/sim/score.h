/*
 * Scores ($E6A8-$E71C, $8719-$874F): a rider's table +4 points at its
 * score block (player 1's $A04C: +0-3 the score as BCD, +4 a change flag
 * the score task watches, +5-6 the lives, +7-9 the next extra life), 0
 * for the enemies'. $E6A8 adds A (BCD) to +2 and carries up; $E6B1 adds
 * A's high nibble to +3 and its low nibble (with the carry) to +2. Then,
 * while <$97-<$98 (the CMOS's extra-life interval) is set and the score
 * has passed +7-9, a life ($8719: +5 and +6 up, kept from wrapping; its
 * icon blitted while it is the fifth or fewer, at the table's +6 + 3 x
 * lives - 3 from the picture at +8), the sound $EF67, audit counter 5,
 * and +7-9 raised by the interval (by $80 more when its middle byte
 * passes $30).
 */
#ifndef JOUST_SCORE_H
#define JOUST_SCORE_H

#include "sim/game.h"

/* $E6A8 (nibbles 0) or $E6B1 (nibbles 1): A points for the rider whose table is at x */
void score_add(Game *g, uint16_t x, uint8_t a, int nibbles);
/* $8719: a life for the rider whose table is at x */
void score_life(Game *g, uint16_t x);

#endif
