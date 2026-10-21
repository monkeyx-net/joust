#include "sim/wave.h"

#include "sim/tasks.h"

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
    if (!game_peek(g, 0xA0AA)) return STEP_UNPORTED; /* $6BDC: the wave's end */
    if (!game_peek(g, 0xA0AC) || !dec(g, 0xA0AC)) {
        uint16_t d = (uint16_t)(game_word(g, 0xA0AD) - 1);
        if ((int16_t)d <= 0) {
            if (game_peek(g, 0xA0AB) <= 2) return STEP_UNPORTED; /* $6DAE: a pterodactyl */
        } else {
            game_set_word(g, 0xA0AD, d);
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
