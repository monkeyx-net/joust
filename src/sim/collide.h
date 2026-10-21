/*
 * Collisions between the riders ($837A-$8417, $DC21): two tasks walk the
 * task list every frame. One ($837F) pairs each task whose type has bit 7
 * set with every such task after it on the list whose type shares bit 0;
 * the other ($83A4, after counting <$A8 and <$A9 down) takes each task of
 * type $89 against every task of type bit 7 after it. A pair goes through
 * $83D8: their boxes (+7 x, +$29 its right edge, +$27/$28 the top and
 * bottom lines, 16-bit x, the difference kept in <$34), then their
 * collision data ([+$1C], the rows' left and right edges as words, offset
 * by the lines between their tops) through $DC21. A collision goes on to
 * the handlers at $8418 (who wins, eggs, bounces): not ported yet, so a
 * step that meets one is a path not ported.
 */
#ifndef JOUST_COLLIDE_H
#define JOUST_COLLIDE_H

#include "sim/game.h"

int collide_ported(uint16_t at);
StepResult collide_step(Game *g, uint16_t task, uint16_t at);
/* $DC21: do the collision data at x and y (row spans) overlap, x's
   shifted by `dx`? The carry the ROM returns. */
int collide_rows(const Game *g, uint16_t x, uint16_t y, uint16_t dx);
/* The same, leaving *px and *py where the ROM's X and Y end (the row that
   overlapped, which the platforms' handlers look at). */
int collide_rows_at(const Game *g, uint16_t *px, uint16_t *py, uint16_t dx);
/* $83D8: the boxes and data of tasks u and x (writes <$49 and <$34): 0
   apart, -1 a collision (where the ROM goes on to its handlers). */
int collide_pair(Game *g, uint16_t u, uint16_t x);

#endif
