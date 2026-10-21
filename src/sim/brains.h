/*
 * The enemies' brains ($7A83-$80D5): state machines, a state's address at
 * the rider's +$24; each state sets the controls in <$32-<$33 (direction,
 * flap), usually moves +$24 to the next state, and returns. Three families
 * share the exits ($7BD3/$7E25/$802F: the direction from the facing, a
 * push at +$22 taking it over; $7C7D/$7C87: flap above the lava): $7A83
 * onward (the first wave's), $7C95 onward and $7ECF onward, each starting
 * over at its first state. They read the wave's settings ($B1B4-$B1FF),
 * the riders' positions, the platform maps (to fly round an edge),
 * +$2B-2C a remembered line or velocity, +$2D a countdown, +$2E/+$2F a
 * turn, +$32 frames to the next look at a target, and $80D6 for the
 * nearer player (<$73, <$75; <$A8/<$A9 say who is out). $7AD5 and $7C90
 * jump to the start in the rider's table (+2). Not ported yet: $793C (a
 * riderless mount walking to its knight, picking it up at $7A2F).
 */
#ifndef JOUST_BRAINS_H
#define JOUST_BRAINS_H

#include "sim/game.h"

/* Run the brain state at `at` for the rider at u: 0 controls set, -1 a
   state not ported. */
int brain_enemy(Game *g, uint16_t u, uint16_t at);
/* $80D6: the player to go for: 1 and *x its block, or 0 (*x as the ROM leaves X). */
int brain_target(Game *g, uint16_t u, uint16_t *x);

#endif
