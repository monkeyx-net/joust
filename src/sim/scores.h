/*
 * The scores ($E785-$E80C): a task per score, +D its table in ROM ($E71D
 * for player 1's): +0 the byte flagging a change, +2 the colours (the
 * second blit's control and solid colour), then from +4 entries of 6
 * bytes: the byte holding two digits (BCD), the screen address, the
 * routine drawing the high ($E7D4) or low digit ($E7D8); a negative
 * address ends them. When the flag is set ($E7A6, every 2 frames) it is
 * cleared and the digits are drawn from the first one that is not a
 * leading zero, one a frame ($E79B resumes); a digit is a solid $EE blit
 * over its cell, then its glyph ([[$4A68]] + 2 + digit x $17, 3 x 7) in
 * the score's colour. +F keeps the entry being drawn. At the task's
 * start ($E785) the digits are drawn from table +$2B.
 */
#ifndef JOUST_SCORES_H
#define JOUST_SCORES_H

#include "sim/game.h"

int scores_ported(uint16_t at);
StepResult scores_step(Game *g, uint16_t task, uint16_t at);

#endif
