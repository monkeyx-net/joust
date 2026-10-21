#include "sim/rider.h"

#include "sim/display.h"
#include "sim/platform.h"
#include "sim/tasks.h"

#define CONTROLS 0xA032 /* <$32: the direction (-1, 0, 1), <$33 the flap */
#define GRAVITY 0xA048  /* <$48 */
#define SPEEDS 0xDCD5   /* x per frame (8.8) by +$14 */
#define FLYING 0xD858
#define P1_BRAIN 0xDDB2
#define P2_BRAIN 0xDDB9
#define AWAY_BRAIN 0xEEC2 /* flies to a line and off the side of the screen */
#define AWAY_FLAPPED 0xEF12
#define AWAY_GONE 0xEF08  /* where the task resumes once it is off the screen */

#define RIDER_STANDING 0x8D4D
#define RUN_SPEEDS 0x8FB5 /* the run animation by the landing speed */
#define SKID 0xDD2C       /* the run animation's skid */

int rider_ported(uint16_t at) { return at == RIDER_FLAPPING || at == RIDER_GLIDING || at == RIDER_STANDING; }

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

static void pictures(Game *g, uint16_t u, int chain);

/* $EEF2: the brain's direction (the facing) and flap to <$32-<$33 */
static void ahead(Game *g, uint16_t u, uint8_t flap)
{
    game_poke(g, CONTROLS, game_peek(g, field(u, 0xF)) & 0x80 ? 0xFF : 0x01);
    game_poke(g, CONTROLS + 1, flap);
}

/* the brain at +$24: 0 its controls in <$32-<$33, 1 it ended the step
   (asleep), -1 a brain not ported */
enum { BRAIN_SET = 0, BRAIN_SLEPT = 1, BRAIN_UNPORTED = -1 };

static int brain(Game *g, uint16_t u, StepResult *slept)
{
    uint16_t b = game_word(g, field(u, 0x24));
    switch (b) {
    case P1_BRAIN:
    case P2_BRAIN:
        player_controls(g, b == P2_BRAIN);
        return BRAIN_SET;
    case AWAY_FLAPPED:
        game_set_word(g, field(u, 0x24), AWAY_BRAIN);
        ahead(g, u, 0);
        return BRAIN_SET;
    case AWAY_BRAIN: {
        int16_t x = (int16_t)game_word(g, field(u, 7));
        if (x < -7 || x > 0x121) { /* $EF00: gone off the side */
            pictures(g, u, 0);
            *slept = task_sleep(g, 0x3C, AWAY_GONE);
            return BRAIN_SLEPT;
        }
        uint8_t line = game_peek(g, field(u, 0xB)), target = 0x45;
        if (line >= 0x65) target = line >= 0xA1 ? 0xD0 : 0x81;
        uint8_t flap = 0;
        if (((uint8_t)(target - line) & 0x80) && !(game_peek(g, field(u, 0x11)) & 0x80)) {
            game_set_word(g, field(u, 0x24), AWAY_FLAPPED);
            flap = 1;
        }
        ahead(g, u, flap);
        return BRAIN_SET;
    }
    }
    return BRAIN_UNPORTED;
}

