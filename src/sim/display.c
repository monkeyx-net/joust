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
