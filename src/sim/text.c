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
