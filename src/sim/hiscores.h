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
 *   - $464F (through $3B61) after "game over", when $CC05 bit 0 allows:
 *     scores capped at 99999999; for each player (the higher first) a
 *     task: the best ever ($48E9, type $42: a name of up to [$CC16]
 *     letters, then its initials) or a place in today's table or the
 *     table ($4765, type $41 or $42: 3 initials); <$F5 counts them. The
 *     input ($43F6/$4438 every 3 frames, a block at +7: left/right step
 *     the letter $0A-$25, flap takes it, $25 rubs one out; A and B kept in
 *     +$32 as the ROM's registers) goes on at [+$C]. The scores go in
 *     ($48B8: entries moved down, nibble pairs, a check nibble at +6;
 *     the best one's own check at $CD6C), at most 5 with the same
 *     initials (4 when the best has them): the oldest beyond goes ($4091)
 *     with a message. Then ($3B64) the screen cleared, "high scores"
 *     messages, until the entries are done or 255 x 30 frames pass.
 */
#ifndef JOUST_HISCORES_H
#define JOUST_HISCORES_H

#include "sim/game.h"

/* $414F: the high score table's page */
void hiscores_table(Game *g);
/* $FF87 (through $F00F): the border */
void hiscores_border(Game *g);

int hiscores_ported(uint16_t at);
StepResult hiscores_step(Game *g, uint16_t task, uint16_t at);
/* $464F: the entries after a game (the game over task) */
StepResult hiscores_game_over(Game *g, uint16_t task);

#endif
