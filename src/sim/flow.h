/*
 * The game's flow, so far its frequent paths: the start buttons' task
 * ($6359-$63E5: <$B3 a coin to show, then the start buttons at $C804;
 * with neither it sleeps a frame) and a countdown ($62A8-$62D7: +D-E
 * frames, <$11 counted up each; then a frame-by-frame look at both
 * players' controls, +$10 frames more, reset to $3C while player 1 holds
 * right and player 2 left). The paths that start things (a coin, a start,
 * the countdown's end: $6368, $6380, $62CE) are not ported.
 */
#ifndef JOUST_FLOW_H
#define JOUST_FLOW_H

#include "sim/game.h"

int flow_ported(uint16_t at);
StepResult flow_step(Game *g, uint16_t task, uint16_t at);

#endif
