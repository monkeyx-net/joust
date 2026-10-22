#include "sim/points.h"

#include "sim/eggs.h"
#include "sim/tasks.h"
#include "sim/text.h"

#define COLLECTED 0x77AC
#define PLACED 0x77BE
#define WAITING 0x7826
#define AIR_BONUS 0x785F
#define POINTS 0x7861
#define SHOWING 0x787B
#define COPY 0x78D1
#define COPY_NEXT 0x78D6

int points_ported(uint16_t at)
{
    return at == COLLECTED || at == PLACED || at == WAITING || at == AIR_BONUS || at == POINTS || at == SHOWING ||
           at == COPY || at == COPY_NEXT;
}

static uint16_t F(uint16_t u, int off) { return (uint16_t)(u + off); }
static uint8_t peek(const Game *g, uint16_t a) { return game_peek(g, a); }
static void poke(Game *g, uint16_t a, uint8_t v) { game_poke(g, a, v); }
static uint16_t word(const Game *g, uint16_t a) { return game_word(g, a); }

/* $788D: x made even, the screen address (+$20) kept on the screen, x and the line from it */
static void place(Game *g, uint16_t u)
{
    uint16_t d = word(g, F(u, 7));
    uint8_t b = (uint8_t)(d & 0xFE);
    poke(g, F(u, 8), b);
    uint8_t a = (uint8_t)(((d >> 8) & 1) << 7 | b >> 1);
    b = (uint8_t)(peek(g, F(u, 0xB)) - 5);
    if (b > 0xCE) b = 0xCE;
    if (a > 0x88) a = 0x88;
    game_set_word(g, F(u, 0x20), (uint16_t)(a << 8 | b));
    poke(g, F(u, 0xB), b);
    game_set_word(g, F(u, 7), (uint16_t)(a << 1));
}

/* $78B4: a copy of these points, shown again later ($78D1) */
static void copy(Game *g, uint16_t u)
{
    uint16_t y = task_create(g, u, COPY, 0x02, 0xFF);
    game_set_word(g, F(y, 7), word(g, F(u, 7)));
    game_set_word(g, F(y, 0xA), word(g, F(u, 0xA)));
    game_set_word(g, F(y, 0x2E), word(g, F(u, 0x2E)));
}

/* $7868: shown every 2nd frame for +$2D frames, then erased */
static StepResult show(Game *g, uint16_t u)
{
    uint16_t x = word(g, F(u, 0x20)), d = word(g, F(u, 0x2E));
    uint8_t n = (uint8_t)(peek(g, F(u, 0x2D)) - 1);
    poke(g, F(u, 0x2D), n);
    if (!n) {
        text_message(g, (uint8_t)(d >> 8), 0, x);
        eggs_walls(g, u);
        return task_end(g);
    }
    text_message(g, (uint8_t)(d >> 8), (uint8_t)d, x);
    return task_sleep(g, 2, SHOWING);
}

static StepResult points(Game *g, uint16_t u)
{
    poke(g, F(u, 0x2D), 0x1E);
    place(g, u);
    return show(g, u);
}

/* $16-bit |d| as `COMA; NEGB; SBCA #$FF` */
static uint16_t magnitude(uint16_t d) { return d & 0x8000 ? (uint16_t)-d : d; }

/* $77BE: a frame's wait for each display near it */
static StepResult placed(Game *g, uint16_t u)
{
    place(g, u);
    poke(g, F(u, 0x32), 1);
    for (uint16_t x = word(g, word(g, 0xA00A)); (x = word(g, x)) != 0;) {
        if (peek(g, F(x, 2)) != 3) continue;
        if ((int16_t)magnitude((uint16_t)(word(g, F(u, 0xA)) - word(g, F(x, 0xA)))) >= 6) continue;
        if ((int16_t)magnitude((uint16_t)(word(g, F(u, 7)) - word(g, F(x, 7)))) >= 0x10) continue;
        uint8_t a = peek(g, F(x, 0x2D));
        if (a >= 8) {
            a = 8;
            poke(g, F(x, 0x2D), 8);
            uint16_t y = word(g, x);
            if (game_read(g, F(y, 2)) == 4) poke(g, F(y, 0x2D), 8);
        }
        a = (uint8_t)((uint8_t)(a << 1) + peek(g, F(x, 0x32)));
        if (a < peek(g, F(u, 0x32))) continue;
        poke(g, F(u, 0x32), a);
    }
    poke(g, F(u, 2), 3);
    poke(g, F(u, 0x2D), 0x1E);
    return task_sleep(g, 1, WAITING);
}

/* $7826 */
static StepResult waiting(Game *g, uint16_t u)
{
    uint8_t n = (uint8_t)(peek(g, F(u, 0x32)) - 1);
    poke(g, F(u, 0x32), n);
    if (n) return task_sleep(g, 1, WAITING);
    if (peek(g, F(u, 0x33))) { /* caught in the air: 500 more */
        uint16_t y = task_create(g, u, AIR_BONUS, 0x04, 0xFF);
        game_set_word(g, F(y, 0x2E), 0x0222);
        game_set_word(g, F(y, 7), word(g, F(u, 7)));
        uint16_t d = word(g, F(u, 0xA));
        game_set_word(g, F(y, 0xA), (uint16_t)((d & 0xFF00) | (uint8_t)(d - 6)));
        d = word(g, F(u, 0x20));
        game_set_word(g, F(y, 0x20), (uint16_t)((d & 0xFF00) | (uint8_t)(d - 6)));
        poke(g, F(y, 0x2D), peek(g, F(u, 0x2D)));
        poke(g, F(y, 0x32), 0);
    }
    return task_sleep(g, 2, AIR_BONUS);
}

StepResult points_step(Game *g, uint16_t u, uint16_t at)
{
    switch (at) {
    case COLLECTED:
        eggs_erase(g, u);
        eggs_walls(g, u);
        if ((int8_t)peek(g, 0xA090) > 0) copy(g, u);
        return task_sleep(g, 2, PLACED);
    case PLACED: return placed(g, u);
    case WAITING: return waiting(g, u);
    case AIR_BONUS:
    case SHOWING: return show(g, u);
    case POINTS: return points(g, u);
    case COPY: return task_sleep(g, 0x44, COPY_NEXT);
    case COPY_NEXT: {
        uint8_t n = (uint8_t)(peek(g, F(u, 0x2E)) + 1);
        poke(g, F(u, 0x2E), n);
        if (n > 4) return task_end(g);
        copy(g, u);
        return points(g, u);
    }
    }
    return STEP_UNPORTED;
}
