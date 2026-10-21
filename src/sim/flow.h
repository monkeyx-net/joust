/*
 * The game's flow, so far its frequent paths: the start buttons' task
 * ($6359-$63E5: <$B3 a coin to show, then the start buttons at $C804;
 * with neither it sleeps a frame) and a countdown ($62A8-$62D7: +D-E
 * frames, <$11 counted up each; then a frame-by-frame look at both
 * players' controls, +$10 frames more, reset to $3C while player 1 holds
 * right and player 2 left). The paths that start things (a coin, a start,
 * the countdown's end: $6368, $6380, $62CE) are not ported. Also the idle
 * paths of: $65AE (the platforms met, <$4B, looked at every 2 frames),
 * $62DA/$62DF (both players' controls every 5 frames, for player 1 right
 * and player 2 left together: $626B, not ported) and $8B0E/$8B1A (a rider
 * waiting for its turn to appear: $8B0E takes a number, <$3A, into +$16;
 * $8B1A waits for <$38 = <$39 and its number in <$3B; then $8B2D, not
 * ported; type 9 goes to $8ABA, not ported), and a secret code
 * ($6B20-$6BA5): both players' controls must show $2502, then $1204,
 * then $0506 ($6B90 keeps the wanted controls in +D and where to carry on
 * in +F, and looks every frame at $6B9B); a wrong position starts over
 * after 30 frames; the message at the end ($6B4C) is not ported.
 */
#ifndef JOUST_FLOW_H
#define JOUST_FLOW_H

#include "sim/game.h"

int flow_ported(uint16_t at);
StepResult flow_step(Game *g, uint16_t task, uint16_t at);

#endif
