#include "sim/flow.h"

#include "sim/spawn.h"
#include "sim/tasks.h"

#define TURN_TAKE 0x8B0E
#define TURN_WAIT 0x8B1A
#define CODE_RETRY 0x6B20
#define CODE_1 0x6B25
#define CODE_2 0x6B35
#define CODE_3 0x6B45
#define CODE_LOOK 0x6B9B

int flow_ported(uint16_t at)
{
    switch (at) {
    case TURN_TAKE:
    case TURN_WAIT:
    case CODE_RETRY:
    case CODE_1:
    case CODE_2:
    case CODE_3:
    case CODE_LOOK: return 1;
    }
    return 0;
}

/* $6B90: wait for the controls `want`, carrying on at `then` */
static StepResult code_wait(Game *g, uint16_t u, uint16_t want, uint16_t then)
{
    game_set_word(g, (uint16_t)(u + 0xD), want);
    game_set_word(g, (uint16_t)(u + 0xF), then);
    return task_sleep(g, 1, CODE_LOOK);
}

/* $6B9B: the controls against +D, on to the continuation at +F */
static StepResult code_look(Game *g, uint16_t u)
{
    uint16_t want = game_word(g, (uint16_t)(u + 0xD));
    int same = game_controls(g) == want;
    switch (game_word(g, (uint16_t)(u + 0xF))) {
    case 0x6B2A: return same ? code_wait(g, u, want, 0x6B2E) : task_sleep(g, 30, CODE_1);
    case 0x6B2E: return same ? code_wait(g, u, want, 0x6B2E) : task_sleep(g, 15, CODE_2);
    case 0x6B3A: return same ? code_wait(g, u, want, 0x6B3E) : task_sleep(g, 30, CODE_1);
    case 0x6B3E: return same ? code_wait(g, u, want, 0x6B3E) : task_sleep(g, 15, CODE_3);
    case 0x6B4A: return same ? STEP_UNPORTED /* $6B4C: the message */ : code_wait(g, u, want, 0x6B2E);
    }
    return STEP_UNPORTED;
}

/* $8ABA: a player's turn to appear: the pad farthest from the riders' thirds */
static StepResult player_turn(Game *g, uint16_t u, uint8_t b)
{
    if (b != game_peek(g, 0xA039)) return task_sleep(g, 1, TURN_WAIT);
    game_poke(g, 0xA088, 0);
    game_poke(g, 0xA089, 0);
    game_poke(g, 0xA08A, 0);
    for (uint16_t x = game_word(g, game_word(g, 0xA00A)); (x = game_word(g, x)) != 0;) {
        uint8_t a = game_peek(g, (uint16_t)(x + 2));
        if (!(a & 0x80)) continue;
        if (a == 0x82 && !game_word(g, (uint16_t)(x + 0x2B))) continue;
        uint8_t line = game_peek(g, (uint16_t)(x + 0xB)); /* $D800 */
        uint16_t third = line >= 0xA9 ? 0xA08A : line >= 0x51 ? 0xA089 : 0xA088;
        game_poke(g, third, (uint8_t)(game_peek(g, third) + 1));
    }
    game_set_word(g, 0xA06A, game_word(g, 0xA06E));
    game_set_word(g, 0xA06C, game_word(g, 0xA070));
    int first = -1;
    if (!game_peek(g, 0xA08A) && !game_peek(g, 0xA06D)) first = 3;
    else if (!game_peek(g, 0xA089) && !game_peek(g, 0xA06B)) first = 1;
    else if (!game_peek(g, 0xA089) && !game_peek(g, 0xA06C)) first = 2;
    else if (!game_peek(g, 0xA088) && !game_peek(g, 0xA06A)) first = 0;
    if (first >= 0) return spawn_from(g, u, first);
    return spawn_choose(g, u, game_peek(g, 0xA03C) ? 0x885C : 0x883E);
}

StepResult flow_step(Game *g, uint16_t u, uint16_t at)
{
    switch (at) {
    case CODE_RETRY: return task_sleep(g, 30, CODE_1);
    case CODE_1: return code_wait(g, u, 0x2502, 0x6B2A);
    case CODE_2: return code_wait(g, u, 0x1204, 0x6B3A);
    case CODE_3: return code_wait(g, u, 0x0506, 0x6B4A);
    case CODE_LOOK: return code_look(g, u);
    case TURN_TAKE:
        game_poke(g, (uint16_t)(u + 0x16), game_peek(g, 0xA03A));
        game_poke(g, 0xA03A, (uint8_t)(game_peek(g, 0xA03A) + 1));
        return task_sleep(g, 1, TURN_WAIT);
    case TURN_WAIT: {
        uint8_t b = game_peek(g, (uint16_t)(u + 0x16));
        if (game_peek(g, (uint16_t)(u + 2)) == 0x09) return player_turn(g, u, b);
        if (game_peek(g, 0xA038) != game_peek(g, 0xA039) || b != game_peek(g, 0xA03B)) return task_sleep(g, 1, TURN_WAIT);
        return spawn_choose(g, u, 0x883E); /* $8B2D */
    }
    }
    return STEP_UNPORTED;
}
