/*
 * The lava ($E4E6-$E5E6): one task first fills the lava's surface, a line
 * every 20 frames, from line $FF up to <$9B (the row in <$9D, the column
 * in <$9C), drawing $44 pairs in from each side of the screen until they
 * meet something; then it makes bubbles: a random column from the table
 * at <$B1 ($E5E9), and with the random carry either a bubble rising
 * (+$14-15 its screen address, three frames of 8: $40, then $04/$40, then
 * $44 back, with the sound at $E692) or a dip (cleared, then drawn back
 * over three frames). +E steps the bubble's column, +F the random number.
 * It writes the screen with the ROM unselected (`$BFFF`, `$C900`), as it
 * reads the screen too.
 */
#ifndef JOUST_LAVA_H
#define JOUST_LAVA_H

#include "sim/game.h"

#define LAVA_START 0xE4E6

int lava_ported(uint16_t at);
StepResult lava_step(Game *g, uint16_t task, uint16_t at);

#endif
