/*
 * The high scores (the system ROM, $3B43-$4A10): tables in the CMOS, an
 * entry 14 bytes as nibbles ([3 initials][4 BCD score bytes]): the best
 * ever at $CD3E (20 characters of name) and $CD6C, then 39 more from
 * $CD74 to $CF95; today's six from $CFA4.
 *   - $414F (through $3B43) the table's page: the border ($FF87: solid
 *     blits in colours stepping by $11 from $77), text block $0F, the best
 *     one's name (the spaces after its last letter not counted) and score,
 *     three columns of 13 ranks in the large font, today's two columns of
 *     3 in the small; the newest entries (<$EA/<$EC player 1's, <$EE/<$F0
 *     player 2's) in their colours; a colour cycle task ($F012, type $40).
 */
#ifndef JOUST_HISCORES_H
#define JOUST_HISCORES_H

#include "sim/game.h"

/* $414F: the high score table's page */
void hiscores_table(Game *g);
/* $FF87 (through $F00F): the border */
void hiscores_border(Game *g);

#endif
