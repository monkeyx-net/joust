#include "sim/scores.h"

#include "sim/tasks.h"

#define START 0xE785
#define NEXT 0xE79B
#define WAIT 0xE7A6
#define HIGH_DIGIT 0xE7D4
#define LOW_DIGIT 0xE7D8
#define FONT 0x4A68 /* a pointer to the font's base */
#define TABLE 0xD
#define ENTRY 0xF

int scores_ported(uint16_t at) { return at == START || at == NEXT || at == WAIT; }

/* $E7D4 / $E7D8: a digit at y: its cell cleared, then drawn in `colours` */
static StepResult digit(Game *g, uint16_t routine, uint8_t b, uint16_t y, uint16_t colours)
{
    if (routine == HIGH_DIGIT) b >>= 4; /* RORB x4; ANDB #$0F */
    else if (routine != LOW_DIGIT) return STEP_UNPORTED;
    uint16_t src = (uint16_t)((b & 0x0F) * 0x17 + game_read_word(g, game_read_word(g, FONT)) + 2);
    game_blit(g, 0x12, 0xEE, src, y, 0x07, 0x03);
    game_blit(g, (uint8_t)(colours >> 8), (uint8_t)colours, src, y, 0x07, 0x03);
    return task_sleep(g, 1, NEXT);
}

/* $E78A: draw the entry at x, or wait for the next change after the last */
static StepResult draw(Game *g, uint16_t u, uint16_t x)
{
    game_set_word(g, (uint16_t)(u + ENTRY), x);
    uint16_t colours = game_read_word(g, (uint16_t)(game_word(g, (uint16_t)(u + TABLE)) + 2));
    uint16_t y = game_read_word(g, (uint16_t)(x + 2));
    if (y & 0x8000) return task_sleep(g, 2, WAIT);
    uint8_t b = game_read(g, game_read_word(g, x));
    return digit(g, game_read_word(g, (uint16_t)(x + 4)), b, y, colours);
}

static uint8_t at_entry(const Game *g, uint16_t x) { return game_read(g, game_read_word(g, x)); }

/* $E7A6: the flag; then the first entry worth drawing, skipping leading zeros */
static StepResult wait(Game *g, uint16_t u)
{
    uint16_t x = game_word(g, (uint16_t)(u + TABLE));
    uint16_t flag = game_read_word(g, x);
    if (!game_read(g, flag)) return task_sleep(g, 2, WAIT);
    game_poke(g, flag, 0);
    x = (uint16_t)(x + 4);
    if (at_entry(g, x)) return draw(g, u, x);
    x = (uint16_t)(x + 6);
    uint8_t b = at_entry(g, x);
    if (b) return draw(g, u, b & 0xF0 ? x : (uint16_t)(x + 6));
    x = (uint16_t)(x + 0xC);
    b = at_entry(g, x);
    if (b) return draw(g, u, b & 0xF0 ? x : (uint16_t)(x + 6));
    return draw(g, u, (uint16_t)(x + 0xC));
}

StepResult scores_step(Game *g, uint16_t u, uint16_t at)
{
    switch (at) {
    case START: return draw(g, u, (uint16_t)(game_word(g, (uint16_t)(u + TABLE)) + 0x2B));
    case NEXT: return draw(g, u, (uint16_t)(game_word(g, (uint16_t)(u + ENTRY)) + 6));
    case WAIT: return wait(g, u);
    }
    return STEP_UNPORTED;
}
