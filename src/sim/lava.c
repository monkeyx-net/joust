#include "sim/lava.h"

#include "sim/tasks.h"

#define COLUMN 0xA09C   /* <$9C: the surface's column; <$9D its row */
#define ROW 0xA09D
#define TOP 0xA09B      /* <$9B: the row the surface rises to */
#define COLUMNS 0xA0B1  /* <$B1: the bubbles' columns */
#define ROMSEL_COPY 0xBFFF
#define AT 0x14         /* +$14-15: the bubble's screen address */
#define STEP 0xE        /* +E: steps the bubble's column */
#define RANDOM 0xF      /* +F */

#define FILL 0xE4F5
#define BUBBLE 0xE54F
#define RISE_2 0xE579
#define RISE_3 0xE58F
#define DIP_2 0xE5B5
#define DIP_3 0xE5C5
#define DIP_4 0xE5DB

int lava_ported(uint16_t at)
{
    switch (at) {
    case LAVA_START:
    case FILL:
    case BUBBLE:
    case RISE_2:
    case RISE_3:
    case DIP_2:
    case DIP_3:
    case DIP_4: return 1;
    }
    return 0;
}

/* $E544: the surface's next line in 20 frames, or bubbles once it is up */
static StepResult next(Game *g)
{
    if ((int8_t)game_peek(g, ROW) >= (int8_t)game_peek(g, TOP)) return task_sleep(g, 20, FILL);
    return task_sleep(g, 1, BUBBLE);
}

/* $E4F5: a line of the surface, in from the left, then from the right */
static StepResult fill(Game *g)
{
    uint16_t x = game_word(g, COLUMN);
    game_select_rom(g, game_peek(g, ROMSEL_COPY) & 0xFE);
    for (;;) {
        game_poke(g, x, 0x44);
        x = (uint16_t)(x + 0x100);
        if (x > 0x9600) goto done; /* across the screen: no right side */
        uint8_t a = game_read(g, x) & 0xEE;
        if (!a) continue;
        if (!(a & 0xF0)) game_poke(g, x, a | 0x40);
        break;
    }
    x = (uint16_t)(0x9600 | game_peek(g, ROW));
    for (;;) {
        game_poke(g, x, 0x44);
        x = (uint16_t)(x - 0x100);
        uint8_t a = game_read(g, x);
        if (!a) continue;
        if (!(a & 0x0F)) game_poke(g, x, a | 0x04);
        break;
    }
done:
    game_select_rom(g, game_peek(g, ROMSEL_COPY) | 0x01);
    game_poke(g, ROW, (uint8_t)(game_peek(g, ROW) - 1));
    return next(g);
}

/* $E54F: a new bubble or dip */
static StepResult bubble(Game *g, uint16_t u)
{
    int carry;
    uint8_t r = game_random(g, &carry);
    game_poke(g, (uint16_t)(u + RANDOM), r);
    uint8_t col = game_read(g, (uint16_t)(game_word(g, COLUMNS) + (r & 0x0F)));
    if (carry) {
        uint8_t b = (uint8_t)(((game_peek(g, (uint16_t)(u + STEP)) & 0x0F) | 0x02) + game_peek(g, ROW) + 1);
        uint16_t x = (uint16_t)(col << 8 | b);
        game_set_word(g, (uint16_t)(u + AT), x);
        game_poke(g, x, 0x40);
        game_sound(g, 0xE692);
        return task_sleep(g, 8, RISE_2);
    }
    uint16_t x = (uint16_t)(col << 8 | (uint8_t)(game_peek(g, ROW) + 1));
    game_set_word(g, (uint16_t)(u + AT), x);
    game_poke(g, (uint16_t)(x + 1), 0);
    return task_sleep(g, 8, DIP_2);
}

StepResult lava_step(Game *g, uint16_t u, uint16_t at)
{
    uint16_t x = game_word(g, (uint16_t)(u + AT));
    switch (at) {
    case LAVA_START:
        game_set_word(g, COLUMN, 0x00FF);
        game_set_word(g, COLUMNS, 0xE5E9);
        return task_sleep(g, 20, FILL);
    case FILL: return fill(g);
    case BUBBLE: return bubble(g, u);
    case RISE_2:
        game_poke(g, x, 0x04);
        game_poke(g, (uint16_t)(x + 0x100), 0x04);
        game_poke(g, (uint16_t)(x - 1), 0x40);
        game_poke(g, (uint16_t)(x + 1), 0x40);
        return task_sleep(g, 8, RISE_3);
    case RISE_3:
        game_poke(g, (uint16_t)(x - 1), 0x44);
        game_poke(g, x, 0x44);
        game_poke(g, (uint16_t)(x + 1), 0x44);
        game_poke(g, (uint16_t)(x + 0x100), 0x44);
        game_poke(g, (uint16_t)(u + STEP), (uint8_t)(game_peek(g, (uint16_t)(u + STEP)) + 5));
        return next(g);
    case DIP_2:
        game_poke(g, (uint16_t)(x + 1), 0x44);
        game_poke(g, x, 0);
        game_poke(g, (uint16_t)(x - 1), 0x44);
        return task_sleep(g, 8, DIP_3);
    case DIP_3:
        game_poke(g, x, 0x44);
        game_poke(g, (uint16_t)(x - 0x101), 0x44);
        game_poke(g, (uint16_t)(x + 0xFF), 0x44);
        game_poke(g, (uint16_t)(x - 1), 0);
        return task_sleep(g, 8, DIP_4);
    case DIP_4:
        game_poke(g, (uint16_t)(x - 0x101), 0);
        game_poke(g, (uint16_t)(x + 0xFF), 0);
        return next(g);
    }
    return STEP_UNPORTED;
}
