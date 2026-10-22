#include "sim/burn.h"

#include "sim/display.h"
#include "sim/spawn.h"
#include "sim/tasks.h"

int burn_ported(uint16_t at)
{
    switch (at) {
    case 0x81A0: case 0x81B0: case 0x81BF: case 0x81CC: case 0x81FB:
    case 0x8638: case 0x863D: case 0x8648: case 0x8688: case 0x869C: case 0x86B2:
    case 0x86D0: case 0x86EC:
        return 1;
    }
    return 0;
}

static uint16_t F(uint16_t u, int off) { return (uint16_t)(u + off); }
static uint8_t peek(const Game *g, uint16_t a) { return game_peek(g, a); }
static void poke(Game *g, uint16_t a, uint8_t v) { game_poke(g, a, v); }
static uint16_t word(const Game *g, uint16_t a) { return game_word(g, a); }

/* $8240: an erase node for the template at +$E, on the line from y's record */
static void erase_node(Game *g, uint16_t u, uint16_t y)
{
    uint8_t line = (uint8_t)((game_read(g, F(y, 7)) ^ 4) + game_read(g, F(y, 5)));
    uint16_t x = display_node(g, 0, line);
    poke(g, x, 0x12);
    poke(g, F(x, 1), 0);
    y = word(g, F(u, 0xE));
    game_set_word(g, F(x, 2), game_read_word(g, F(y, 2)));
    uint16_t d = game_read_word(g, F(y, 4));
    game_set_word(g, F(u, 0x10), d);
    game_set_word(g, F(x, 4), d);
    d = game_read_word(g, F(y, 6));
    game_set_word(g, F(x, 6), (uint16_t)((d & 0xFF00) | (uint8_t)((((uint8_t)d ^ 4) + 0x0B) ^ 4)));
}

/* $81D5: the next of the burn's 5 pixel frames */
static StepResult burn_frame(Game *g, uint16_t u, uint16_t y)
{
    uint16_t x = word(g, F(u, 0xE));
    if (x == game_read_word(g, 0x0000)) x = 0xF145;
    else if (x == game_read_word(g, 0x0004)) x = 0x3151;
    else x = game_read_word(g, F(x, 4));
    game_set_word(g, F(u, 0x10), x);
    game_set_word(g, F(u, 0x13), display_pixels(g, u, y));
    return task_sleep(g, 8, 0x81FB);
}

/* $86EE (chain 1, draw) / $86F5 (chain 0, erase): the flame's frame a & 3 */
static void flame(Game *g, uint16_t u, int chain, uint8_t a)
{
    uint16_t node = display_object(g, u, chain, 1);
    uint16_t rec = (uint16_t)(game_read_word(g, 0x002C) + (a & 3) * 6);
    display_fill(g, node, rec);
    display_clip(g, node);
}

static StepResult next_flame(Game *g, uint16_t u, uint16_t resume)
{
    poke(g, F(u, 0x10), (uint8_t)(peek(g, F(u, 0x10)) + 1));
    flame(g, u, 1, peek(g, F(u, 0x10)));
    return task_sleep(g, 6, resume);
}

/* $8648: the bridge's next column */
static StepResult bridge(Game *g, uint16_t u)
{
    for (;;) {
        uint16_t x = word(g, F(u, 0x2B));
        uint16_t map = (uint16_t)(x - 0x4FFE);
        uint8_t old = game_peek(g, map);
        uint8_t a = (uint8_t)((((old ^ game_read(g, (uint16_t)(x - 0x1448))) & 0x20)) ^ old);
        poke(g, map, a);
        if (a & 0x20) return next_flame(g, u, 0x86B2); /* $86A4: done: the flame sinks */
        x = (uint16_t)(x + (int8_t)peek(g, F(u, 0x2D)));
        game_set_word(g, F(u, 0x2B), x);
        x = (uint16_t)(x + (int8_t)peek(g, F(u, 0x14)));
        if ((int16_t)x < -0x0E || (int16_t)x > 0x128) return task_sleep(g, 1, 0x8648);
        game_set_word(g, F(u, 7), x);
        return next_flame(g, u, 0x8688);
    }
}

StepResult burn_step(Game *g, uint16_t u, uint16_t at)
{
    switch (at) {
    case 0x81A0:
        poke(g, F(u, 0x10), 5);
        /* fall through */
    case 0x81A5:
        spawn_pad_draw(g, word(g, F(u, 0xE)));
        return task_sleep(g, 0x0A, 0x81B0);
    case 0x81B0:
        poke(g, spawn_pad_draw(g, word(g, F(u, 0xE))), 0x2A);
        return task_sleep(g, 0x0A, 0x81BF);
    case 0x81BF: {
        uint8_t n = (uint8_t)(peek(g, F(u, 0x10)) - 1);
        poke(g, F(u, 0x10), n);
        if (n) return burn_step(g, u, 0x81A5);
        erase_node(g, u, g->y);
        return task_sleep(g, 2, 0x81CC);
    }
    case 0x81CC:
        poke(g, F(u, 0x12), 5);
        return burn_frame(g, u, 0xED19);
    case 0x81FB: {
        uint16_t y = word(g, F(u, 0x13));
        uint8_t n = (uint8_t)(peek(g, F(u, 0x12)) - 1);
        poke(g, F(u, 0x12), n);
        if (n) return burn_frame(g, u, y);
        erase_node(g, u, y);
        return task_end(g);
    }
    case 0x8638: return task_sleep(g, 4, 0x863D);
    case 0x863D:
        if (peek(g, 0xA09D) > 0xDF) return task_sleep(g, 4, 0x863D);
        return task_sleep(g, 1, 0x8648);
    case 0x8648: return bridge(g, u);
    case 0x8688:
        flame(g, u, 0, peek(g, F(u, 0x10)));
        return next_flame(g, u, 0x869C);
    case 0x869C:
        flame(g, u, 0, peek(g, F(u, 0x10)));
        return bridge(g, u);
    case 0x86B2:
        flame(g, u, 0, peek(g, F(u, 0x10)));
        if (peek(g, F(u, 0x10)) & 3) return next_flame(g, u, 0x86B2);
        poke(g, F(u, 0xB), (uint8_t)(peek(g, F(u, 0xB)) + 1));
        if (peek(g, F(u, 0xB)) < 0xEF) return next_flame(g, u, 0x86B2);
        poke(g, 0xA04B, peek(g, 0xA04B) | 0x80);
        return task_end(g);
    case 0x86D0:
    case 0x86EC:
        flame(g, u, 0, peek(g, F(u, 0x10)));
        poke(g, F(u, 0x10), (uint8_t)(peek(g, F(u, 0x10)) + 1));
        if (peek(g, F(u, 0xB)) > 0xDF) poke(g, F(u, 0xB), (uint8_t)(peek(g, F(u, 0xB)) - 1));
        flame(g, u, 1, peek(g, F(u, 0x10)));
        return task_sleep(g, 6, 0x86EC);
    }
    return STEP_UNPORTED;
}
