/*
 * The waves ($6BC3-$6D90, $6E01-$6FCA, $71C8-$7483): the wave task (the
 * one whose manager is $6D91, sim/wave.c) between waves.
 *   - $6BC3, a game's first: wave 0, the first wave kind's record
 *     ($702F), 2 easy waves (+$1C), 32 frames.
 *   - $6BDC, a wave's end (no enemies left): <$3C down; the settings at
 *     $B1AD (3 bytes each: a word and a countdown) stepped by $DE11's
 *     records (14 bytes: countdowns by the CMOS difficulty, $CC14, a
 *     nibble each; +6 the limit, +$D the step); <$9B down by 5 to $E0.
 *   - $6C38: <$9F counted down to the lava bridges burning ($6E01: two
 *     $8638 tasks), or <$A0 to the lava's flames (two $86D0 tasks after
 *     the list's first); the egg counts, <$8F, <$A2, <$B0, the message
 *     slots cleared; the wave number (<$9E, BCD) up; the last wave's
 *     ending (+$13: bonuses, their messages), the messages erased.
 *   - $6CD8: "WAVE n"; the next record (+7, 4 bytes from $7173 to $719B,
 *     round: [bounders, hunters][shadow lords, <$8F][pterodactyls]
 *     [platforms, kind]); the platforms that change ($6EF0 with $6FCB,
 *     $6FD2, $6FD9, $6FE0: one burns away, $81A0, its map bits cleared;
 *     one comes back, its bits from the ROM's templates, a frame's wait);
 *     the kind's routine ($71BC: none, the first wave's, team, gladiator,
 *     egg, pterodactyl waves: messages, an ending in +$13, the eggs of the
 *     egg wave, $7417, or the pterodactyl task $7200); the messages for 30
 *     frames and until the tasks after this one are gone ($6E53), then
 *     erased; the enemies ($6E17: a task $8B0E every $3D frames); the
 *     pterodactyl's wait (+$1B by +$1C from $6DFE, <$AD from $6DE2); the
 *     manager every 8 frames.
 * The messages ($6E73) are shown again one slot a frame (+$1E from $23 to
 * $33: [message][colour][screen address], the small font). The ROM keeps
 * its return addresses across sleeps in +$F and +$11 (and A, B in +$15);
 * the C keeps the same fields and a stack within the step.
 */
#ifndef JOUST_WAVES_H
#define JOUST_WAVES_H

#include "sim/game.h"

#define WAVES_START 0x6BC3
#define WAVES_END 0x6BDC

int waves_ported(uint16_t at);
/* the wave task from `at` (a resume address, or WAVES_END from the manager) */
StepResult waves_step(Game *g, uint16_t task, uint16_t at);

#endif
