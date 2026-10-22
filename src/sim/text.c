#include "sim/text.h"

#define FONT_LARGE 0x4BC6
#define FONT_SMALL 0x4B5C
#define MESSAGES 0x531F

/* $4A8E: character c from the font at `font` */
static uint16_t character(Game *g, uint16_t font, uint8_t c, uint8_t colour, uint16_t x)
{
    uint16_t y = game_read_word(g, (uint16_t)(font + (int8_t)(uint8_t)(c << 1)));
    uint8_t w = game_read(g, y), h = game_read(g, (uint16_t)(y + 1));
    game_blit(g, 0x1A, colour, (uint16_t)(y + 2), x, (uint8_t)(w ^ 4), (uint8_t)(h ^ 4));
    return (uint16_t)(x + (w << 8));
}

static uint16_t message(Game *g, uint16_t font, uint8_t msg, uint8_t colour, uint16_t x)
{
    game_set_word(g, 0xA0D1, font);
    uint16_t u = game_read_word(g, (uint16_t)(MESSAGES + 2 * (int8_t)msg));
    uint8_t c;
    do {
        c = game_read(g, u++);
        x = character(g, game_word(g, 0xA0D1), c, colour, x);
    } while (!(c & 0x80));
    return x;
}

uint16_t text_message(Game *g, uint8_t msg, uint8_t colour, uint16_t x) { return message(g, FONT_LARGE, msg, colour, x); }

uint16_t text_message_small(Game *g, uint8_t msg, uint8_t colour, uint16_t x)
{
    return message(g, FONT_SMALL, msg, colour, x);
}

uint16_t text_number_small(Game *g, uint8_t bcd, uint8_t colour, uint16_t x)
{
    game_set_word(g, 0xA0D1, FONT_SMALL);
    game_poke(g, 0xA0D0, bcd); /* $4B09 */
    uint8_t a = (uint8_t)(bcd >> 4);
    if (a > 0x0A) a = 0x0A;
    x = character(g, game_word(g, 0xA0D1), a, colour, x);
    a = game_peek(g, 0xA0D0) & 0x0F;
    if (a > 0x0A) a = 0x0A;
    return character(g, game_word(g, 0xA0D1), a, colour, x);
}

uint16_t text_char_small(Game *g, uint8_t c, uint8_t colour, uint16_t x)
{
    return character(g, FONT_SMALL, c, colour, x);
}

uint16_t text_char_large(Game *g, uint8_t c, uint8_t colour, uint16_t x)
{
    return character(g, FONT_LARGE, c, colour, x);
}

uint16_t text_number_large(Game *g, uint8_t bcd, uint8_t colour, uint16_t x)
{
    game_set_word(g, 0xA0D1, FONT_LARGE);
    game_poke(g, 0xA0D0, bcd);
    uint8_t a = (uint8_t)(bcd >> 4);
    if (a > 0x0A) a = 0x0A;
    x = character(g, game_word(g, 0xA0D1), a, colour, x);
    a = game_peek(g, 0xA0D0) & 0x0F;
    if (a > 0x0A) a = 0x0A;
    return character(g, game_word(g, 0xA0D1), a, colour, x);
}

/* $4B2B-$4B5B: the text block a ([$5D7B + 2a]: [screen address][colour]
   [message], bit 7 on the last), shown or erased (<$F6), in the small or
   large font */
void text_block(Game *g, uint8_t a, int erase, int large)
{
    game_poke(g, 0xA0F6, erase ? (uint8_t)(game_peek(g, 0xA0F6) + 1) : 0);
    uint16_t y = game_read_word(g, (uint16_t)(0x5D7B + 2 * (int8_t)a));
    for (;;) {
        uint16_t x = game_read_word(g, y);
        uint8_t colour = game_read(g, (uint16_t)(y + 2));
        y = (uint16_t)(y + 3);
        if (game_peek(g, 0xA0F6)) colour = 0;
        uint8_t msg = game_read(g, y) & 0x7F;
        if (large) text_message(g, msg, colour, x);
        else text_message_small(g, msg, colour, x);
        if (game_read(g, y++) & 0x80) return;
    }
}
