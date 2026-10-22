/*
 * The riders (the players and the enemies on their mounts): a task each,
 * its block holding +7-8 x (pixels), +A-B the line as 8.8 (+B the line),
 * +D a table in ROM ($88EE player 1's, $8981-$89E3 the enemies'), +F the
 * facing ($FF left), +$11-12 the vertical velocity, +$13 frames since the
 * last flap, +$14 an index into the speed table (-8..8, signed), +$15 a
 * flap's frames, +$16 x's fraction, +$17 the frame, +$18/+$1A the frame
 * tables (+$1A the rider, 0 for none), +$1C/+$1E the pictures drawn,
 * +$20 the running animation, +$22 a push (a bounce's), +$23 a hit on the
 * ceiling, +$24 the brain (the player's controls, $DDB2 / $DDB9, or an
 * enemy's), +$26-29 the box, +$30 the motion ($D858 in the air).
 *
 * In the air ($8F04 while the flap is held, $8F3D gliding): $D83D (the
 * brain, the facing, +$13, the pictures' erase nodes, $8E8E), on a new
 * flap $D813 (lift and speed), the motion, the frame ($8E05), the
 * platforms below ($D9A3: none, keep flying), the pictures' draw nodes
 * ($8E4D), a sleep of the table's +$1A frames. Ported for the players'
 * brains in the air; landing ($8F87), the lava ($D899), a platform's
 * classes ($D9C2 on) and the enemies' brains are paths not yet ported.
 */
#ifndef JOUST_RIDER_H
#define JOUST_RIDER_H

#include "sim/game.h"

#define RIDER_FLAPPING 0x8F04
#define RIDER_GLIDING 0x8F3D

int rider_ported(uint16_t at);
StepResult rider_step(Game *g, uint16_t task, uint16_t at);

/* $D861: on in the air at vertical velocity vel (8.8 lines), x by the
   speed table `speeds` (8.8 pixels by +$14): 0, or -1 into the lava (not
   ported) */
int rider_fly(Game *g, uint16_t u, uint16_t vel, uint16_t speeds);
/* $8E05: frame `frame`, its box (+$26-29) and pictures (+$1C/+$1E) */
void rider_frame(Game *g, uint16_t u, uint8_t frame);
/* $DDD2: x moved by dx, with the push at +$22 and the screen's wrap */
void rider_move_x(Game *g, uint16_t u, uint8_t dx);
/* $8E8E (chain 0: erase) / $8E4D (chain 1: draw): the pictures' nodes;
   returns the last node filled (the mount's), 0 if none */
uint16_t rider_pictures(Game *g, uint16_t u, int chain);
/* the brain at +$24: 0 its controls in <$32-<$33, 1 it ended the step
   (*slept), -1 a brain not ported */
int rider_brain(Game *g, uint16_t u, StepResult *slept);
/* $8DF9: the standing box (13 lines) and the pictures for the frame */
void rider_stand_box(Game *g, uint16_t u);
/* $8D50: on the ground with the controls in <$32-<$33 */
StepResult rider_ground_on(Game *g, uint16_t u);

#endif
