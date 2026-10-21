#include "sim/checksum.h"

#include "sim/tasks.h"

#define POINTER 0xD
#define SUM 0xF
#define EXPECTED 0xEEC1

int checksum_ported(uint16_t at) { return at == CHECKSUM_START || at == CHECKSUM_NEXT; }

/* $ECF8: start a pass */
static StepResult start(Game *g, uint16_t u)
{
    game_poke(g, (uint16_t)(u + POINTER), 0);
    game_poke(g, (uint16_t)(u + POINTER + 1), 0);
    game_poke(g, (uint16_t)(u + SUM), 0);
    return task_sleep(g, 1, CHECKSUM_NEXT);
}

StepResult checksum_step(Game *g, uint16_t u, uint16_t at)
{
    if (at == CHECKSUM_START) return start(g, u);
    uint16_t x = game_word(g, (uint16_t)(u + POINTER));
    uint8_t a = (uint8_t)(game_read(g, x) + game_peek(g, (uint16_t)(u + SUM))); /* LDA ,X++; ADDA $F,U */
    x = (uint16_t)(x + 2);
    game_poke(g, (uint16_t)(u + SUM), a);
    game_set_word(g, (uint16_t)(u + POINTER), x);
    if (!(x & 0x8000)) return task_sleep(g, 1, CHECKSUM_NEXT); /* BPL on the pointer's store */
    if (a == game_read(g, EXPECTED)) return start(g, u);
    return STEP_UNPORTED; /* JSR $7A75 */
}
