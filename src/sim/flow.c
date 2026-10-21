#include "sim/flow.h"

#include "sim/tasks.h"

#define STARTS_BEGIN 0x6359
#define STARTS 0x6364
#define COUNTDOWN 0x62AD
#define COIN_SHOWN 0xA0B3 /* <$B3 */
#define FRAMES 0xA011     /* <$11 */

#define MET 0xA04B
#define PLATFORMS_MET 0x65AE
#define COMBO_START 0x62DA
#define COMBO 0x62DF
#define TURN_TAKE 0x8B0E
#define TURN_WAIT 0x8B1A

int flow_ported(uint16_t at)
{
    switch (at) {
    case STARTS_BEGIN:
    case STARTS:
    case COUNTDOWN:
    case PLATFORMS_MET:
    case COMBO_START:
    case COMBO:
    case TURN_TAKE:
    case TURN_WAIT: return 1;
    }
    return 0;
}

/* $6364 */
static StepResult starts(Game *g)
{
    if ((int8_t)game_peek(g, COIN_SHOWN) > 0) return STEP_UNPORTED;
    if (game_read(g, 0xC804) & 0x30) return STEP_UNPORTED;
    return task_sleep(g, 1, STARTS);
}

/* $62AD */
static StepResult countdown(Game *g, uint16_t u)
{
    game_poke(g, FRAMES, (uint8_t)(game_peek(g, FRAMES) + 1));
    uint16_t d = game_word(g, (uint16_t)(u + 0xD));
    if (d) {
        game_set_word(g, (uint16_t)(u + 0xD), (uint16_t)(d - 1));
        return task_sleep(g, 1, COUNTDOWN);
    }
    if (game_controls(g) == 0x0201) { /* player 1 right, player 2 left */
        game_poke(g, (uint16_t)(u + 0x10), 0x3C);
        return task_sleep(g, 1, COUNTDOWN);
    }
    uint8_t left = (uint8_t)(game_peek(g, (uint16_t)(u + 0x10)) - 1);
    game_poke(g, (uint16_t)(u + 0x10), left);
    if ((int8_t)left > 0) return task_sleep(g, 1, COUNTDOWN);
    return STEP_UNPORTED; /* $62CE */
}

StepResult flow_step(Game *g, uint16_t u, uint16_t at)
{
    switch (at) {
    case STARTS_BEGIN:
        task_kill_type(g, 0x48, 0xFF);
        return task_sleep(g, 1, STARTS);
    case STARTS: return starts(g);
    case COUNTDOWN: return countdown(g, u);
    case PLATFORMS_MET:
        if (game_peek(g, MET)) return STEP_UNPORTED;
        return task_sleep(g, 2, PLATFORMS_MET);
    case COMBO_START: return task_sleep(g, 5, COMBO);
    case COMBO:
        if (game_controls(g) == 0x0201) return STEP_UNPORTED; /* $626B */
        return task_sleep(g, 5, COMBO);
    case TURN_TAKE:
        game_poke(g, (uint16_t)(u + 0x16), game_peek(g, 0xA03A));
        game_poke(g, 0xA03A, (uint8_t)(game_peek(g, 0xA03A) + 1));
        return task_sleep(g, 1, TURN_WAIT);
    case TURN_WAIT: {
        uint8_t b = game_peek(g, (uint16_t)(u + 0x16));
        if (game_peek(g, (uint16_t)(u + 2)) == 0x09) return STEP_UNPORTED; /* $8ABA */
        if (game_peek(g, 0xA038) != game_peek(g, 0xA039) || b != game_peek(g, 0xA03B)) return task_sleep(g, 1, TURN_WAIT);
        return STEP_UNPORTED; /* $8B2D */
    }
    }
    return STEP_UNPORTED;
}
