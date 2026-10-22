/*
 * A player unhorsed ($8279 player 1, $827F player 2: the table's +$10
 * routine): the egg count (<$36 / <$37) cleared, the deaths (<$A5 / <$A6)
 * counted, <$AC = $25. A winning player gets the gladiator bounty when
 * its count ([its table + $18], <$A3/<$A4) reaches 0 from below: 3000
 * points, a task showing them ($7861) and the sound $EF80; an enemy's
 * win in a game with <$90 set and <$A0 sets <$B0 = 2. Then the death's
 * task ($8139, type $20), 50 points to the dead player, the ROM check at
 * $85C1 (a sum of $22 bytes from [$8636]; a wrong one corrupts the free
 * list), the sound $EFB7, the player out of <$73-<$76 (player 2 moved to
 * <$73), a life gone; the last one: with the other player in (<$52 |
 * <$5C) its turn ($834B), else game over ($61F0, after the list's first
 * task).
 */
#ifndef JOUST_DEATH_H
#define JOUST_DEATH_H

#include "sim/game.h"

/* $8279 (player2 0) / $827F: the player at u unhorsed by `winner` (0: none) */
void death_player(Game *g, uint16_t u, uint16_t winner, int player2);

/* The death's task ($8139-$8181: three pictures, [$002E], [$0030],
   [$0032], for 5, 8 and 12 frames, x kept within 4-$120, then the
   platform bits there to <$4B) and the other player's turn ($834B: its
   message, [table+$30], and "UP" in the small font at $296C, every 8
   frames for +$2D looks in a two-player game, then erased). */
int death_ported(uint16_t at);
StepResult death_step(Game *g, uint16_t task, uint16_t at);

#endif
