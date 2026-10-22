/*
 * The lava troll ($D9FC, $69DA-$6B1F, $D7C8-$D7F9, $D916): a player or a
 * rider of type $8F at the lava's line, with no hand out (<$96) and the
 * lava up (<$A0 0), gets a hand: a task ($69DD, type $15) just before
 * it on the list, +$24 the rider, rising (+$10 the frame of [$002A]'s
 * records, 6 a step every [$B1AE] frames) while the rider stays over a
 * pit (x - 2 at most $28 or from $F0, line $C6 on, not on the ground:
 * $6A9B), sinking back ($6AD9) when it leaves; at the top it climbs to the
 * rider's line + 3 and grabs ($6A29: the sound $EF83, the rider's motion
 * +$30 to $D916, a grip task $6A66 of type $16 after the rider drawing
 * the hand on it, the pull <$B302 from $B1B0 growing by 1 every $708
 * frames to $500, $D7C8). Held ($D916): the velocity is the motion's
 * gravity, the pull and its own; one past $FE80 up escapes (50 points
 * for a rider of type bit 7, the hand back down, $6AE3), one past line
 * $E6 is the lava ($D899); x kept within 8-$116.
 */
#ifndef JOUST_TROLL_H
#define JOUST_TROLL_H

#include "sim/game.h"

int troll_ported(uint16_t at);
StepResult troll_step(Game *g, uint16_t task, uint16_t at);
/* $DA0E: a hand for the rider at u */
void troll_hand(Game *g, uint16_t u);
/* $D916: the held rider's motion (b the caller's gravity, 0 or 4): 0 on,
   1 into the lava (the step ended, *r) */
int troll_held(Game *g, uint16_t u, uint8_t b, StepResult *r);

#endif
