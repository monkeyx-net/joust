#include "sim/hiscores.h"

#include "sim/tasks.h"
#include "sim/text.h"

static uint8_t peek(const Game *g, uint16_t a) { return game_peek(g, a); }
static void poke(Game *g, uint16_t a, uint8_t v) { game_poke(g, a, v); }
static uint16_t word(const Game *g, uint16_t a) { return game_word(g, a); }

/* $FFC4 / $FFBF: a solid block at x in the next colour */
static void border_block(Game *g, uint16_t x, uint8_t w_reg, uint8_t h_reg)
{
    uint8_t a = (uint8_t)(peek(g, 0xA0D5) + 0x11);
    if (a < 0x11) a = 0x88; /* the add's carry */
    poke(g, 0xA0D5, a);
    game_blit(g, 0x12, a, 0, x, w_reg, h_reg);
}

void hiscores_border(Game *g)
{
    poke(g, 0xA0D5, 0x77);
    uint16_t x = 0x0912;
    do {
        border_block(g, x, 0x02, 0x0C);
        x = (uint16_t)(x + 0x600);
    } while (x < 0x8D00);
    do {
        border_block(g, x, 0x00, 0x08);
        x = (uint16_t)(x + 0x0C);
    } while (x < 0x8DE8);
    x = (uint16_t)(x - 0x608);
    for (;;) {
        border_block(g, x, 0x02, 0x0C);
        int borrow = x < 0x09E3;
        x = (uint16_t)(x - 0x600);
        if (borrow || !x) break;
    }
    x = (uint16_t)(x + 0x1FC);
    do {
        border_block(g, x, 0x00, 0x08);
        x = (uint16_t)(x - 0x0C);
    } while (x > 0x050E);
}

/* the colour for the entry at y: the newest ones' own */
static uint8_t colour_of(const Game *g, uint16_t y, uint8_t b)
{
    if (y == word(g, 0xA0EA)) return 0x55;
    if (y == word(g, 0xA0EE)) return 0x77;
    return b;
}

typedef uint16_t (*Printer)(Game *, uint8_t, uint8_t, uint16_t);

/* $4246: <$DC rows from x: the rank (<$E2), initials, the score (leading
   zeros blank), <$E7 columns and <$E1 lines apart; y moves on */
static uint16_t rows(Game *g, uint16_t x, uint16_t y, uint8_t b, Printer number, Printer chr)
{
    game_set_word(g, 0xA0D9, x);
    do {
        poke(g, 0xA0E9, 3);
        poke(g, 0xA0E6, 4);
        uint8_t c = b;
        if (y == word(g, 0xA0EA)) c = 0x55;
        else if (y == word(g, 0xA0EC)) c = 0x55;
        if (y == word(g, 0xA0EE) || y == word(g, 0xA0F0)) c = 0x77;
        uint8_t a = peek(g, 0xA0E2);
        if (!(a & 0xF0)) a |= 0xF0;
        x = number(g, a, c, x);
        x = chr(g, 0x2B, c, x);
        x = chr(g, 0x0A, c, x);
        do { /* the initials */
            x = chr(g, game_cmos_byte(g, y), c, x);
            y = (uint16_t)(y + 2);
            poke(g, 0xA0E9, (uint8_t)(peek(g, 0xA0E9) - 1));
        } while (peek(g, 0xA0E9));
        game_set_word(g, 0xA0D5, x);
        x = (uint16_t)((uint8_t)((word(g, 0xA0D9) >> 8) + peek(g, 0xA0E7)) << 8 | peek(g, 0xA0D6));
        poke(g, 0xA0DD, 0);
        do { /* the score */
            a = game_cmos_byte(g, y);
            y = (uint16_t)(y + 2);
            if (!peek(g, 0xA0DD) && (peek(g, 0xA0E6) == 4 || !(a & 0xF0))) { /* leading zeros blank */
                a |= 0xF0;
                if (!(a & 0x0F)) a |= 0x0F;
            }
            poke(g, 0xA0DD, a);
            poke(g, 0xA0DD, (uint8_t)~peek(g, 0xA0DD));
            x = number(g, a, c, x);
            poke(g, 0xA0E6, (uint8_t)(peek(g, 0xA0E6) - 1));
        } while (peek(g, 0xA0E6));
        game_set_word(g, 0xA0D5, x);
        x = (uint16_t)((word(g, 0xA0D9) & 0xFF00) | (uint8_t)(peek(g, 0xA0D6) + peek(g, 0xA0E1)));
        {
            int cy, h;
            uint8_t r = game_add(peek(g, 0xA0E2), 1, 0, &cy, &h);
            poke(g, 0xA0E2, game_daa(r, &cy, h));
        }
        poke(g, 0xA0DC, (uint8_t)(peek(g, 0xA0DC) - 1));
    } while (peek(g, 0xA0DC));
    return y;
}

