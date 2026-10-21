#include "sim/rider.h"

#include "sim/display.h"
#include "sim/tasks.h"

#define CONTROLS 0xA032 /* <$32: the direction (-1, 0, 1), <$33 the flap */
#define GRAVITY 0xA048  /* <$48 */
#define SPEEDS 0xDCD5   /* x per frame (8.8) by +$14 */
#define FLYING 0xD858
#define P1_BRAIN 0xDDB2
#define P2_BRAIN 0xDDB9

int rider_ported(uint16_t at) { return at == RIDER_FLAPPING || at == RIDER_GLIDING; }

static uint16_t field(uint16_t u, int off) { return (uint16_t)(u + off); }

/* an 8-bit add's N and V, for the signed branches */
static int add_lt(uint8_t a, uint8_t b, uint8_t *res)
{
    uint8_t r = (uint8_t)(a + b);
    int v = ((a ^ r) & (b ^ r) & 0x80) != 0;
    *res = r;
    return ((r & 0x80) != 0) != v;
}

/* $DDB2 / $DDB9: a player's controls, to <$32 */
static void player_controls(Game *g, int player2)
{
    uint8_t crb = game_read(g, 0xC807);
    game_poke(g, 0xC807, player2 ? crb & 0xF7 : crb | 0x08);
    uint8_t a = game_read(g, 0xC804);
    uint8_t flap = (a & 0x04) ? 1 : 0;
    a &= 0x03;
    int carry = a & 1;
    a = (uint8_t)(a >> 1);                 /* ASRA (bit 7 is 0) */
    a = (uint8_t)(a - carry);              /* SBCA #0 */
    game_poke(g, CONTROLS, a);
    game_poke(g, CONTROLS + 1, flap);
}

/* $DDD2 */
void rider_move_x(Game *g, uint16_t u, uint8_t b)
{
    int8_t push = (int8_t)game_peek(g, field(u, 0x22));
    if (push) {
        if (push < 0) {
            if (push >= -3) goto all;
            b = (uint8_t)(b - 3);
            game_poke(g, field(u, 0x22), (uint8_t)(push + 3));
        } else {
            if (push <= 3) goto all;
            b = (uint8_t)(b + 3);
            game_poke(g, field(u, 0x22), (uint8_t)(push - 3));
        }
        goto move;
    all:
        b = (uint8_t)(b + (uint8_t)push);
        game_poke(g, field(u, 0x22), 0);
    }
move:;
    int16_t x = (int16_t)((int8_t)b + (int16_t)game_word(g, field(u, 7)));
    if (x > 0x0124) x = (int16_t)(x - 0x12F);
    if (x < -10) x = (int16_t)(x + 0x12F);
    game_set_word(g, field(u, 7), (uint16_t)x);
}

/* $8E58/$8E70: fill a node from the frame record at y, at <$30-<$31 */
static void fill(Game *g, uint16_t node, uint16_t rec)
{
    uint16_t d = game_read_word(g, (uint16_t)(rec + 2));
    uint8_t col = (uint8_t)((d >> 8) + game_peek(g, DISPLAY_COLUMN)), line = (uint8_t)(d + game_peek(g, DISPLAY_LINE));
    game_set_word(g, (uint16_t)(node + 4), (uint16_t)(col << 8 | line));
    uint16_t pic = game_read_word(g, (uint16_t)(rec + 4));
    game_set_word(g, (uint16_t)(node + 6), game_read_word(g, pic));
    game_set_word(g, (uint16_t)(node + 2), (uint16_t)(pic + 2));
}

/* $8E8E (erase: chain 0) and $8E4D (draw: chain 1): the pictures' nodes */
static void pictures(Game *g, uint16_t u, int chain)
{
    uint16_t rider = game_word(g, field(u, 0x1E)), mount = game_word(g, field(u, 0x1C));
    if (rider) {
        uint16_t node = display_object(g, u, chain, 2);
        fill(g, node, rider);
        fill(g, (uint16_t)(node + 10), mount);
    } else if (mount) {
        fill(g, display_object(g, u, chain, 1), mount);
    }
}

