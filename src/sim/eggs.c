#include "sim/eggs.h"

#include "sim/display.h"
#include "sim/platform.h"
#include "sim/rider.h"
#include "sim/score.h"
#include "sim/tasks.h"

#define FLYING 0x7611
#define FALLING 0x7619
#define RESTING 0x76A7
#define GROWING 0x7757  /* hatching: the knight's frames from $7924 */
#define HATCHED 0x7779
#define STANDING 0x7792 /* the knight waiting for its mount */
#define TAKEN 0x7887    /* picked up ($7A2F) */
#define MOUNT 0x8EAF    /* the mount's task, flying in */
#define MOUNT_BRAIN 0x793C

int eggs_ported(uint16_t at)
{
    return at == FLYING || at == FALLING || at == RESTING || at == GROWING || at == HATCHED || at == STANDING ||
           at == TAKEN;
}

static uint16_t F(uint16_t u, int off) { return (uint16_t)(u + off); }

/* $78E3: the picture's draw node */
static void draw(Game *g, uint16_t u)
{
    uint16_t node = display_object(g, u, 1, 1);
    uint16_t y = game_peek(g, F(u, 0x14)) & 0x80 ? 0x791E : 0x7921;
    uint16_t v = game_word(g, F(u, 0x11));
    uint8_t a;
    if (v & 0x8000) a = (int16_t)(uint16_t)(v + 0x80) > 0 ? game_read(g, y) : game_read(g, (uint16_t)(y + 1));
    else a = (int16_t)(uint16_t)(v - 0x80) > 0 ? game_read(g, (uint16_t)(y + 2)) : game_read(g, y);
    uint16_t rec = (uint16_t)(game_read_word(g, 0x0028) + (int8_t)a);
    game_set_word(g, F(u, 0x1C), rec);
    display_fill(g, node, rec);
    display_clip(g, node);
}

void eggs_erase(Game *g, uint16_t u)
{
    uint16_t rec = game_word(g, F(u, 0x1C));
    if (!rec) return;
    game_set_word(g, F(u, 0x1C), 0);
    uint16_t node = display_object(g, u, 0, 1);
    display_fill(g, node, rec);
    display_clip(g, node);
}

void eggs_walls(Game *g, uint16_t u)
{
    uint8_t a = game_read(g, (uint16_t)(game_word(g, F(u, 7)) - 0x515E)) &
                game_read(g, (uint16_t)(game_word(g, F(u, 0xA)) - 0x17BE));
    game_poke(g, 0xA04B, a | game_peek(g, 0xA04B));
}

static void erase(Game *g, uint16_t u) { eggs_erase(g, u); }

/* $75B0 */
static void move(Game *g, uint16_t u)
{
    uint16_t vel = (uint16_t)((int8_t)game_peek(g, 0xA048) + game_word(g, F(u, 0x11)));
    game_set_word(g, F(u, 0x11), vel);
    uint16_t d = (uint16_t)(vel + game_word(g, F(u, 0xB)));
    uint8_t a = (uint8_t)((d >> 8) + game_peek(g, F(u, 0x23)));
    d = (uint16_t)(a << 8 | (d & 0xFF));
    game_poke(g, F(u, 0x23), 0);
    if (a <= 0x20) {
        game_poke(g, F(u, 0x23), 1);
        vel = game_word(g, F(u, 0x11));
        if (vel & 0x8000) game_set_word(g, F(u, 0x11), (uint16_t)-vel);
        d = 0x2000;
        a = 0x20;
    }
    game_set_word(g, F(u, 0xB), d);
    game_poke(g, F(u, 0x27), a);
    game_poke(g, F(u, 0x28), (uint8_t)(a - 6));
    uint16_t speed = game_read_word(g, (uint16_t)(0xDCD5 + (int8_t)game_peek(g, F(u, 0x14))));
    uint16_t frac = (uint16_t)((speed & 0xFF) + game_peek(g, F(u, 0x16)));
    game_poke(g, F(u, 0x16), (uint8_t)frac);
    rider_move_x(g, u, (uint8_t)((speed >> 8) + (frac >> 8)));
    int16_t x = (int16_t)game_word(g, F(u, 7));
    if (x > 0x0120) x = (int16_t)(x - 0x11C);
    if (x < 4) x = (int16_t)(x + 0x11C);
    game_set_word(g, F(u, 7), (uint16_t)x);
    game_set_word(g, F(u, 0x29), (uint16_t)(x + 6));
}

