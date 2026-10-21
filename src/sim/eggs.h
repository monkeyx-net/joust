/*
 * Eggs ($75B0-$7660, $7611-$76CF): an egg's task. In flight ($7619 every
 * <$B0 frames): its picture's erase ($6AF3), its motion ($75B0: gravity,
 * the ceiling, x by the speed table and $DDD2, wrapped at $120 and 4 for
 * an egg, its box), the platforms beside it ($779B into <$4B; $DA3E's
 * bounce when its maps meet), landing on one ($D9C2: then its speed
 * index slowed by 2, and a falling egg bounces up at a quarter of its
 * speed until it is slow and still), or the lava's line ($E3: lost, one
 * enemy fewer, <$AA). At rest ($76A7 every 12 frames) +$2D counts down
 * ($B1B7 frames) to hatching, when <$A1 is below <$A2 ($76D0: the
 * knight's task $8EAF, not ported); an egg pushed off its platform falls
 * again. Its picture ($78E3): a frame by its facing and vertical speed
 * from the records at [$0028], clipped at the lava ($D8F4).
 */
#ifndef JOUST_EGGS_H
#define JOUST_EGGS_H

#include "sim/game.h"

int eggs_ported(uint16_t at);
StepResult eggs_step(Game *g, uint16_t task, uint16_t at);

#endif
