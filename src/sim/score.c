#include "sim/score.h"

void score_life(Game *g, uint16_t x)
{
    uint16_t s = game_read_word(g, (uint16_t)(x + 4));
    if (!s) return;
    game_poke(g, (uint16_t)(s + 5), (uint8_t)(game_peek(g, (uint16_t)(s + 5)) + 1));
    game_poke(g, (uint16_t)(s + 6), (uint8_t)(game_peek(g, (uint16_t)(s + 6)) + 1));
    if (!game_peek(g, (uint16_t)(s + 6))) {
        game_poke(g, (uint16_t)(s + 5), (uint8_t)(game_peek(g, (uint16_t)(s + 5)) - 1));
        game_poke(g, (uint16_t)(s + 6), (uint8_t)(game_peek(g, (uint16_t)(s + 6)) - 1));
    }
    uint8_t lives = game_peek(g, (uint16_t)(s + 5));
    if (lives > 5) return;
    uint16_t dst = (uint16_t)(((uint8_t)(lives * 3 - 3) << 8) + game_read_word(g, (uint16_t)(x + 6)));
    game_blit(g, 0x0A, 0, game_read_word(g, (uint16_t)(x + 8)), dst, 0x07, 0x03);
}

void score_add(Game *g, uint16_t x, uint8_t a, int nibbles)
{
    uint16_t y = game_read_word(g, (uint16_t)(x + 4));
    if (!y) return;
    int c, h;
    if (nibbles) {
        uint8_t b = a & 0x0F;
        a = game_add(a & 0xF0, game_peek(g, (uint16_t)(y + 3)), 0, &c, &h);
        a = game_daa(a, &c, h);
        game_poke(g, (uint16_t)(y + 3), a);
        a = game_add(b, game_peek(g, (uint16_t)(y + 2)), c, &c, &h);
    } else {
        a = game_add(a, game_peek(g, (uint16_t)(y + 2)), 0, &c, &h);
    }
    a = game_daa(a, &c, h);
    game_poke(g, (uint16_t)(y + 2), a);
    game_poke(g, (uint16_t)(y + 4), (uint8_t)(game_peek(g, (uint16_t)(y + 4)) + 1));
    a = game_add(game_peek(g, (uint16_t)(y + 1)), 0, c, &c, &h);
    a = game_daa(a, &c, h);
    game_poke(g, (uint16_t)(y + 1), a);
    a = game_add(game_peek(g, y), 0, c, &c, &h);
    a = game_daa(a, &c, h);
    if (c) a = (uint8_t)(a + 0x10);
    game_poke(g, y, a);
    if (!game_word(g, 0xA097)) return;
    for (;;) { /* $E6E0: extra lives */
        a = game_add(game_peek(g, (uint16_t)(y + 1)), game_peek(g, (uint16_t)(y + 7)), 0, &c, &h);
        a = game_daa(a, &c, h);
        uint16_t d = (uint16_t)(a << 8 | game_peek(g, (uint16_t)(y + 2)));
        if (d < game_word(g, (uint16_t)(y + 8))) return;
        a = game_add(game_peek(g, (uint16_t)(y + 9)), game_peek(g, 0xA098), 0, &c, &h);
        a = game_daa(a, &c, h);
        game_poke(g, (uint16_t)(y + 9), a);
        a = game_add(game_peek(g, (uint16_t)(y + 8)), game_peek(g, 0xA097), c, &c, &h);
        a = game_daa(a, &c, h);
        game_poke(g, (uint16_t)(y + 8), a);
        if (a >= 0x30) {
            a = game_add(a, 0x80, 0, &c, &h);
            game_poke(g, (uint16_t)(y + 8), game_daa(a, &c, h));
            a = game_add(game_peek(g, (uint16_t)(y + 7)), 0x80, 0, &c, &h);
            game_poke(g, (uint16_t)(y + 7), game_daa(a, &c, h));
        }
        score_life(g, x);
        game_sound(g, 0xEF67);
        game_audit(g, 5);
    }
}

int score_life_used(Game *g, uint16_t x)
{
    uint16_t s = game_read_word(g, (uint16_t)(x + 4));
    if (!s) return 0;
    uint8_t a = game_peek(g, (uint16_t)(s + 5));
    if (!a) return 0;
    if (game_peek(g, 0xA090) & 0x80) game_audit(g, 7);
    a = (uint8_t)(a - 1);
    game_poke(g, (uint16_t)(s + 5), a);
    if (a <= 4) { /* its icon erased */
        uint16_t dst = (uint16_t)(((uint8_t)((uint8_t)(a << 1) + a) << 8) + game_read_word(g, (uint16_t)(x + 6)));
        game_blit(g, 0x1A, 0xEE, game_read_word(g, (uint16_t)(x + 8)), dst, 0x07, 0x03);
    }
    return 1;
}
