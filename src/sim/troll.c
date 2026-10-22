#include "sim/troll.h"

#include "sim/display.h"
#include "sim/eggs.h"
#include "sim/rider.h"
#include "sim/score.h"
#include "sim/tasks.h"

#define HAND 0x69DD
#define HAND_ON 0x69DA
#define HAND_CLIMB 0x6A1F
#define HAND_HOLD 0x6A5E
#define GRIP 0x6A66
#define GRIP_ON 0x6A63
#define GRIP_GONE 0x6A8D
#define HAND_DOWN 0x6AE3
#define HAND_GONE 0x6AEE

int troll_ported(uint16_t at)
{
    return at == HAND || at == HAND_ON || at == HAND_CLIMB || at == HAND_HOLD || at == GRIP || at == GRIP_ON ||
           at == GRIP_GONE || at == HAND_DOWN || at == HAND_GONE;
}

static uint16_t F(uint16_t u, int off) { return (uint16_t)(u + off); }
static uint8_t peek(const Game *g, uint16_t a) { return game_peek(g, a); }
static void poke(Game *g, uint16_t a, uint8_t v) { game_poke(g, a, v); }
static uint16_t word(const Game *g, uint16_t a) { return game_word(g, a); }
static void set_word(Game *g, uint16_t a, uint16_t v) { game_set_word(g, a, v); }

void troll_hand(Game *g, uint16_t u)
{
    poke(g, 0xA096, (uint8_t)(peek(g, 0xA096) + 1));
    uint16_t y = task_create(g, word(g, TASK_PREVIOUS), HAND, 0x15, peek(g, F(u, 3)));
    set_word(g, F(y, 0x24), u);
    poke(g, F(y, 0x10), 0);
    set_word(g, F(y, 0xA), 0x00D6);
    set_word(g, F(y, 7), (uint16_t)(word(g, F(u, 7)) - 2));
    set_word(g, F(y, 0x1C), 0);
    poke(g, F(y, 0x2D), 1);
}

/* $6B06: the hand's frame (+$10 - 6 into [$002A]'s records), if any */
static void draw(Game *g, uint16_t u)
{
    uint8_t a = (uint8_t)(peek(g, F(u, 0x10)) - 6);
    if (a & 0x80) return;
    uint16_t rec = (uint16_t)(game_read_word(g, 0x002A) + (int8_t)a);
    set_word(g, F(u, 0x1C), rec);
    uint16_t node = display_object(g, u, 1, 1);
    display_fill(g, node, rec);
    display_clip(g, node);
}

/* $6AA1: is the rider at y in the air over a pit? *d its x - 2 */
static int pit(const Game *g, uint16_t y, uint16_t *d)
{
    if (!(game_read(g, F(y, 2)) & 0x80)) return 0;
    if (game_read(g, F(y, 0x20))) return 0;
    if (game_read(g, F(y, 0xB)) < 0xC6) return 0;
    int16_t x = (int16_t)(game_read_word(g, F(y, 7)) - 2);
    *d = (uint16_t)x;
    return x <= 0x28 || x >= 0xF0;
}

/* $6A9B: the same, and it is the rider at +$24 */
static int over_pit(const Game *g, uint16_t u, uint16_t y, uint16_t *d)
{
    return y == word(g, F(u, 0x24)) && pit(g, y, d);
}

/* $6AD9: the hand sinks back */
static StepResult down(Game *g, uint16_t u)
{
    uint8_t a = peek(g, F(u, 0x10));
    uint8_t n = (uint8_t)(a + 0xFA);
    poke(g, F(u, 0x10), n);
    if ((int8_t)a - 6 <= 0) { /* $6AEE */
        poke(g, 0xA096, (uint8_t)(peek(g, 0xA096) - 1));
        return task_end(g);
    }
    draw(g, u);
    return task_sleep(g, 5, HAND_ON);
}

/* $6A07: up to the rider's line + 3, then the grab */
static StepResult climb(Game *g, uint16_t u, uint16_t y)
{
    uint8_t b = (uint8_t)(game_read(g, F(y, 0xB)) + 3);
    if (b != peek(g, F(u, 0xB))) {
        if (b > peek(g, F(u, 0xB))) poke(g, F(u, 0xB), (uint8_t)(peek(g, F(u, 0xB)) + 1));
        else poke(g, F(u, 0xB), (uint8_t)(peek(g, F(u, 0xB)) - 1));
        draw(g, u);
        return task_sleep(g, 1, HAND_CLIMB);
    }
    game_sound(g, 0xEF83);
    uint16_t v = word(g, u);
    set_word(g, F(v, 0x30), 0xD916);
    uint16_t t = task_create(g, v, GRIP, (uint8_t)(peek(g, F(u, 2)) + 1), peek(g, F(u, 3)));
    set_word(g, F(t, 7), word(g, F(u, 7)));
    set_word(g, F(t, 0xA), word(g, F(u, 0xA)));
    set_word(g, F(t, 0x24), word(g, F(u, 0x24)));
    set_word(g, F(t, 0x2B), u);
    set_word(g, F(t, 0x1C), 0); /* $D7EA */
    set_word(g, 0xB300, 0x0708);
    set_word(g, 0xB302, word(g, 0xB1B0));
    return task_sleep(g, 2, HAND_HOLD);
}

