/*
 * Riders waiting for their turn to appear ($8B0E takes a number, <$3A,
 * into +$16; $8B1A waits for <$38 = <$39 and its number in <$3B, then
 * picks a pad, $8B2D; a player, type 9, waits for <$39 and takes the pad
 * in the emptiest third, $8ABA), and the secret code ($6B20-$6BA5): both
 * players' controls must show $2502, then $1204, then $0506 ($6B90 keeps
 * the wanted controls in +D and where to carry on in +F, and looks every
 * frame at $6B9B); a wrong position starts over after 30 frames; the
 * message at the end ($6B4C, then a restart) is not ported. The start
 * buttons and the countdowns that were here are in sim/start.c.
 */
#ifndef JOUST_FLOW_H
#define JOUST_FLOW_H

#include "sim/game.h"

int flow_ported(uint16_t at);
StepResult flow_step(Game *g, uint16_t task, uint16_t at);

#endif
