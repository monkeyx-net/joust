/*
 * Points shown where they were won ($77AC-$78E2): a task prints a
 * message ($4AAD: +$2E its number, +$2F the colour) at +$20 (the screen
 * address $788D works out from its x and line, kept on the screen) for
 * +$2D frames, every 2nd frame, then prints it again in colour 0 (an
 * erase), adds the platform bits under it to <$4B ($779B) and ends.
 *   - $7861: points (the pterodactyl's, the gladiator bounty's)
 *   - $77AC: a collected egg's task turns into its points: the egg erased;
 *     in a two-player game (<$90 > 0) a copy at $78D1; after 2 frames it
 *     waits a frame for each points display (type 3) near it (within 6
 *     lines and 16 pixels: twice its +$2D, at most 8, plus its wait),
 *     becomes type 3 for $1E frames, and if caught in the air (+$33) a
 *     second display, "500" ($0222, 6 lines up, type 4)
 *   - $78D1: after $44 frames the next message number up to 4, again
 */
#ifndef JOUST_POINTS_H
#define JOUST_POINTS_H

#include "sim/game.h"

int points_ported(uint16_t at);
StepResult points_step(Game *g, uint16_t task, uint16_t at);

#endif
