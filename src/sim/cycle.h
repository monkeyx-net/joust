/*
 * The colour cycle ($FF1C-$FF86, started at $F012): a task keeping 8
 * colours at +$D-+$14 (from $FFE0) copied to the palette's RAM copy
 * <$17-<$1E every 4 frames; one entry (+$16, 0-7) shows white ($E8) while
 * its colour waits in +$17, the entry moving on each time, and every 8th
 * move the eight rotate by one.
 */
#ifndef JOUST_CYCLE_H
#define JOUST_CYCLE_H

#include "sim/game.h"

int cycle_ported(uint16_t at);
StepResult cycle_step(Game *g, uint16_t task, uint16_t at);

#endif
