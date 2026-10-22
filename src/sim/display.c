#include "sim/display.h"

/* `node` linked on the chain at `tail` */
static void link(Game *g, uint16_t tail, uint16_t node)
{
    game_set_word(g, game_word(g, tail), node); /* STD [,X] */
    game_set_word(g, tail, (uint16_t)(node + 8));
}

uint16_t display_node(Game *g, int chain, uint8_t line)
{
    uint16_t tail = (uint16_t)((chain ? 0xA006 : 0xA004) + (line & 0xC0));
    uint16_t node = game_word(g, DISPLAY_NEXT);
    link(g, tail, node);
    game_set_word(g, DISPLAY_NEXT, (uint16_t)(node + 10));
    return node;
}

void display_fill(Game *g, uint16_t node, uint16_t rec)
{
    uint16_t d = game_read_word(g, (uint16_t)(rec + 2));
    uint8_t col = (uint8_t)((d >> 8) + game_peek(g, DISPLAY_COLUMN)), line = (uint8_t)(d + game_peek(g, DISPLAY_LINE));
    game_set_word(g, (uint16_t)(node + 4), (uint16_t)(col << 8 | line));
    uint16_t pic = game_read_word(g, (uint16_t)(rec + 4));
    game_set_word(g, (uint16_t)(node + 6), game_read_word(g, pic));
    game_set_word(g, (uint16_t)(node + 2), (uint16_t)(pic + 2));
}

void display_clip(Game *g, uint16_t node)
{
    uint8_t top = game_peek(g, (uint16_t)(node + 5));
    if ((uint8_t)((game_peek(g, (uint16_t)(node + 7)) ^ 4) + top) < 0xE0) return;
    if (top < 0xE0) {
        game_poke(g, (uint16_t)(node + 7), (uint8_t)((0xE0 - top) ^ 4));
        return;
    }
    game_poke(g, (uint16_t)(node + 4), 0xF0);
    game_set_word(g, (uint16_t)(node + 6), 0x0404);
}

uint16_t display_object(Game *g, uint16_t u, int chain, int nodes)
{
    uint8_t line = game_peek(g, (uint16_t)(u + 0xB));
    game_poke(g, DISPLAY_LINE, line);
    uint16_t tail = (uint16_t)((chain ? 0xA006 : 0xA004) + (line & 0xC0));
    uint16_t node = game_word(g, DISPLAY_NEXT);
    for (int i = 0; i < nodes; i++) link(g, tail, (uint16_t)(node + 10 * i));
    game_set_word(g, DISPLAY_NEXT, (uint16_t)(node + 10 * nodes));
    uint16_t x = game_word(g, (uint16_t)(u + 7));
    game_poke(g, DISPLAY_COLUMN, (uint8_t)(x >> 1));
    uint8_t ctrl = (uint8_t)((chain ? 0x0A : 0x1A) | (x & 1 ? 0x20 : 0));
    for (int i = 0; i < nodes; i++) {
        uint16_t at = (uint16_t)(node + 10 * i);
        game_poke(g, at, ctrl);
        if (!chain) game_poke(g, (uint16_t)(at + 1), 0); /* STD: solid 0 too */
    }
    return node;
}

uint16_t display_pixels(Game *g, uint16_t u, uint16_t y)
{
    uint16_t x = game_word(g, (uint16_t)(u + 0x10));
    for (;;) {
        uint8_t a = game_read(g, y++);
        if (!a) return y;
        uint8_t b = (uint8_t)((a & 0x0F) * 0x11);
        a >>= 4;
        if (!a) { /* the next line, from the first column */
            x = (uint16_t)(game_word(g, (uint16_t)(u + 0x10)) + 1);
            game_set_word(g, (uint16_t)(u + 0x10), x);
            continue;
        }
        for (; a; a--) {
            if (a == 1) b &= 0xF0;
            if (x < GAME_RAM) game_poke(g, x, b);
            x = (uint16_t)(x + 0x100);
            if (x > 0x9800) break;
        }
    }
}