/* the egg's top map: under its three columns, one line down if `rest` */
static uint8_t ground(const Game *g, uint16_t u, int rest)
{
    uint16_t x = game_word(g, F(u, 7)), y = game_word(g, F(u, 0xA));
    return (game_read(g, (uint16_t)(x - 0x4FFE)) | game_read(g, (uint16_t)(x - 0x5000))) &
           game_read(g, (uint16_t)(x - 0x5007)) & game_read(g, (uint16_t)(y - (rest ? 0x1568 : 0x1569))) & 0x7F;
}

/* $769F: at rest: drawn, 12 frames */
static StepResult rest(Game *g, uint16_t u)
{
    draw(g, u);
    return task_sleep(g, 0x0C, RESTING);
}

/* $7619 */
static StepResult fall(Game *g, uint16_t u)
{
    erase(g, u);
    move(g, u);
    eggs_walls(g, u);
    uint8_t top = ground(g, u, 0);
    if (top) { /* $7660: on a platform */
        game_poke(g, F(u, 0x35), top);
        if (platform_class(g, u, top) < 0) return STEP_UNPORTED;
        uint8_t b = game_peek(g, F(u, 0xB));
        game_poke(g, F(u, 0x27), b);
        game_poke(g, F(u, 0x28), (uint8_t)(b - 6));
        uint8_t s = game_peek(g, F(u, 0x14));
        if (s) s = (uint8_t)(s & 0x80 ? s + 2 : s - 2);
        game_poke(g, F(u, 0x14), s);
        uint16_t v = game_word(g, F(u, 0x11));
        if (!(v & 0x8000)) {
            v = (uint16_t)-(int16_t)((int16_t)v >> 2); /* a quarter, back up */
            game_set_word(g, F(u, 0x11), v);
            if ((int16_t)v >= (int16_t)0xFFE0 && !game_peek(g, F(u, 0x14))) {
                game_poke(g, F(u, 0x2D), game_peek(g, 0xB1B7));
                return rest(g, u);
            }
        }
    }
    { /* $763B: beside a platform */
        uint16_t x = game_word(g, F(u, 7)), y = game_word(g, F(u, 0xA));
        uint8_t beside = game_read(g, (uint16_t)(x - 0x515E)) & game_read(g, (uint16_t)(x - 0x5168)) &
                         game_read(g, (uint16_t)(y - 0x17C3)) & game_read(g, (uint16_t)(y - 0x17BC));
        if (beside) platform_hit(g, u, beside);
    }
    if (game_peek(g, F(u, 0xB)) >= 0xE3) { /* into the lava */
        game_poke(g, 0xA0AA, (uint8_t)(game_peek(g, 0xA0AA) - 1));
        return task_end(g);
    }
    draw(g, u);
    return task_sleep(g, game_peek(g, 0xA0B0), FALLING);
}

/* $7757: the knight's next frame ([$0028] + the byte at +$2E's record:
   [frame][height][frames to the next, 0 the last]) */
