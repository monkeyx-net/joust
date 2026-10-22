#include "sim/cycle.h"

#include "sim/tasks.h"

#define START 0xF012
#define NEXT 0xFF3F

int cycle_ported(uint16_t at) { return at == START || at == NEXT; }

static uint16_t F(uint16_t u, int off) { return (uint16_t)(u + off); }

/* $FF3A: the palette's copy, then 4 frames */
static StepResult show(Game *g, uint16_t u)
{
    uint16_t y = F(u, 0xD);
    for (uint16_t x = 0xA017; (int16_t)x <= (int16_t)0xA01E; x++) game_poke(g, x, game_peek(g, y++));
    return task_sleep(g, 4, NEXT);
}

StepResult cycle_step(Game *g, uint16_t u, uint16_t at)
{
    if (at == START) { /* $FF1C */
        game_poke(g, F(u, 0x17), game_read(g, 0xFFE0));
        for (uint8_t a = 0x0D; a != 0x15; a++) game_poke(g, F(u, a), game_read(g, (uint16_t)(0xFFE0 + a - 0x0D)));
        game_poke(g, F(u, 0x16), 0);
        game_set_word(g, F(u, 0x2B), F(u, 0x14));
        return task_sleep(g, 4, NEXT);
    }
    if (at != NEXT) return STEP_UNPORTED;
    game_poke(g, F(u, (uint8_t)(0x0D + game_peek(g, F(u, 0x16)))), game_peek(g, F(u, 0x17)));
    uint8_t a = (uint8_t)(game_peek(g, F(u, 0x16)) + 1);
    if (a == 8) a = 0;
    if (a == 4) { /* the eight rotate */
        uint16_t x = F(u, 0xD);
        uint8_t first = game_peek(g, x++);
        do {
            uint8_t b = game_peek(g, x++);
            game_poke(g, (uint16_t)(x - 2), b);
        } while ((int16_t)x <= (int16_t)game_word(g, F(u, 0x2B)));
        game_poke(g, (uint16_t)(x - 1), first);
    }
    game_poke(g, F(u, 0x16), a);
    a = (uint8_t)(a + 0x0D);
    game_poke(g, F(u, 0x17), game_peek(g, F(u, a)));
    game_poke(g, F(u, a), 0xE8);
    return show(g, u);
}
