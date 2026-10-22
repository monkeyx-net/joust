#include "sim/death.h"

#include "sim/score.h"
#include "sim/tasks.h"

static uint16_t F(uint16_t t, int off) { return (uint16_t)(t + off); }

/* $85C1: the ROM check */
static void rom_check(Game *g)
{
    uint16_t x = game_read_word(g, 0x8636);
    uint8_t a = 0;
    for (int i = 0; i < 0x22; i++) a = (uint8_t)(a + game_read(g, (uint16_t)(x + i)));
    if (a == 0x32) return;
    uint16_t free = game_word(g, 0xA008);
    game_poke(g, F(free, 1), (uint8_t)(game_peek(g, F(free, 1)) + 1));
}

void death_player(Game *g, uint16_t u, uint16_t winner, int player2)
{
    game_poke(g, player2 ? 0xA037 : 0xA036, 0);
    uint16_t deaths = player2 ? 0xA0A6 : 0xA0A5;
    game_poke(g, deaths, (uint8_t)(game_peek(g, deaths) + 1));
    game_poke(g, 0xA0AC, 0x25);
    if (winner) {
        uint16_t t = game_word(g, F(winner, 0xD)), y = t ? game_read_word(g, F(t, 0x18)) : 0;
        if (y) {
            uint8_t v = (uint8_t)(game_peek(g, y) + 1);
            game_poke(g, y, v);
            if ((int8_t)v < 0) {
                game_poke(g, y, (uint8_t)(v - 1));
            } else if (!v) { /* $82A0: the gladiator bounty */
                game_poke(g, 0xA0A3, 0);
                game_poke(g, 0xA0A4, 0);
                game_poke(g, y, 0x80);
                score_add(g, t, 0x30, 0);
                uint16_t n = task_create(g, u, 0x7861, 0x02, 0xFF);
                game_set_word(g, F(n, 7), (uint16_t)(game_word(g, F(u, 7)) + 6));
                game_set_word(g, F(n, 0xA), game_word(g, F(u, 0xA)));
                game_set_word(g, F(n, 0x2E), (uint16_t)(0x64 << 8 | game_read(g, F(t, 0x2E))));
                game_sound(g, 0xEF80);
            }
        } else if ((game_peek(g, 0xA090) & 0x80) && game_peek(g, 0xA0A0)) {
            game_poke(g, 0xA0B0, 2);
        }
    }
    uint16_t n = task_create(g, u, 0x8139, 0x20, 0x00); /* $82E3 */
    game_set_word(g, F(n, 7), game_word(g, F(u, 7)));
    game_set_word(g, F(n, 0xA), game_word(g, F(u, 0xA)));
    uint16_t t = game_word(g, F(u, 0xD));
    score_add(g, t, 0x50, 1);
    rom_check(g);
    game_sound(g, 0xEFB7);
    if (u == game_word(g, 0xA073)) {
        game_set_word(g, 0xA073, game_word(g, 0xA075));
        game_poke(g, 0xA0A8, game_peek(g, 0xA0A9));
    }
    game_set_word(g, 0xA075, 0);
    uint16_t s = game_read_word(g, F(t, 4));
    uint8_t lives = (uint8_t)(game_peek(g, F(s, 6)) - 1);
    game_poke(g, F(s, 6), lives);
    if (lives || !(game_peek(g, 0xA090) & 0x80)) return;
    if (game_peek(g, 0xA052) | game_peek(g, 0xA05C)) { /* the other player plays on */
        uint16_t m = task_create(g, u, 0x834B, 0x09, 0xFF);
        game_poke(g, F(m, 0x2D), 0x16);
        game_set_word(g, F(m, 0xD), t);
        return;
    }
    task_create(g, game_word(g, TASK_HEAD), 0x61F0, 0x34, 0x00); /* game over */
}
