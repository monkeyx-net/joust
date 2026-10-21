/*
 * The display list the interrupt blits: 10-byte nodes [control][solid]
 * [source][destination][width][height][next] (the blitter's registers,
 * width and height XOR 4, then the next node), taken one after another
 * from <$2A. The screen is four bands of 64 lines, each with two chains
 * whose tails ($A004 and $A006, plus $40 x the band) point at the last
 * node's next field; a new node is linked there, so the interrupt draws
 * each band's nodes as the beam leaves it. Entries (via the jump table):
 *   $E2A1 ($E01B), $E2B8 ($E01E): a node on chain +4 / +6 for line B
 *   $E2CF ($E021), $E2F9 ($E024): the same for the object at U (+B its
 *     line, +7 its x); <$31 the line, <$30 the column (x / 2); the
 *     control $1A (solid 0) or $0A, | $20 to shift for an odd x
 *   $E322 ($E027), $E358 ($E02A): two such nodes
 * Each returns the (first) node; the stores are in the ROM's order.
 */
#ifndef JOUST_DISPLAY_H
#define JOUST_DISPLAY_H

#include "sim/game.h"

#define DISPLAY_NEXT 0xA02A /* <$2A: the next free node */
#define DISPLAY_COLUMN 0xA030
#define DISPLAY_LINE 0xA031

/* $E2A1 (chain 0, +4) or $E2B8 (chain 1, +6) */
uint16_t display_node(Game *g, int chain, uint8_t line);
/* $E2CF / $E2F9 (nodes 1) and $E322 / $E358 (nodes 2) for the object at u */
uint16_t display_object(Game *g, uint16_t u, int chain, int nodes);
/* $8E70: fill a node from a frame record (+2 dx, dy from <$30-<$31; +4
   the picture: its header's width and height, then its data) */
void display_fill(Game *g, uint16_t node, uint16_t rec);
/* $D8F4: clip a node at line $E0 (the lava): shortened, or emptied */
void display_clip(Game *g, uint16_t node);

#endif
