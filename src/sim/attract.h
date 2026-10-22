/*
 * The attract mode's title page ($D000-$D35C): the screen cleared, the
 * palette from $D224, the operator's two lines from the CMOS ($44D4:
 * column bytes at $CC88/$CC8A, 25 characters each from $CC24/$CC56), the
 * credits, the extra life interval; then the logo ($D070: polylines from
 * $D35D drawn pixel by pixel into video RAM, $D2A8, scratch at $BC00-
 * $BC1F, some filled down to the next set pixel, $D254), lights running
 * round the border ($D15B: 16-pixel strips in colours 8-15, a frame's
 * sleep every third, the loop's place kept in +$13 as the ROM's return
 * address, and the CPU's B as the task started in +$12: `Game.b`) and the
 * palette swept ($D0ED: <$17-<$1E along by one every 2 frames, new
 * colours from $D22C's rows once the lights are done); after $6F x 10
 * frames the demo ($5EEB).
 */
#ifndef JOUST_ATTRACT_H
#define JOUST_ATTRACT_H

#include "sim/game.h"

#define ATTRACT_PAGES 0xD000

int attract_ported(uint16_t at);
StepResult attract_step(Game *g, uint16_t task, uint16_t at);

#endif
