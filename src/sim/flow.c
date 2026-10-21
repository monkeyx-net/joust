#include "sim/flow.h"

#include "sim/tasks.h"

#define STARTS_BEGIN 0x6359
#define STARTS 0x6364
#define COUNTDOWN 0x62AD
#define COIN_SHOWN 0xA0B3 /* <$B3 */
#define FRAMES 0xA011     /* <$11 */

int flow_ported(uint16_t at) { return at == STARTS_BEGIN || at == STARTS || at == COUNTDOWN; }

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
    }
    return STEP_UNPORTED;
}