static StepResult grow(Game *g, uint16_t u)
{
    erase(g, u);
    uint16_t node = display_object(g, u, 1, 1);
    uint16_t x = game_word(g, F(u, 0x2E));
    uint16_t rec = (uint16_t)(game_read_word(g, 0x0028) + (int8_t)game_read(g, x));
    game_set_word(g, F(u, 0x1C), rec);
    display_fill(g, node, rec);
    display_clip(g, node);
    uint8_t a = game_peek(g, F(u, 0xB));
    game_poke(g, F(u, 0x27), a);
    game_poke(g, F(u, 0x28), (uint8_t)(a - game_read(g, (uint16_t)(x + 1))));
    a = game_read(g, (uint16_t)(x + 2));
    if (!a) return task_sleep(g, 7, HATCHED);
    game_set_word(g, F(u, 0x2E), (uint16_t)(x + 3));
    return task_sleep(g, a, GROWING);
}

/* $76D0: hatching: a mount flies in for the knight */
static StepResult hatch(Game *g, uint16_t u)
{
    game_poke(g, 0xA0A1, (uint8_t)(game_peek(g, 0xA0A1) + 1));
    game_sound(g, 0xEF89);
    uint16_t y = task_create(g, u, MOUNT, 0x0F, game_peek(g, F(u, 3)));
    game_set_word(g, F(u, 0x2B), y);
    game_set_word(g, F(y, 0x2B), u);
    game_poke(g, F(y, 0x33), game_peek(g, F(u, 0x33)));
    game_poke(g, F(y, 0x34), 0);
    game_poke(g, F(y, 0xF), 0);
    game_poke(g, F(y, 0x2E), 0);
    game_poke(g, F(y, 0x14), 8);
    game_set_word(g, F(y, 0x11), 0);
    game_set_word(g, F(y, 0x1A), 0);
    game_poke(g, F(y, 0x22), 0);
    game_poke(g, F(y, 0x16), 0);
    game_poke(g, F(y, 0x10), 0);
    game_set_word(g, F(y, 0x18), game_read_word(g, 0x001A));
    game_set_word(g, F(y, 0xD), game_word(g, F(u, 0xD)));
    game_set_word(g, F(y, 0x24), MOUNT_BRAIN);
    uint16_t x = 0xFFF7; /* from the side away from the egg */
    if (game_word(g, F(u, 7)) <= 0x97) {
        game_poke(g, F(y, 0xF), (uint8_t)~game_peek(g, F(y, 0xF)));
        game_poke(g, F(y, 0x14), (uint8_t)-game_peek(g, F(y, 0x14)));
        x = 0x0123;
    }
    game_set_word(g, F(y, 7), x);
    uint8_t a = game_peek(g, F(u, 0xB));
    game_set_word(g, F(y, 0xA), a > 0xBB ? 0xA9 : a > 0x69 ? 0x68 : 0x45);
    game_set_word(g, F(u, 0x2E), 0x7924);
    return grow(g, u);
}

void eggs_collect(Game *g, uint16_t u, uint16_t x)
{
    game_sound(g, 0xEF86);
    uint16_t t = game_word(g, F(u, 0xD));
    game_poke(g, F(x, 0x2F), game_read(g, (uint16_t)(t + 0x2E)));
    uint16_t count = game_read_word(g, (uint16_t)(t + 0x12));
    uint8_t b = game_peek(g, count);
    if (b < 4) b++;
    game_poke(g, count, b);
    game_poke(g, F(x, 0x2E), b);
    uint16_t e = (uint16_t)(0x75A1 + (int8_t)(uint8_t)(b * 3));
    score_add(g, t, game_read(g, e), game_read_word(g, (uint16_t)(e + 1)) == 0xE6B1);
    game_poke(g, F(x, 0x33), 0);
    if (!game_peek(g, F(x, 0x35))) { /* caught in the air */
        game_poke(g, F(x, 0x33), 0xFF);
        score_add(g, t, 0x05, 0);
    }
    game_poke(g, 0xA0AA, (uint8_t)(game_peek(g, 0xA0AA) - 1));
    uint16_t mount = game_word(g, F(x, 0x2B));
    if (mount) {
        game_poke(g, 0xA0A1, (uint8_t)(game_peek(g, 0xA0A1) - 1));
        game_set_word(g, F(mount, 0x24), 0xEEC2);
    }
    game_set_word(g, F(x, 5), 0x77AC);
    game_poke(g, F(x, 4), 4);
    game_poke(g, F(x, 2), game_peek(g, F(x, 2)) & 0x7F);
}