/* $D83D: the brain, the facing, frames since a flap, the erase nodes; 0, or -1 for a brain not ported */
static int think(Game *g, uint16_t u)
{
    uint16_t brain = game_word(g, field(u, 0x24));
    if (brain != P1_BRAIN && brain != P2_BRAIN) return -1;
    player_controls(g, brain == P2_BRAIN);
    uint8_t dir = game_peek(g, CONTROLS);
    if (dir) {
        game_poke(g, field(u, 0xF), 0);
        if (dir & 0x80) game_poke(g, field(u, 0xF), 0xFF);
    }
    uint8_t since = (uint8_t)(game_peek(g, field(u, 0x13)) + 1);
    game_poke(g, field(u, 0x13), since);
    if (!since) game_poke(g, field(u, 0x13), 0xFF);
    pictures(g, u, 0);
    return 0;
}

/* $D813: a flap's lift, and the direction into the speed index */
static void flap(Game *g, uint16_t u)
{
    uint16_t lift = (uint16_t)(((game_peek(g, field(u, 0x13)) * 0x60) >> 8) - 0x60);
    game_set_word(g, field(u, 0x11), (uint16_t)(lift + game_word(g, field(u, 0x11))));
    uint8_t a;
    int lt = add_lt((uint8_t)(game_peek(g, CONTROLS) << 1), game_peek(g, field(u, 0x14)), &a);
    if (lt ? (int8_t)a >= -8 : (int8_t)a <= 8) game_poke(g, field(u, 0x14), a);
}

/* $D858: in the air; 0, or -1 into the lava (not ported) */
static int fly(Game *g, uint16_t u, uint8_t b)
{
    b = (uint8_t)(b + game_peek(g, GRAVITY));
    uint16_t vel = (uint16_t)((int8_t)b + game_word(g, field(u, 0x11)));
    game_set_word(g, field(u, 0x11), vel);
    uint16_t d = (uint16_t)(vel + game_word(g, field(u, 0xB)));
    uint8_t a = (uint8_t)((d >> 8) + game_peek(g, field(u, 0x23)));
    d = (uint16_t)(a << 8 | (d & 0xFF));
    game_poke(g, field(u, 0x23), 0);
    if (a <= 0x20) { /* the ceiling: bounce down */
        game_poke(g, field(u, 0x23), 1);
        vel = game_word(g, field(u, 0x11));
        if (vel & 0x8000) game_set_word(g, field(u, 0x11), (uint16_t)-vel);
        d = 0x2000;
        a = 0x20;
    }
    if (a >= 0xE6) return -1;
    game_set_word(g, field(u, 0xB), d);
    uint16_t speed = game_read_word(g, (uint16_t)(SPEEDS + (int8_t)game_peek(g, field(u, 0x14))));
    uint16_t frac = (uint16_t)((speed & 0xFF) + game_peek(g, field(u, 0x16)));
    game_poke(g, field(u, 0x16), (uint8_t)frac);
    rider_move_x(g, u, (uint8_t)((speed >> 8) + (frac >> 8)));
    return 0;
}

