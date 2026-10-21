#include "sim/spawn.h"

#include <stddef.h>

#include "sim/display.h"
#include "sim/rider.h"
#include "sim/tasks.h"

#define PADS 0x8A5D
#define TURN_WAIT 0x8B1A

int spawn_ported(uint16_t at) { return at == SPAWN_GROW || at == SPAWN_WAIT; }

static uint16_t F(uint16_t u, int off) { return (uint16_t)(u + off); }
static uint16_t table(const Game *g, uint16_t u, int off) { return (uint16_t)(game_word(g, F(u, 0xD)) + off); }

/* $8824: a pad's zone against x */
static void zone(Game *g, uint16_t y, uint16_t x)
{
    int busy;
    if (!game_read(g, y)) busy = x >= game_read_word(g, (uint16_t)(y + 1)) && x <= game_read_word(g, (uint16_t)(y + 3));
    else busy = x > game_read_word(g, (uint16_t)(y + 1)) || x <= game_read_word(g, (uint16_t)(y + 3));
    if (busy) {
        uint16_t flag = game_read_word(g, (uint16_t)(y + 5));
        game_poke(g, flag, (uint8_t)(game_peek(g, flag) + 1));
    }
}

/* $87B0 */
static void pads_busy(Game *g, uint16_t zones)
{
    game_set_word(g, 0xA06A, game_word(g, 0xA06E));
    game_set_word(g, 0xA06C, game_word(g, 0xA070));
    for (uint16_t u = game_word(g, game_word(g, 0xA00A)); (u = game_word(g, u));) {
        uint8_t type = game_peek(g, F(u, 2));
        if (!(type & 0x80)) continue;
        if (type == 0x82 && !game_word(g, F(u, 0x2B))) continue;
        uint16_t y = zones;
        uint16_t ahead = game_read_word(g, y);
        y = (uint16_t)(y + 2);
        int16_t x = (int16_t)(game_read_word(g, (uint16_t)(ahead + (int8_t)game_peek(g, F(u, 0x14)))) +
                              game_word(g, F(u, 7)));
        if (x < -10) x = (int16_t)(x + 0x12E);
        if (x > 0x0124) x = (int16_t)(x - 0x12E);
        uint8_t line = game_peek(g, F(u, 0xB));
        if (line <= 0x58) {
            if (!game_peek(g, 0xA06A)) zone(g, y, (uint16_t)x);
        } else if (line <= 0x91) {
            y = (uint16_t)(y + 7);
            if (!game_peek(g, 0xA06B)) zone(g, y, (uint16_t)x);
            if (game_peek(g, 0xA06C)) continue;
            zone(g, (uint16_t)(y + 7), (uint16_t)x);
        } else if (line >= 0xAA && !game_peek(g, 0xA06D)) {
            zone(g, (uint16_t)(y + 0x15), (uint16_t)x);
        }
    }
}

/* $65EE: a fixed picture from its node template, on chain +6 */
static uint16_t pad_draw(Game *g, uint16_t tmpl)
{
    uint8_t line = (uint8_t)((game_read(g, (uint16_t)(tmpl + 7)) ^ 4) + game_read(g, (uint16_t)(tmpl + 5)));
    uint16_t node = display_node(g, 1, line);
    game_poke(g, node, 0x0A);
    game_set_word(g, (uint16_t)(node + 4), game_read_word(g, (uint16_t)(tmpl + 4)));
    game_set_word(g, (uint16_t)(node + 2), game_read_word(g, (uint16_t)(tmpl + 2)));
    game_set_word(g, (uint16_t)(node + 6), game_read_word(g, (uint16_t)(tmpl + 6)));
    return node;
}

static uint16_t pad_template(const Game *g, uint16_t u)
{
    return game_read_word(g, game_read_word(g, game_word(g, F(u, 0x20))));
}