static void column(Game *g, uint16_t x, uint8_t n, uint8_t rank)
{
    poke(g, 0xA0DC, n);
    poke(g, 0xA0E2, rank);
    (void)x;
}

void hiscores_table(Game *g)
{
    hiscores_border(g);
    text_block(g, 0x0F, 0, 0);
    poke(g, 0xA0D5, 0);
    uint16_t x = 0x1170, y = 0xCD3E;
    uint8_t b = colour_of(g, y, 0x22);
    x = text_number_small(g, 0xF1, b, x);
    x = text_char_small(g, 0x2B, b, x);
    x = (uint16_t)(x + 0x300);
    poke(g, 0xA0DC, 0x15);
    for (;;) { /* the best one's name */
        poke(g, 0xA0DC, (uint8_t)(peek(g, 0xA0DC) - 1));
        if (!peek(g, 0xA0DC)) break;
        uint8_t a = game_cmos_byte(g, y);
        y = (uint16_t)(y + 2);
        if ((int8_t)a > 0x0A) poke(g, 0xA0D5, 0);
        else if (!peek(g, 0xA0D5)) game_set_word(g, 0xA0D5, x);
        x = text_char_small(g, a, b, x);
    }
    if (peek(g, 0xA0D5)) x = word(g, 0xA0D5);
    poke(g, 0xA0DC, 4);
    y = 0xCD6C;
    uint8_t a = game_cmos_byte(g, y);
    y = (uint16_t)(y + 2);
    a |= 0xF0;
    if (!(a & 0x0F)) a |= 0x0F;
    x = text_number_small(g, a, b, x);
    poke(g, 0xA0DC, (uint8_t)(peek(g, 0xA0DC) - 1));
    do {
        x = text_number_small(g, game_cmos_byte(g, y), b, x);
        y = (uint16_t)(y + 2);
        poke(g, 0xA0DC, (uint8_t)(peek(g, 0xA0DC) - 1));
    } while (peek(g, 0xA0DC));
    /* the table: three columns of 13 in the large font */
    game_set_word(g, 0xA0DF, 0x4A5F);
    poke(g, 0xA0DC, 0x0D);
    poke(g, 0xA0E2, 0x02);
    poke(g, 0xA0E1, 0x07);
    poke(g, 0xA0E7, 0x0E);
    y = rows(g, 0x1380, y, 0x33, text_number_large, text_char_large);
    column(g, 0, 0x0D, 0x15);
    y = rows(g, 0x3D80, y, 0x33, text_number_large, text_char_large);
    column(g, 0, 0x0D, 0x28);
    rows(g, 0x6780, y, 0x33, text_number_large, text_char_large);
    /* today's: two columns of 3 in the small */
    game_set_word(g, 0xA0DF, 0x4A56);
    poke(g, 0xA0DC, 0x03);
    poke(g, 0xA0E2, 0x01);
    poke(g, 0xA0E1, 0x0A);
    poke(g, 0xA0E7, 0x15);
    y = rows(g, 0x1336, 0xCFA4, 0x11, text_number_small, text_char_small);
    column(g, 0, 0x03, 0x04);
    rows(g, 0x5336, y, 0x11, text_number_small, text_char_small);
    task_create(g, game_word(g, TASK_CURRENT), 0xF012, 0x40, 0x00);
}
