#include "sim/frame.h"

#include "sim/display.h"
#include "sim/tasks.h"

static uint8_t peek(const Game *g, uint16_t a) { return game_peek(g, a); }
static void poke(Game *g, uint16_t a, uint8_t v) { game_poke(g, a, v); }
static uint16_t word(const Game *g, uint16_t a) { return game_word(g, a); }
static void set_word(Game *g, uint16_t a, uint16_t v) { game_set_word(g, a, v); }

/* $E0F8: the queued sound, when its frames run out */
static void sound(Game *g)
{
    if (!peek(g, 0xA061)) return;
    poke(g, 0xA061, (uint8_t)(peek(g, 0xA061) - 1));
    if (peek(g, 0xA061)) return;
    uint16_t x = word(g, 0xA063);
    if (!x) return;
    uint16_t d = game_read_word(g, x);
    x = (uint16_t)(x + 2);
    if (!(d & 0x8000)) x = 0; /* the script's last */
    set_word(g, 0xA063, x);
    poke(g, 0xA061, (uint8_t)d);
    if (g->on_sound) {
        g->on_sound(g->user, 0xFF);
        g->on_sound(g->user, (uint8_t)(d >> 8) & 0x3F);
    }
}

/* $E119: each band's two chains emptied (heads at +0/+2, tails at +4/+6) */
static void layers(Game *g)
{
    for (uint16_t x = 0xA000; x <= 0xA0C0; x = (uint16_t)(x + 0x40)) {
        set_word(g, (uint16_t)(x + 4), x);
        set_word(g, x, 0);
        set_word(g, (uint16_t)(x + 6), (uint16_t)(x + 2));
        set_word(g, (uint16_t)(x + 2), 0);
    }
}

/* $E197: the chains into the list's band heads, the list ready */
static void chain(Game *g, uint16_t u)
{
    set_word(g, 0xA068, u);
    u = (uint16_t)(u + 1);
    set_word(g, u, word(g, 0xA000));
    set_word(g, word(g, 0xA004), 0);
    if (!word(g, 0xA040)) {
        set_word(g, (uint16_t)(u + 2), word(g, 0xA002));
    } else {
        set_word(g, (uint16_t)(u + 2), word(g, 0xA040));
        set_word(g, word(g, 0xA044), word(g, 0xA002));
    }
    set_word(g, word(g, 0xA006), 0);
    if (!word(g, 0xA080)) {
        set_word(g, (uint16_t)(u + 4), word(g, 0xA042));
    } else {
        set_word(g, (uint16_t)(u + 4), word(g, 0xA080));
        set_word(g, word(g, 0xA084), word(g, 0xA042));
    }
    set_word(g, word(g, 0xA046), 0);
    if (!word(g, 0xA0C0)) {
        if (!word(g, 0xA082)) {
            set_word(g, (uint16_t)(u + 6), word(g, 0xA0C2));
        } else {
            set_word(g, (uint16_t)(u + 6), word(g, 0xA082));
            set_word(g, word(g, 0xA086), word(g, 0xA0C2));
        }
    } else {
        set_word(g, (uint16_t)(u + 6), word(g, 0xA0C0));
        if (!word(g, 0xA082)) {
            set_word(g, word(g, 0xA0C4), word(g, 0xA0C2));
        } else {
            set_word(g, word(g, 0xA0C4), word(g, 0xA082));
            set_word(g, word(g, 0xA086), word(g, 0xA0C2));
        }
    }
    set_word(g, word(g, 0xA0C6), 0);
    poke(g, (uint16_t)(u - 1), (uint8_t)(peek(g, (uint16_t)(u - 1)) + 1));
}

/* $E152: a task's step; 1 if not ported */
static int run(Game *g, const FrameHooks *h, uint16_t prev, uint16_t t, uint16_t *unported)
{
    set_word(g, TASK_PREVIOUS, prev);
    set_word(g, TASK_CURRENT, t);
    if (h && h->interrupts) h->interrupts(h->ctx, g, t);
    if (game_step(g, t) == STEP_UNPORTED) {
        if (unported) *unported = t;
        return 1;
    }
    return 0;
}

/* after a step: where the walk goes on (the task, or the one before it if it ended) */
static uint16_t after(const Game *g, uint16_t prev, uint16_t t)
{
    return word(g, TASK_FREE) == t ? prev : t;
}

FrameResult frame_run(Game *g, const FrameHooks *h, uint16_t *unported)
{
    uint16_t x = (int8_t)peek(g, 0xA027) < 0 ? 0xAC21 : 0xA9C0;
    set_word(g, 0xA028, x);
    poke(g, 0xA027, (uint8_t)~peek(g, 0xA027));
    if (h && h->interrupts) h->interrupts(h->ctx, g, FRAME_START); /* (while it waits for the list to be drawn) */
    set_word(g, DISPLAY_NEXT, (uint16_t)(x + 9));
    sound(g);
    layers(g);
    poke(g, 0xA065, 0);
    /* the first pass */
    uint16_t prev = TASK_HEAD;
    for (;;) {
        set_word(g, TASK_PREVIOUS, prev);
        uint16_t t = word(g, prev);
        if (!t) break;
        if (peek(g, (uint16_t)(t + TASK_PASS))) {
            prev = t;
            continue;
        }
        poke(g, (uint16_t)(t + TASK_SLEEP), (uint8_t)(peek(g, (uint16_t)(t + TASK_SLEEP)) - 1));
        if (peek(g, (uint16_t)(t + TASK_SLEEP))) {
            prev = t;
            continue;
        }
        if (run(g, h, prev, t, unported)) return FRAME_UNPORTED;
        prev = after(g, prev, t);
    }
    /* the second */
    poke(g, 0xA065, (uint8_t)~peek(g, 0xA065));
    prev = word(g, TASK_PASS2);
    if (!prev) prev = TASK_HEAD;
    for (;;) {
        set_word(g, TASK_PREVIOUS, prev);
        if (h && h->pass2_on ? !h->pass2_on(h->ctx, g, prev) : !game_read(g, word(g, 0xA068))) break;
        uint16_t t = word(g, prev);
        if (!t) {
            prev = 0;
            break;
        }
        if (!peek(g, (uint16_t)(t + TASK_PASS))) {
            prev = t;
            continue;
        }
        poke(g, (uint16_t)(t + TASK_SLEEP), (uint8_t)(peek(g, (uint16_t)(t + TASK_SLEEP)) - 1));
        if (peek(g, (uint16_t)(t + TASK_SLEEP))) {
            prev = t;
            continue;
        }
        if (run(g, h, prev, t, unported)) return FRAME_UNPORTED;
        prev = after(g, prev, t);
    }
    set_word(g, TASK_PASS2, prev);
    if (h && h->interrupts) h->interrupts(h->ctx, g, FRAME_END);
    chain(g, word(g, 0xA028));
    return FRAME_DONE;
}