void rider_frame(Game *g, uint16_t u, uint8_t frame)
{
    game_poke(g, field(u, 0x17), frame);
    uint8_t b = (uint8_t)(game_peek(g, field(u, 0xB)) - 6);
    game_poke(g, field(u, 0x27), b);
    game_poke(g, field(u, 0x28), (uint8_t)(b - 0x0D));
    game_set_word(g, field(u, 0x29), (uint16_t)(game_word(g, field(u, 7)) + 0x10));
    game_poke(g, field(u, 0x26), 0);
    b = (uint8_t)((game_peek(g, field(u, 0xF)) & 0x80 ? 6 : 0) + game_peek(g, field(u, 0x17)));
    game_set_word(g, field(u, 0x1C), (uint16_t)(game_word(g, field(u, 0x18)) + b));
    uint16_t x = game_word(g, field(u, 0x1A));
    if (!x) {
        game_set_word(g, field(u, 0x1E), 0);
    } else if (b < 0x0C) {
        game_set_word(g, field(u, 0x1E), (uint16_t)(x + b));
        game_poke(g, field(u, 0x26), 2);
    } else {
        game_set_word(g, field(u, 0x1E), (uint16_t)(x + (uint8_t)(b - game_peek(g, field(u, 0x17)) + 0x0C)));
    }
}

/* $D9A3: anything under the rider? 1 none (keep flying), 0 or -1 a platform (paths not ported) */
static int below(Game *g, uint16_t u)
{
    uint16_t x = game_word(g, field(u, 7)), y = game_word(g, field(u, 0xA));
    if (game_read(g, (uint16_t)(x - 0x4FFE)) & game_read(g, (uint16_t)(y - 0x1569))) return -1; /* $D9C2 */
    if (game_read(g, (uint16_t)(x - 0x515E)) & game_read(g, (uint16_t)(y - 0x17C9))) return -1; /* $DA3E */
    return 1;
}

/* the frame, the platforms, then the pictures and a sleep; or a landing */
static StepResult air_end(Game *g, uint16_t u, uint8_t frame, uint16_t resume)
{
    rider_frame(g, u, frame);
    if (below(g, u) <= 0) return STEP_UNPORTED; /* $8F87 */
    pictures(g, u, 1);
    return task_sleep(g, game_read(g, game_read_word(g, (uint16_t)(game_word(g, field(u, 0xD)) + 0x1A))), resume);
}

/* $8F43: gliding */
static StepResult glide(Game *g, uint16_t u)
{
    if (fly(g, u, 4)) return STEP_UNPORTED;
    uint8_t frame = 0x60;
    if ((int8_t)game_peek(g, field(u, 0x15)) > 0) {
        game_poke(g, field(u, 0x15), (uint8_t)(game_peek(g, field(u, 0x15)) - 1));
        frame = 0x48;
    }
    return air_end(g, u, frame, RIDER_GLIDING);
}

static uint16_t table(const Game *g, uint16_t u, int off)
{
    return game_read_word(g, (uint16_t)(game_word(g, field(u, 0xD)) + off));
}

StepResult rider_step(Game *g, uint16_t u, uint16_t at)
{
    if (game_word(g, field(u, 0x30)) != FLYING) return STEP_UNPORTED;
    if (think(g, u)) return STEP_UNPORTED;
    uint8_t flapping = game_peek(g, CONTROLS + 1);
    if (at == RIDER_FLAPPING) {
        if (!flapping) { /* $8F25: let go */
            game_sound(g, table(g, u, 0x1C));
            game_poke(g, field(u, 0x13), 0);
            return glide(g, u);
        }
        if (fly(g, u, 0)) return STEP_UNPORTED;
        if ((int8_t)game_peek(g, field(u, 0x15)) > 0)
            game_poke(g, field(u, 0x15), (uint8_t)(game_peek(g, field(u, 0x15)) - 1));
        return air_end(g, u, 0x48, RIDER_FLAPPING);
    }
    if (!flapping) return glide(g, u);
    /* $8F63: a new flap */
    flap(g, u);
    if (fly(g, u, 0)) return STEP_UNPORTED;
    game_sound(g, table(g, u, 0x1E));
    game_poke(g, field(u, 0x13), 0);
    uint8_t frames = game_peek(g, field(u, 0x15));
    game_poke(g, field(u, 0x15), 5);
    return air_end(g, u, (int8_t)frames > 0 ? 0x60 : 0x48, RIDER_FLAPPING);
}