/* $8BAF: the pad in the rider's colour, the rider growing in; then $8C17's frame */
static StepResult grow(Game *g, uint16_t u)
{
    uint16_t node = pad_draw(g, pad_template(g, u));
    uint8_t colour = game_read(g, table(g, u, 0x2E));
    game_poke(g, node, game_peek(g, node) | 0x10);
    game_poke(g, (uint16_t)(node + 1), colour);
    if ((int8_t)game_peek(g, F(u, 0x10)) <= 0x14) {
        uint16_t x = rider_pictures(g, u, 1);
        if (!x) return STEP_UNPORTED;
        uint8_t a = (uint8_t)((((int8_t)(uint8_t)~game_peek(g, F(u, 0x10))) >> 1) & 0x0F) ^ 4;
        game_poke(g, (uint16_t)(x + 7), a);
        game_poke(g, (uint16_t)(x - 3), (int8_t)a > 3 ? 3 : a);
        a = (uint8_t)((uint8_t)(-(uint8_t)(a ^ 4) + 2) & 0x0F);
        uint8_t b = a;
        a = (uint8_t)(a + game_peek(g, (uint16_t)(x + 5)));
        b = (uint8_t)(b + game_peek(g, (uint16_t)(x - 5)));
        game_poke(g, (uint16_t)(x + 5), a);
        game_poke(g, (uint16_t)(x - 5), b);
        b = (uint8_t)((game_peek(g, (uint16_t)(x + 7)) ^ 4) + game_peek(g, (uint16_t)(x + 5)));
        if (b > game_peek(g, F(u, 0xB))) {
            b = (uint8_t)((uint8_t)(game_peek(g, F(u, 0xB)) + 1 - game_peek(g, (uint16_t)(x + 5))) ^ 4);
            game_poke(g, (uint16_t)(x + 7), b);
        }
        a = game_peek(g, x) | 0x10;
        game_poke(g, x, a);
        game_poke(g, (uint16_t)(x + 1), colour);
        game_poke(g, (uint16_t)(x - 10), a);
        game_poke(g, (uint16_t)(x - 9), colour);
    }
    return task_sleep(g, 1, SPAWN_GROW);
}

StepResult spawn_choose(Game *g, uint16_t u, uint16_t zones)
{
    pads_busy(g, zones);
    int carry;
    uint8_t r = game_random(g, &carry);
    int first = carry ? (r & 1 ? 3 : 2) : (r & 1 ? 1 : 0);
    uint16_t pad = 0;
    for (int k = 0; k < 8 && !pad; k++) {
        int p = (first + k) & 3;
        if (k >= 4 && p == first) break; /* the second round stops where the first began */
        if (!game_peek(g, (uint16_t)(0xA06A + p))) pad = (uint16_t)(PADS + 8 * p);
    }
    if (!pad) return task_sleep(g, 1, TURN_WAIT);
    uint16_t count = game_read_word(g, (uint16_t)(pad + 6));
    game_poke(g, count, (uint8_t)(game_peek(g, count) + 1));
    game_set_word(g, F(u, 7), game_read_word(g, (uint16_t)(pad + 2)));
    game_set_word(g, F(u, 0xA), game_read_word(g, (uint16_t)(pad + 4)));
    game_set_word(g, F(u, 0x20), pad);
    uint16_t sound = game_read_word(g, table(g, u, 0x2C));
    if (game_peek(g, F(u, 2)) == 0x09) game_poke(g, 0xA039, (uint8_t)(game_peek(g, 0xA039) + 1));
    else game_poke(g, 0xA03B, (uint8_t)(game_peek(g, 0xA03B) + 1));
    game_sound(g, sound);
    game_poke(g, F(u, 0x10), 0x1E);
    game_set_word(g, F(u, 0x1A), game_read_word(g, game_read_word(g, table(g, u, 0x0A))));
    rider_frame(g, u, 0x0C);
    return grow(g, u);
}

/* $8CA5: a node in the shimmer's colour */
static uint8_t shimmer(Game *g, uint16_t u, uint16_t *x, uint16_t colours, uint8_t b)
{
    uint8_t saved = b, i = b & 7;
    if (!i) {
        i = 2;
        game_poke(g, F(u, 0x26), 2);
        saved = 2;
    }
    uint8_t c = game_read(g, (uint16_t)(colours + (int8_t)i));
    if (!(game_peek(g, F(u, 0x16)) && (c == 0x55 || c == 0x77))) {
        game_poke(g, *x, game_peek(g, *x) | 0x10);
        game_poke(g, (uint16_t)(*x + 1), c);
    }
    *x = (uint16_t)(*x + 10);
    return saved;
}

