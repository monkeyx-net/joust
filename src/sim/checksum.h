/*
 * The ROM check ($ECF8-$ED17): a task that adds up every other byte of
 * $0000-$7FFF, one a frame (about 4.5 minutes a pass), the sum in +F and
 * the pointer in +D-E. A pass ending on any sum but the byte at $EEC1
 * calls $7A75: a random byte of $A000-$AFFF one up (a genuine set never
 * takes it).
 */
#ifndef JOUST_CHECKSUM_H
#define JOUST_CHECKSUM_H

#include "sim/game.h"

#define CHECKSUM_START 0xECF8
#define CHECKSUM_NEXT 0xED03

int checksum_ported(uint16_t at);
StepResult checksum_step(Game *g, uint16_t task, uint16_t at);

#endif