void eggs_lay(Game *g, uint16_t u, uint16_t winner)
{
    game_sound(g, 0xEF97);
    game_poke(g, 0xA08E, (uint8_t)(game_peek(g, 0xA08E) - game_peek(g, F(u, 0x34))));
    game_poke(g, 0xA0A1, (uint8_t)(game_peek(g, 0xA0A1) - 1));
    uint16_t y = task_create(g, u, FLYING, 0x82, game_peek(g, F(u, 3)));
    game_set_word(g, F(y, 0x2B), 0);
    game_set_word(g, F(y, 0x1C), 0);
    game_set_word(g, F(y, 0x27), 0);
    uint16_t a = game_word(g, F(u, 0xA));
    game_set_word(g, F(y, 0xA), (uint16_t)((a & 0xFF00) | (uint8_t)(a - 8)));
    game_set_word(g, F(y, 7), (uint16_t)(game_word(g, F(u, 7)) + 4));
    game_poke(g, F(y, 0x35), 0);
    uint16_t t = game_word(g, F(u, 0xD));
    if (t < 0x89E3) t = (uint16_t)(t + 0x31); /* hatching one rank up */
    game_set_word(g, F(y, 0xD), t);
    game_set_word(g, F(y, 0x11), game_word(g, F(u, 0x11)));
    game_poke(g, F(y, 0x14), game_peek(g, F(u, 0x14)));
    game_poke(g, F(y, 0x22), 0);
    game_poke(g, F(y, 0x23), 0);
    game_poke(g, F(y, 0x16), 0);
    game_poke(g, F(y, 0x10), 0);
    game_poke(g, F(y, 0x33), game_peek(g, F(u, 0x33)));
    uint8_t n = (uint8_t)(game_peek(g, F(y, 0x33)) - 1);
    game_poke(g, F(y, 0x33), n);
    if (!n && winner) eggs_collect(g, winner, y);
}

StepResult eggs_step(Game *g, uint16_t u, uint16_t at)
{
    switch (at) {
    case FLYING:
        draw(g, u);
        return task_sleep(g, game_peek(g, 0xA0B0), FALLING);
    case FALLING: return fall(g, u);
    case RESTING:
        if (!ground(g, u, 1)) {
            draw(g, u);
            return task_sleep(g, game_peek(g, 0xA0B0), FALLING);
        }
        if (game_poke(g, F(u, 0x2D), (uint8_t)(game_peek(g, F(u, 0x2D)) - 1)), game_peek(g, F(u, 0x2D)))
            return rest(g, u);
        game_poke(g, F(u, 0x2D), (uint8_t)(game_peek(g, F(u, 0x2D)) + 1));
        if (game_peek(g, 0xA0A1) >= game_peek(g, 0xA0A2)) return rest(g, u);
        return hatch(g, u);
    case GROWING: return grow(g, u);
    case HATCHED: {
        erase(g, u);
        uint16_t rec = (uint16_t)(game_read_word(g, game_read_word(g, (uint16_t)(game_word(g, F(u, 0xD)) + 0x0A))) + 0x18);
        game_set_word(g, F(u, 0x1C), rec);
        display_fill(g, display_object(g, u, 1, 1), rec); /* $8E89 */
        return task_sleep(g, 8, STANDING);
    }
    case STANDING:
        display_fill(g, display_object(g, u, 1, 1), game_word(g, F(u, 0x1C)));
        return task_sleep(g, 8, STANDING);
    case TAKEN:
        erase(g, u);
        return task_end(g);
    }
    return STEP_UNPORTED;
}
