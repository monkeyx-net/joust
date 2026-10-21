/*
 * The platforms under and around a rider ($D9A3-$DC20). Two maps say
 * where they are: a column's bits at $B002 + x (RAM, set up for the
 * wave's platforms) against a line's bits at $EA97 + line (ROM): a
 * platform's top under the rider; and $AEA2 + x against $E837 + line:
 * one beside or above it.
 *   - on a top ($D9C2) the rider lands: its line (+B) set to the
 *     platform's ($44, $50, $80, $89, $A2, $D2 by the bits); the lava's
 *     line ($D9FC) is where the troll's hand may grab (a task: not
 *     ported)
 *   - beside or above ($DA3E): the bits pick the platform's collision
 *     data ([[$0000]]-[[$000E]]) and where it is; the rider's data
 *     against it ($DBF9: x from the given column, the lists offset by the
 *     lines between the rider's bottom and the given line, then $DC21);
 *     a hit bounces it: the vertical velocity reversed and halved
 *     (+$11), a push sideways (+$22) and the speed index (+$14) turned,
 *     +$23 set to 2 (below) or $FE (above), the sound at $EF8F. <$4B
 *     collects the bits met.
 */
#ifndef JOUST_PLATFORM_H
#define JOUST_PLATFORM_H

#include "sim/game.h"

/* $D9A3: 1 nothing to land on (it flies on, maybe bounced), 0 it lands
   (its line set), -1 a path not ported (the troll's hand) */
int platform_below(Game *g, uint16_t u);
/* $D9C2: a platform's top under the rider (its bits): 0 landed (its line
   set), 1 not (the lava's line), -1 the troll's hand (not ported) */
int platform_class(Game *g, uint16_t u, uint8_t top);
/* $DA3E: beside or above it (its bits): the bounce, if the data meet */
void platform_hit(Game *g, uint16_t u, uint8_t bits);

#endif
