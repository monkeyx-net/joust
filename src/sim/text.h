/*
 * The ROM's text printer ($4A50-$4B5B, a jump table at $4A50): characters
 * blitted from a font (records [width][height][data] by character, the
 * table at $4BC6 for the large font, $4B5C the small), in a solid colour
 * (control $1A), each one's width in columns to the right of the last.
 * Messages are strings of characters at [$531F + 2 x number], the last
 * with bit 7 set. The printers mask the interrupts while they blit and
 * keep the font's table in <$D1.
 */
#ifndef JOUST_TEXT_H
#define JOUST_TEXT_H

#include "sim/game.h"

/* $4AAD (through $4A5C): message `msg` in colour `colour` at screen address
   x (the column << 8 | the line), large font; returns x after it */
uint16_t text_message(Game *g, uint8_t msg, uint8_t colour, uint16_t x);

/* $4ABF (through $4A53): the same in the small font */
uint16_t text_message_small(Game *g, uint8_t msg, uint8_t colour, uint16_t x);

/* $4AF7 (through $4A56): a BCD byte as two digits in the small font (a
   digit above 9 is character $0A, a blank) */
uint16_t text_number_small(Game *g, uint8_t bcd, uint8_t colour, uint16_t x);

/* $4A7F (through $4A50): character c in the small font */
uint16_t text_char_small(Game *g, uint8_t c, uint8_t colour, uint16_t x);

/* $4A70 (through $4A59) and $4AE5 (through $4A5F): the same in the large font */
uint16_t text_char_large(Game *g, uint8_t c, uint8_t colour, uint16_t x);
uint16_t text_number_large(Game *g, uint8_t bcd, uint8_t colour, uint16_t x);

/* $4B2B-$4B5B (through $4A62, $4A65, $4A6A, $4A6D): the text block a
   ([$5D7B + 2a]: [screen address][colour][message], bit 7 on the last),
   shown or erased (<$F6), in the small or large font */
void text_block(Game *g, uint8_t a, int erase, int large);

#endif
