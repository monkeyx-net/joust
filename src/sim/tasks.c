#include "sim/tasks.h"

StepResult task_sleep(Game *g, uint8_t ticks, uint16_t resume)
{
    uint16_t u = game_word(g, TASK_CURRENT);
    game_set_word(g, (uint16_t)(u + TASK_RESUME), resume); /* STX 5,U */
    game_poke(g, (uint16_t)(u + TASK_SLEEP), ticks);       /* STA 4,U */
    return STEP_DONE;
}

StepResult task_end(Game *g)
{
    uint16_t u = game_word(g, TASK_CURRENT);
    game_set_word(g, game_word(g, TASK_PREVIOUS), game_word(g, u)); /* the one before skips it */
    game_set_word(g, u, game_word(g, TASK_FREE));
    game_set_word(g, TASK_FREE, u);
    return STEP_DONE;
}

uint16_t task_create(Game *g, uint16_t after, uint16_t start, uint8_t type, uint8_t pass)
{
    uint16_t y = game_word(g, TASK_FREE);
    game_set_word(g, TASK_FREE, game_word(g, y));
    game_set_word(g, y, game_word(g, after));
    game_set_word(g, after, y);
    game_poke(g, (uint16_t)(y + TASK_SLEEP), 1);
    game_set_word(g, (uint16_t)(y + TASK_RESUME), start);
    game_poke(g, (uint16_t)(y + TASK_TYPE), type);
    game_poke(g, (uint16_t)(y + TASK_PASS), pass);
    return y;
}

void task_kill_type(Game *g, uint8_t type, uint8_t mask)
{
    uint16_t x = TASK_HEAD, u = game_word(g, TASK_HEAD);
    while (u) {
        if ((game_peek(g, (uint16_t)(u + TASK_TYPE)) & mask) == type && u != game_word(g, TASK_CURRENT)) {
            if (u == game_word(g, TASK_PASS2)) game_set_word(g, TASK_PASS2, x);
            game_set_word(g, x, game_word(g, u));
            game_set_word(g, u, game_word(g, TASK_FREE));
            game_set_word(g, TASK_FREE, u);
            u = x; /* the one before carries on */
        } else if ((game_peek(g, (uint16_t)(u + TASK_TYPE)) & mask) == type) {
            game_set_word(g, TASK_PREVIOUS, x); /* passing the current task */
        }
        x = u;
        u = game_word(g, u);
    }
}