/* $6A66: the grip on the rider just before it */
static StepResult grip(Game *g, uint16_t u)
{
    uint16_t y = word(g, TASK_PREVIOUS), d;
    if (!over_pit(g, u, y, &d)) return task_end(g);
    uint16_t x = word(g, F(u, 0x2B));
    set_word(g, F(u, 7), d);
    set_word(g, F(x, 7), d);
    uint16_t a = word(g, F(y, 0xA));
    a = (uint16_t)((a & 0xFF00) | (uint8_t)(a + 0x0A));
    set_word(g, F(u, 0xA), a);
    set_word(g, F(x, 0xA), a);
    poke(g, F(u, 0x10), 0x24);
    draw(g, u);
    int16_t n = (int16_t)(word(g, 0xB300) - 1); /* $D7C8: the pull grows */
    if (n <= 0) {
        if (word(g, 0xB302) <= 0x0500) set_word(g, 0xB302, (uint16_t)(word(g, 0xB302) + 1));
        n = 1;
    }
    set_word(g, 0xB300, (uint16_t)n);
    return task_sleep(g, 1, GRIP_ON);
}

StepResult troll_step(Game *g, uint16_t u, uint16_t at)
{
    uint16_t d;
    switch (at) {
    case HAND_ON:
        eggs_erase(g, u);
        /* fall through */
    case HAND: {
        if (!over_pit(g, u, word(g, u), &d)) return down(g, u);
        set_word(g, F(u, 7), d);
        uint8_t n = peek(g, F(u, 0x2D));
        poke(g, F(u, 0x2D), (uint8_t)(n - 1));
        if ((int8_t)n <= 1) {
            poke(g, F(u, 0x2D), game_read(g, 0xB1AE));
            uint8_t a = peek(g, F(u, 0x10));
            if (a == 0x1E) return climb(g, u, word(g, u));
            poke(g, F(u, 0x10), (uint8_t)(a + 6));
        }
        draw(g, u);
        return task_sleep(g, 1, HAND_ON);
    }
    case HAND_CLIMB:
        eggs_erase(g, u);
        if (!over_pit(g, u, word(g, u), &d)) return down(g, u);
        set_word(g, F(u, 7), d);
        return climb(g, u, word(g, u));
    case HAND_HOLD:
        if (!over_pit(g, u, word(g, u), &d)) return down(g, u);
        return task_sleep(g, 2, HAND_HOLD);
    case GRIP_ON:
        eggs_erase(g, u);
        /* fall through */
    case GRIP: return grip(g, u);
    case GRIP_GONE:
        eggs_erase(g, u);
        return task_end(g);
    case HAND_DOWN:
        draw(g, u);
        return task_sleep(g, 5, HAND_ON);
    case HAND_GONE:
        poke(g, 0xA096, (uint8_t)(peek(g, 0xA096) - 1));
        return task_end(g);
    }
    return STEP_UNPORTED;
}

int troll_held(Game *g, uint16_t u, uint8_t b, StepResult *r)
{
    poke(g, F(u, 0x22), 0);
    poke(g, F(u, 0x23), 0);
    poke(g, F(u, 0x14), 0);
    uint16_t v = (uint16_t)((int8_t)b + word(g, 0xB302) + word(g, F(u, 0x11)));
    set_word(g, F(u, 0x11), v);
    if ((int16_t)v >= (int16_t)0xFE80) {
        uint16_t line = (uint16_t)(v + word(g, F(u, 0xB)));
        set_word(g, F(u, 0xB), line);
        if ((line >> 8) >= 0xE6) { /* into the lava */
            uint16_t x = word(g, u);
            if (game_read(g, F(x, 2)) == 0x16) set_word(g, F(x, 5), GRIP_GONE);
            set_word(g, F(word(g, TASK_PREVIOUS), 5), HAND_GONE);
            set_word(g, F(u, 0x30), 0xD858);
            *r = rider_lava(g, u);
            return 1;
        }
        int16_t x = (int16_t)word(g, F(u, 7));
        if (x <= 8) x = 8;
        if (x >= 0x116) x = 0x116;
        set_word(g, F(u, 7), (uint16_t)x);
        uint16_t dd;
        if (pit(g, u, &dd)) return 0;
    }
    /* $D972: free */
    if (game_peek(g, F(u, 2)) & 0x80) score_add(g, word(g, F(u, 0xD)), 0x50, 1);
    uint16_t x = word(g, u);
    if (game_read(g, F(x, 2)) == 0x16) set_word(g, F(x, 5), GRIP_GONE);
    x = word(g, TASK_PREVIOUS);
    set_word(g, F(x, 5), HAND_DOWN);
    poke(g, F(x, 4), 1);
    set_word(g, F(u, 0x30), 0xD858);
    set_word(g, F(u, 0xB), (uint16_t)(word(g, F(u, 0x11)) + word(g, F(u, 0xB))));
    return 0;
}
