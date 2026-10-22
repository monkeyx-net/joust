/*
 * The interrupt ($E38D-$E4B1), at the beam's lines 0, 64, 128 and 192
 * (the PIA's VA11 edges): the random generator stirred (<$0D up, <$0E
 * down), S saved at <$1F; at the top of the frame (band 0, $CB00 & $C0
 * zero) the palette's copy <$0F-<$1E to the hardware, a minute of play
 * counted (<$99 from $0E10: audit 6) in a game, the tilt's timer <$8D,
 * the switches at $C80C sampled into <$B4-<$BC and debounced (<$8B), a
 * new press handled: the advance (the operator's test mode, $F342: not
 * ported), the coins ($4309 left, $4303 centre, $42FD right: audits 1-3,
 * units by the CMOS's coin settings, credits <$F2 and the CMOS's copy,
 * the sound $EF1B, the credits shown), the high score reset ($3DBD: held
 * for 24 x 8 delays it restores the best one's entry). Then the display
 * list of the frame before ([<$25]: a ready flag, the four bands'
 * chains) blitted band by band as the beam leaves each, the flag cleared
 * after the last, <$25 to the other list ($A9C0 / $AC21).
 */
#ifndef JOUST_IRQ_H
#define JOUST_IRQ_H

#include "sim/game.h"

/* The interrupt (the beam's line read from $CB00: Game.beam / vcount).
   Returns 1 if the advance switch was pressed (the test mode, not ported). */
int irq_run(Game *g);

#endif