/* $D83D: the brain, the facing, frames since a flap, the erase nodes; as brain() */
static int think(Game *g, uint16_t u, StepResult *slept)
{
    int r = brain(g, u, slept);
    if (r != BRAIN_SET) return r;
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

/* $8E14: the box's right edge and the pictures for the frame at +$17 */
static void frame_tail(Game *g, uint16_t u)
{
    uint8_t b;
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

void rider_frame(Game *g, uint16_t u, uint8_t frame)
{
    game_poke(g, field(u, 0x17), frame);
    uint8_t b = (uint8_t)(game_peek(g, field(u, 0xB)) - 6);
    game_poke(g, field(u, 0x27), b);
    game_poke(g, field(u, 0x28), (uint8_t)(b - 0x0D));
    frame_tail(g, u);
}

static uint16_t table(const Game *g, uint16_t u, int off)
{
    return game_read_word(g, (uint16_t)(game_word(g, field(u, 0xD)) + off));
}

/* $8D45: a sleep of the table's +$1A frames, resuming at `resume` */
static StepResult rest(Game *g, uint16_t u, uint16_t resume)
{
    return task_sleep(g, game_read(g, game_read_word(g, (uint16_t)(game_word(g, field(u, 0xD)) + 0x1A))), resume);
}

/* $8DEF: standing: the box (13 lines), the pictures, the rest */
static StepResult standing(Game *g, uint16_t u)
{
    uint8_t b = game_peek(g, field(u, 0xB));
    game_poke(g, field(u, 0x27), b);
    game_poke(g, field(u, 0x28), (uint8_t)(b - 0x13));
    frame_tail(g, u);
    pictures(g, u, 1);
    return rest(g, u, RIDER_STANDING);
}

/* $8F87: landed: the run animation by the speed, at rest vertically */
static StepResult landed(Game *g, uint16_t u)
{
    uint8_t a = game_peek(g, field(u, 0x14));
    if (a) {
        game_poke(g, field(u, 0xF), 0);
        if (a & 0x80) {
            game_poke(g, field(u, 0xF), 0xFF); /* COM after CLR */
            a = (uint8_t)-a;
        }
    }
    game_set_word(g, field(u, 0x20), game_read_word(g, (uint16_t)(RUN_SPEEDS + (int8_t)a)));
    game_poke(g, field(u, 0x16), 0);
    game_poke(g, field(u, 0x15), 0);
    game_poke(g, field(u, 0x13), 1);
    game_set_word(g, field(u, 0x11), 0);
    game_poke(g, field(u, 0x17), 0x0C);
    return standing(g, u);
}

/* the frame, the platforms, then the pictures and a sleep; or a landing */
static StepResult air_end(Game *g, uint16_t u, uint8_t frame, uint16_t resume)
{
    rider_frame(g, u, frame);
    int r = platform_below(g, u);
    if (r < 0) return STEP_UNPORTED;
    if (!r) return landed(g, u);
    pictures(g, u, 1);
    return rest(g, u, resume);
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

/* $8F6D: a flap's sound and frames, then on in the air */
static StepResult flapped(Game *g, uint16_t u)
{
    game_sound(g, table(g, u, 0x1E));
    game_poke(g, field(u, 0x13), 0);
    uint8_t frames = game_peek(g, field(u, 0x15));
    game_poke(g, field(u, 0x15), 5);
    return air_end(g, u, (int8_t)frames > 0 ? 0x60 : 0x48, RIDER_FLAPPING);
}

/* $8F0A: flapping on: no lift, the flap's frames counted down */
static StepResult flap_on(Game *g, uint16_t u)
{
    if (fly(g, u, 0)) return STEP_UNPORTED;
    if ((int8_t)game_peek(g, field(u, 0x15)) > 0) game_poke(g, field(u, 0x15), (uint8_t)(game_peek(g, field(u, 0x15)) - 1));
    return air_end(g, u, 0x48, RIDER_FLAPPING);
}

/* $8EA4: taking off from the ground */
static StepResult take_off(Game *g, uint16_t u)
{
    pictures(g, u, 0);
    game_set_word(g, field(u, 0x11), 0xFF80);
    game_poke(g, field(u, 0xB), (uint8_t)(game_peek(g, field(u, 0xB)) - 1));
    game_poke(g, field(u, 0xC), 0);
    game_poke(g, field(u, 9), 0);
    game_poke(g, field(u, 0x23), 0);
    game_set_word(g, field(u, 0x20), 0);
    game_set_word(g, field(u, 0x30), FLYING);
    game_poke(g, field(u, 0x15), (uint8_t)(game_peek(g, field(u, 0x15)) + 1));
    return flapped(g, u);
}

/* $8EC8: no ground under it: into the air */
static StepResult fall_off(Game *g, uint16_t u)
{
    if (game_peek(g, field(u, 0x10)) & 0x80) game_sound(g, table(g, u, 0x28));
    game_poke(g, field(u, 0xC), 0);
    game_poke(g, field(u, 9), 0);
    game_poke(g, field(u, 0x23), 0);
    game_poke(g, field(u, 0x15), 0x0A);
    game_set_word(g, field(u, 0x11), 0);
    game_set_word(g, field(u, 0x20), 0);
    game_set_word(g, field(u, 0x30), FLYING);
    game_poke(g, field(u, 0x13), 0);
    if (game_peek(g, CONTROLS + 1)) return flap_on(g, u);
    return glide(g, u);
}

/* the run animation's routines ($DD3A-$DDB1): the frame in A, dx in B */
static int run_frame(Game *g, uint16_t u, uint16_t routine, uint16_t *d)
{
    switch (routine) {
    case 0xDD3A:
        game_poke(g, field(u, 0xF), (uint8_t)~game_peek(g, field(u, 0xF)));
        /* fall through */
    case 0xDD3C:
        game_poke(g, field(u, 0x10), 0);
        *d = 0x0C00;
        return 0;
    case 0xDD4D: { /* the run cycle, a footstep sound every 4 frames */
        uint8_t b = game_peek(g, field(u, 0x10));
        if ((int8_t)b <= 0) {
            game_poke(g, field(u, 0x10), 4);
            *d = game_read_word(g, 0xDD43);
            return 0;
        }
        b--;
        if ((int8_t)b <= 0) b = 4;
        game_poke(g, field(u, 0x10), b);
        if (b == 1) {
            uint8_t steps = (uint8_t)(game_peek(g, field(u, 0x35)) + 1);
            game_poke(g, field(u, 0x35), steps);
            game_sound(g, table(g, u, steps & 1 ? 0x24 : 0x26));
            b = game_peek(g, field(u, 0x10));
        }
        *d = game_read_word(g, (uint16_t)(0xDD43 + (uint8_t)(b << 1)));
        return 0;
    }
    case 0xDD8E: {
        uint8_t b = game_read(g, (uint16_t)(0xDD89 + (int8_t)game_peek(g, field(u, 0x10))));
        game_poke(g, field(u, 0x10), 0);
        *d = (uint16_t)(0x0C00 | b);
        return 0;
    }
    case 0xDD9C: /* the skid, its sound once */
        if (!(game_peek(g, field(u, 0x10)) & 0x80)) game_sound(g, table(g, u, 0x20));
        game_poke(g, field(u, 0x10), 0xFF);
        *d = 0x0002;
        return 0;
    }
    return -1;
}

/* $8D4D: on the ground */
static StepResult ground(Game *g, uint16_t u)
{
    StepResult slept = STEP_DONE;
    int r = brain(g, u, &slept);
    if (r == BRAIN_SLEPT) return slept;
    if (r != BRAIN_SET) return STEP_UNPORTED;
    uint8_t dir = game_peek(g, CONTROLS), flap = game_peek(g, CONTROLS + 1);
    if (!game_peek(g, field(u, 0x13)) && flap) return take_off(g, u);
    game_poke(g, field(u, 0x13), flap);
    if (game_peek(g, field(u, 0xF)) & 0x80) dir = (uint8_t)-dir;
    uint8_t b, wait = game_peek(g, field(u, 0x15));
    int step_on = 1;
    if (wait) {
        game_poke(g, field(u, 0x15), (uint8_t)(wait - 1));
        step_on = wait == 1;
    }
    if (step_on) { /* $8D6C: a step of the run animation by the direction */
        uint16_t x = game_word(g, field(u, 0x20));
        uint8_t next = game_read(g, (uint16_t)(x + (int8_t)(uint8_t)(dir + 4)));
        if (next) {
            game_poke(g, field(u, 0x15), 8);
            game_poke(g, field(u, 0x16), 0);
            x = (uint16_t)(x + (int8_t)next);
            game_set_word(g, field(u, 0x20), x);
            if (x == SKID) game_sound(g, table(g, u, 0x22));
        }
    }
    uint8_t left = (uint8_t)(game_peek(g, field(u, 0x16)) - 1);
    game_poke(g, field(u, 0x16), left);
    if ((int8_t)left > 0) {
        if (!game_peek(g, field(u, 0x22))) return rest(g, u, RIDER_STANDING);
        pictures(g, u, 0);
        b = 0;
    } else {
        pictures(g, u, 0);
        uint16_t x = game_word(g, field(u, 0x20)), d;
        game_poke(g, field(u, 0x14), game_read(g, (uint16_t)(x + 6)));
        game_poke(g, field(u, 0x16), game_read(g, x));
        if (run_frame(g, u, game_read_word(g, (uint16_t)(x + 1)), &d)) return STEP_UNPORTED;
        game_poke(g, field(u, 0x17), (uint8_t)(d >> 8));
        b = (uint8_t)d;
        if (game_peek(g, field(u, 0xF)) & 0x80) {
            game_poke(g, field(u, 0x14), (uint8_t)-game_peek(g, field(u, 0x14)));
            b = (uint8_t)-b;
        }
    }
    rider_move_x(g, u, b);
    /* $8DC1: the ground under its feet, a wall at its side */
    uint16_t x = game_word(g, field(u, 7)), y = game_word(g, field(u, 0xA));
    if (!(game_read(g, (uint16_t)(x - 0x4FFE)) & game_read(g, (uint16_t)(y - 0x1568)) & 0x7F)) return fall_off(g, u);
    uint8_t wall = game_read(g, (uint16_t)(x - 0x5158)) & game_read(g, (uint16_t)(y - 0x17C9));
    if (wall) {
        game_poke(g, 0xA04B, wall | game_peek(g, 0xA04B));
        game_poke(g, field(u, 0xF), 0);
        uint8_t push = game_peek(g, field(u, 0x22));
        if (push & 0x80) push = (uint8_t)-push;
        game_poke(g, field(u, 0x22), (uint8_t)(push + 3));
    }
    return standing(g, u);
}

StepResult rider_step(Game *g, uint16_t u, uint16_t at)
{
    if (at == RIDER_STANDING) return ground(g, u);
    if (game_word(g, field(u, 0x30)) != FLYING) return STEP_UNPORTED;
    StepResult slept = STEP_DONE;
    int r = think(g, u, &slept);
    if (r == BRAIN_SLEPT) return slept;
    if (r != BRAIN_SET) return STEP_UNPORTED;
    uint8_t flapping = game_peek(g, CONTROLS + 1);
    if (at == RIDER_FLAPPING) {
        if (!flapping) { /* $8F25: let go */
            game_sound(g, table(g, u, 0x1C));
            game_poke(g, field(u, 0x13), 0);
            return glide(g, u);
        }
        return flap_on(g, u);
    }
    if (!flapping) return glide(g, u);
    /* $8F63: a new flap */
    flap(g, u);
    if (fly(g, u, 0)) return STEP_UNPORTED;
    return flapped(g, u);
}
