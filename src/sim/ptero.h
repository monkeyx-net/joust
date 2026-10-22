/*
 * The pterodactyl ($6640-$69BE): a task of type $97 (type $17 | $80 while
 * it can collide), +$D its table ($8A14), the riders' fields for its
 * position and motion (+7 x, +B the line, +$11 the vertical velocity,
 * +$14 the speed index: 4 or 8 a side, at $69CF), +$10/+$15/+$17 the wing
 * beat ($69BF: frame, frames), +$34 set for one the wave manager made.
 *   - start ($68A8, $68A5 clearing +$34): its fields cleared, a side by
 *     the random generator, a line away from the players ($D800 counts
 *     them by thirds of the screen), the sound $EF78
 *   - each frame ($6925): the brain at +$24 (below), +$13 counted, the
 *     erase node, the wing beat, the flight ($D861, no gravity), the
 *     picture ([$0034] by frame and facing) and box (clamped at line
 *     $D2), the draw node; with no enemies left (<$AA) it flies off
 *     ($67B3); the platforms beside it ($DA3E)
 *   - the brain: $6640 every +$2F frames picks a player ($80D6) and goes
 *     for its line and x (+$2E, +$2B), else for the middle of its third
 *     ($66F4); level with it, a swoop ($6695, sound $EF7D); $6795 a climb;
 *     $67B3 off the side of the screen ($6808: its task ends)
 *   - killed ($74AE, +$10 of its table): brain $6828, the death: blinks
 *     ($6819-$6853), then its body as pixels in video RAM ($6895,
 *     $8209 from [$0038]), then the points ($7861: not ported)
 */
#ifndef JOUST_PTERO_H
#define JOUST_PTERO_H

#include "sim/game.h"

int ptero_ported(uint16_t at);
StepResult ptero_step(Game *g, uint16_t task, uint16_t at);
/* the brain state at +$24 ($6640, $6695, $6795, $67B3): 0 its speed and
   velocity set (<$32-<$33 cleared), -1 a state that ends the step (off
   the screen at $6808, the death) or not the pterodactyl's */
int ptero_brain(Game *g, uint16_t u);
/* $74AE: the pterodactyl at u killed */
void ptero_killed(Game *g, uint16_t u);

#endif
