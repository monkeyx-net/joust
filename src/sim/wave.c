#include "sim/wave.h"

#include "sim/tasks.h"
#include "sim/waves.h"

#define MANAGER 0x6D91

int wave_ported(uint16_t at) { return at == MANAGER; }

static uint8_t dec(Game *g, uint16_t a)
{
    uint8_t v = (uint8_t)(game_peek(g, a) - 1);
    game_poke(g, a, v);
    return v;
}

StepResult wave_step(Game *g, uint16_t u, uint16_t at)
{
    (void)at;
    if (!game_peek(g, 0xA0AA)) return waves_step(g, u, WAVES_END);
    if (!game_peek(g, 0xA0AC) || !dec(g, 0xA0AC)) {
        uint16_t d = (uint16_t)(game_word(g, 0xA0AD) - 1);
        if ((int16_t)d > 0) {
            game_set_word(g, 0xA0AD, d);
        } else if (game_peek(g, 0xA0AB) <= 2) { /* $6DAE: a pterodactyl */
            uint16_t y = task_create(g, u, 0x68A8, 0x17, 0xFF);
            game_poke(g, 0xA0AB, (uint8_t)(game_peek(g, 0xA0AB) + 1));
            game_poke(g, (uint16_t)(y + 0x34), 0xFF);
            uint8_t a = game_peek(g, (uint16_t)(u + 0x1B));
            if (a) game_poke(g, (uint16_t)(u + 0x1B), (uint8_t)(a - 1));
            game_set_word(g, 0xA0AD, game_read_word(g, (uint16_t)(0x6DE2 + (int8_t)(uint8_t)(a << 1))));
        }
    }
    if (game_peek(g, 0xA0A1) && !dec(g, (uint16_t)(u + 0xB))) { /* $6DD6: one more brain at once */
        if (game_peek(g, 0xA090)) {
            game_poke(g, 0xA08F, (uint8_t)(game_peek(g, 0xA08F) + 1));
            if (!game_peek(g, 0xA08F)) game_poke(g, 0xA08F, 0xFF);
        }
        game_poke(g, (uint16_t)(u + 0xB), 0x70);
    }
    return task_sleep(g, 8, MANAGER);
}