/* $8CD5: off the pad and onto the ground */
static StepResult leave(Game *g, uint16_t u)
{
    rider_pictures(g, u, 0);
    uint16_t pad = game_word(g, F(u, 0x20)), count = game_read_word(g, (uint16_t)(pad + 6));
    game_poke(g, count, (uint8_t)(game_peek(g, count) - 1));
    pad_draw(g, pad_template(g, u));
    uint16_t who = game_read_word(g, table(g, u, 0x14));
    if (who == 0x8267) { /* a player: <$73 or <$75 */
        if (!game_word(g, 0xA073)) {
            game_set_word(g, 0xA073, u);
            game_poke(g, 0xA0A8, game_peek(g, 0xA0A7));
        } else {
            game_set_word(g, 0xA075, u);
            game_poke(g, 0xA0A9, game_peek(g, 0xA0A7));
        }
    } else if (who != 0x8A7D) {
        return STEP_UNPORTED;
    }
    /* $8CF1 */
    game_set_word(g, F(u, 0x1A), game_read_word(g, game_read_word(g, table(g, u, 0x0A))));
    game_set_word(g, F(u, 0x20), 0xDCE6);
    game_set_word(g, F(u, 0x11), 0);
    static const int cleared[] = { 0x14, 0x16, 0x10, 0x13, 0x15, 0x22, 0x23 };
    for (size_t i = 0; i < sizeof cleared / sizeof cleared[0]; i++) game_poke(g, F(u, cleared[i]), 0);
    game_poke(g, F(u, 2), game_peek(g, F(u, 2)) | 0x80);
    game_poke(g, F(u, 0x17), 0x0C);
    rider_stand_box(g, u);
    return rider_ground_on(g, u);
}

/* $8C56 */
static StepResult wait(Game *g, uint16_t u)
{
    StepResult slept = STEP_DONE;
    int r = rider_brain(g, u, &slept);
    if (r == 1) return slept;
    if (r) return STEP_UNPORTED;
    if (game_word(g, 0xA032)) { /* $8CCB: moved */
        uint16_t sound = game_read_word(g, table(g, u, 0x2A));
        if (sound) game_sound(g, sound);
        return leave(g, u);
    }
    uint8_t n = (uint8_t)(game_peek(g, F(u, 0x13)) - 1);
    game_poke(g, F(u, 0x13), n);
    if ((int8_t)n <= 0) {
        game_poke(g, F(u, 0x13), 0x4B);
        uint8_t left = (uint8_t)(game_peek(g, F(u, 0x35)) >> 1);
        game_poke(g, F(u, 0x35), left);
        if (!left) return leave(g, u);
    }
    rider_pictures(g, u, 1);
    uint16_t node = pad_draw(g, pad_template(g, u));
    uint16_t colours = game_read_word(g, table(g, u, 0xC)), x = (uint16_t)(node - 0x14);
    uint8_t b = game_peek(g, F(u, 0x26));
    b = (uint8_t)(shimmer(g, u, &x, colours, b) - 1);
    b = (uint8_t)(shimmer(g, u, &x, colours, b) - 1);
    shimmer(g, u, &x, colours, b);
    uint8_t pace = (uint8_t)(game_peek(g, F(u, 0x15)) - 1);
    game_poke(g, F(u, 0x15), pace);
    if ((int8_t)pace <= 0) {
        game_poke(g, F(u, 0x26), (uint8_t)(game_peek(g, F(u, 0x26)) + 1));
        game_poke(g, F(u, 0x15), game_peek(g, F(u, 0x35)));
    }
    if (!(game_peek(g, F(u, 0x35)) >> 1)) game_poke(g, F(u, 0x16), (uint8_t)(game_peek(g, F(u, 0x16)) + 1));
    return task_sleep(g, 1, SPAWN_WAIT);
}

/* $8C1C: the mount's node a solid erase; the next frame, or on the pad */
static StepResult grown(Game *g, uint16_t u)
{
    uint16_t rec = game_word(g, F(u, 0x1C));
    if (!rec) return STEP_UNPORTED;
    uint16_t node = display_object(g, u, 0, 1);
    display_fill(g, node, rec);
    game_poke(g, node, 0x12);
    game_poke(g, (uint16_t)(node + 4), (uint8_t)(game_word(g, F(u, 7)) >> 1));
    game_poke(g, (uint16_t)(node + 6), 0x0D);
    uint8_t left = (uint8_t)(game_peek(g, F(u, 0x10)) - 1);
    game_poke(g, F(u, 0x10), left);
    if (left) return grow(g, u);
    game_set_word(g, F(u, 0x24), game_read_word(g, game_word(g, F(u, 0xD))));
    game_poke(g, F(u, 0x35), 0x20);
    game_poke(g, F(u, 0x15), 0x10);
    game_poke(g, F(u, 0x26), 2);
    game_poke(g, F(u, 0x13), 0);
    game_poke(g, F(u, 0x16), 0);
    return wait(g, u);
}

StepResult spawn_step(Game *g, uint16_t u, uint16_t at)
{
    if (at == SPAWN_GROW) return grown(g, u);
    if (at == SPAWN_WAIT) return wait(g, u);
    return STEP_UNPORTED;
}
