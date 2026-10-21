/*
 * The wave (so far its manager's frequent path, $6D88-$6DE0): a task
 * looking every 8 frames while enemies are left (<$AA): <$AC frames to
 * hold, then <$AD-AE counted down, and at 0 the pterodactyl's task
 * ($68A8, at most 3 a wave by <$AB; not ported) with the next wait from
 * $6DE2 by +$1B; and while <$A1 is set, every $70 looks (+B) one more
 * brain may run at once (<$8F up, kept from wrapping, when <$90 is set).
 * No enemies left ends the wave ($6BDC: not ported).
 */
#ifndef JOUST_WAVE_H
#define JOUST_WAVE_H

#include "sim/game.h"

int wave_ported(uint16_t at);
StepResult wave_step(Game *g, uint16_t task, uint16_t at);

#endif
