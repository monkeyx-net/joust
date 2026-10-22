/*
 * Burning and the lava's fire, tasks the waves start (sim/waves.c):
 *   - $81A0 (type $27): a platform burning away: its picture (+$E, a
 *     template as the pads', $65EE) flashed 5 times ($2A every other 10
 *     frames), an erase node ($8240), then 5 frames of pixels straight
 *     into video RAM ($8209 from $ED19, at the template's address, or
 *     $F145 / $3151 for the two whose templates are [$0000] / [$0004]),
 *     8 frames apart, and the erase node again. The first erase node's
 *     line comes from the CPU's Y as the step starts (left by the step
 *     before: `Game.y`).
 *   - $8638 (type $6A): a lava bridge: once the lava (<$9D) is up to line
 *     $DF, from +$2B (x) by +$2D a frame its top map bits ($B002 + x,
 *     bit 5) turned to the template's ($EBB8 + x); its flame (+$10 the
 *     frame of the records at [$002C]) drawn 2 frames at +$2B + +$14,
 *     until a bit was already right; then the flame sinks to line $EF,
 *     <$4B gets bit 7, the task ends.
 *   - $86D0 (type $69): the lava's flames at the sides, every 6 frames
 *     the next frame, rising to line $DF.
 */
#ifndef JOUST_BURN_H
#define JOUST_BURN_H

#include "sim/game.h"

int burn_ported(uint16_t at);
StepResult burn_step(Game *g, uint16_t task, uint16_t at);

#endif
