#include "sim/collide.h"

#include "sim/death.h"
#include "sim/eggs.h"
#include "sim/score.h"
#include "sim/tasks.h"

#define PAIRS_START 0x837A
#define PAIRS 0x837F
#define VERSUS_START 0x839F
#define VERSUS 0x83A4
#define WITH 0xA093      /* <$93: the task being tested */
#define OTHER 0xA049     /* <$49: the one against it */
#define DX 0xA034        /* <$34: x difference */
#define COUNTDOWN_1 0xA0A8
#define COUNTDOWN_2 0xA0A9

int collide_ported(uint16_t at) { return at == PAIRS_START || at == PAIRS || at == VERSUS_START || at == VERSUS; }

/* D and the flags the routines branch on, as the 6809 sets them */
typedef struct {
    uint16_t d;
    int n, z, v;
} Acc;

static void ldd(Acc *r, uint16_t m)
{
    r->d = m;
    r->n = m >> 15;
    r->z = !m;
    r->v = 0;
}

static void subd(Acc *r, uint16_t m)
{
    uint16_t res = (uint16_t)(r->d - m);
    r->v = ((r->d ^ m) & (r->d ^ res) & 0x8000) != 0;
    r->d = res;
    r->n = res >> 15;
    r->z = !res;
}

static int lt(const Acc *r) { return r->n != r->v; }
static int gt(const Acc *r) { return !r->z && r->n == r->v; }

int collide_rows_at(const Game *g, uint16_t *px, uint16_t *py, uint16_t dx)
{
    Acc r;
    uint8_t a;
    uint16_t x = *px, y = *py;
    int hit = 0;
#define W(addr) game_read_word(g, (uint16_t)(addr))
#define HIT(n)                       \
    do {                             \
        x = (uint16_t)(x + (n));     \
        y = (uint16_t)(y + (n));     \
        hit = 1;                     \
        goto done;                   \
    } while (0)
#define MISS goto done
dc21:
    ldd(&r, W(x + 2));
    if (lt(&r)) {
        a = (uint8_t)(r.d >> 8);
        goto dc18;
    }
    subd(&r, W(y));
    if (r.v) {
        a = game_read(g, x);
        goto dc18;
    }
    subd(&r, dx);
    if (lt(&r)) goto dc4b;
    ldd(&r, W(x));
    subd(&r, W(y + 2));
    subd(&r, dx);
    if (gt(&r)) goto dc97;
    HIT(0);
dc18:
    a |= game_read(g, y);
    if (a & 1) MISS; /* RORA; BCS: the lists' end */
    x = (uint16_t)(x + 4);
    y = (uint16_t)(y + 4);
    goto dc21;
dc37:
    x = (uint16_t)(x + 16);
    y = (uint16_t)(y + 16);
    ldd(&r, W(x + 2));
    if (lt(&r)) MISS;
    subd(&r, W(y));
    if (r.v) MISS;
    subd(&r, dx);
    if (!lt(&r)) HIT(0);
dc4b:
    ldd(&r, W(x + 6));
    if (lt(&r)) MISS;
    subd(&r, W(y + 4));
    if (r.v) MISS;
    subd(&r, dx);
    if (!lt(&r)) HIT(4);
    ldd(&r, W(x + 0xA));
    if (lt(&r)) MISS;
    subd(&r, W(y + 8));
    if (r.v) MISS;
    subd(&r, dx);
    if (!lt(&r)) HIT(8);
    ldd(&r, W(x + 0xE));
    if (lt(&r)) MISS;
    subd(&r, W(y + 0xC));
    if (r.v) MISS;
    subd(&r, dx);
    if (lt(&r)) goto dc37;
    HIT(0xC);
dc83:
    x = (uint16_t)(x + 16);
    y = (uint16_t)(y + 16);
    ldd(&r, W(x));
    if (lt(&r)) MISS;
    subd(&r, W(y + 2));
    if (r.v) MISS;
    subd(&r, dx);
    if (!gt(&r)) HIT(0);
dc97:
    ldd(&r, W(x + 4));
    if (lt(&r)) MISS;
    subd(&r, W(y + 6));
    if (r.v) MISS;
    subd(&r, dx);
    if (!gt(&r)) HIT(4);
    ldd(&r, W(x + 8));
    if (lt(&r)) MISS;
    subd(&r, W(y + 0xA));
    if (r.v) MISS;
    subd(&r, dx);
    if (!gt(&r)) HIT(8);
    ldd(&r, W(x + 0xC));
    if (lt(&r)) MISS;
    subd(&r, W(y + 0xE));
    if (r.v) MISS;
    subd(&r, dx);
    if (gt(&r)) goto dc83;
    HIT(0xC);
done:
    *px = x;
    *py = y;
    return hit;
#undef HIT
#undef MISS
#undef W
}

int collide_rows(const Game *g, uint16_t x, uint16_t y, uint16_t dx) { return collide_rows_at(g, &x, &y, dx); }

/* ---- a collision's outcome ($8418-$8635) ------------------------------ */

static uint16_t F(uint16_t t, int off) { return (uint16_t)(t + off); }
static int8_t sp(const Game *g, uint16_t t, int off) { return (int8_t)game_peek(g, F(t, off)); }

/* $8599: x pushed down (+$23 $FE), a falling velocity reversed and halved */
static void bump_down(Game *g, uint16_t x)
{
    game_poke(g, F(x, 0x23), 0xFE);
    int16_t v = (int16_t)game_word(g, F(x, 0x11));
    if (v > 0) game_set_word(g, F(x, 0x11), (uint16_t)((int16_t)-v >> 1));
}

/* $85AD: x pushed up (+$23 2), a rising velocity reversed and halved */
static void bump_up(Game *g, uint16_t x)
{
    game_poke(g, F(x, 0x23), 2);
    int16_t v = (int16_t)game_word(g, F(x, 0x11));
    if (v < 0) game_set_word(g, F(x, 0x11), (uint16_t)((int16_t)-v >> 1));
}

/* $8539: apart sideways by the x difference <$34: speeds turned, pushes, facings */
static void apart(Game *g, uint16_t u, uint16_t x)
{
    uint16_t d = game_word(g, DX);
    if (!d) return;
    int8_t a;
    if (d & 0x8000) {
        a = sp(g, x, 0x14);
        if (a > 0) game_poke(g, F(x, 0x14), (uint8_t)(a = (int8_t)(-a + 2)));
        game_poke(g, F(u, 0x22), (uint8_t)((int8_t)(-a + 2) >> 1));
        game_poke(g, F(x, 0xF), 0xFF);
        a = sp(g, u, 0x14);
        if (a < 0) game_poke(g, F(u, 0x14), (uint8_t)(a = (int8_t)(-a - 2)));
        game_poke(g, F(x, 0x22), (uint8_t)((int8_t)(-(int8_t)(a + 2)) >> 1));
        game_poke(g, F(u, 0xF), 0);
    } else {
        a = sp(g, x, 0x14);
        if (a < 0) game_poke(g, F(x, 0x14), (uint8_t)(a = (int8_t)(-a - 2)));
        game_poke(g, F(u, 0x22), (uint8_t)((int8_t)(-(int8_t)(a + 2)) >> 1));
        game_poke(g, F(x, 0xF), 0);
        a = sp(g, u, 0x14);
        if (a > 0) game_poke(g, F(u, 0x14), (uint8_t)(a = (int8_t)(-a + 2)));
        game_poke(g, F(x, 0x22), (uint8_t)((int8_t)(-a + 2) >> 1));
        game_poke(g, F(u, 0xF), 0xFF);
    }
}

/* $852D: u pushed up, x down, then apart; $851F: the other way */
static void bounce(Game *g, uint16_t u, uint16_t x, int u_up)
{
    if (u_up) {
        bump_up(g, u);
        bump_down(g, x);
    } else {
        bump_down(g, u);
        bump_up(g, x);
    }
    apart(g, u, x);
}

/* $85D7: x off u by their boxes: up or down by their lines, sideways by their x */
static void off_box(Game *g, uint16_t u, uint16_t x)
{
    int d = (int)game_peek(g, F(x, 0x27)) - game_peek(g, F(u, 0x27)) + game_peek(g, F(x, 0x28)) -
            game_peek(g, F(u, 0x28));
    if (d < 0) {
        bump_down(g, x);
        game_poke(g, F(x, 0x23), 0xFB);
    } else {
        bump_up(g, x);
        game_poke(g, F(x, 0x23), 0x05);
    }
    uint16_t s = (uint16_t)(game_word(g, F(x, 7)) - game_word(g, F(u, 7)) + game_word(g, F(x, 0x29)) -
                            game_word(g, F(u, 0x29)));
    int8_t a = sp(g, x, 0x14);
    if (s & 0x8000) {
        if (a > 0) game_poke(g, F(x, 0x14), (uint8_t)-(int8_t)(a - 2));
        game_poke(g, F(x, 0x22), (uint8_t)(-sp(g, u, 0x14) << 1));
        game_poke(g, F(x, 0xF), 0xFF);
    } else {
        if (a < 0) game_poke(g, F(x, 0x14), (uint8_t)(-a - 2));
        game_poke(g, F(x, 0x22), (uint8_t)(-sp(g, u, 0x14) << 1));
        game_poke(g, F(x, 0xF), 0);
    }
}

/* $84E0: `loser` unhorsed by `winner`: its mount flies off, its table's
   +$10 routine ($74C3 an egg; $8279 a player's death: not ported), the
   winner scored by its +$16 ($E6A8 or $E6B1). 0, or -1 not ported. */
static int unhorse(Game *g, uint16_t loser, uint16_t winner)
{
    uint16_t t = game_word(g, F(loser, 0xD));
    uint16_t then = game_read_word(g, (uint16_t)(t + 0x10)), score = game_read_word(g, (uint16_t)(t + 0x16));
    if ((then != 0x74C3 && then != 0x8279 && then != 0x827F) || (score != 0xE6A8 && score != 0xE6B1)) return -1;
    game_poke(g, F(loser, 2), game_peek(g, F(loser, 2)) & 0x7F);
    game_set_word(g, F(loser, 0x1A), 0);
    game_set_word(g, F(loser, 0x24), 0xEEC2);
    if (then == 0x74C3) eggs_lay(g, loser, winner);
    else death_player(g, loser, winner, then == 0x827F);
    score_add(g, game_word(g, F(winner, 0xD)), game_read(g, (uint16_t)(t + 0x2F)), score == 0xE6B1);
    return 0;
}

/* $84B7: bounced by their lines */
static void by_lines(Game *g, uint16_t u, uint16_t x)
{
    bounce(g, u, x, (int8_t)(uint8_t)(game_peek(g, F(x, 0xB)) - game_peek(g, F(u, 0xB))) < 0);
}

/* $8418: 0 the walk goes on ($8413), 1 on to the next pair ($84DD), -1 a path not ported */
static int outcome(Game *g, uint16_t u, uint16_t x)
{
    uint8_t both = game_peek(g, F(u, 2)) & game_peek(g, F(x, 2));
    if (!(both & 1)) { /* $7535: an egg collected; or a SWI */
        if (both & 2) return -1;
        eggs_collect(g, u, x);
        return 0;
    }
    if (both & 4) { /* $8487 */
        game_sound(g, 0xEF9D);
        if (game_peek(g, F(u, 2)) == 0x97) {
            if (game_peek(g, F(x, 2)) == 0x97) by_lines(g, u, x);
            else off_box(g, u, x);
        } else if (game_peek(g, F(x, 2)) == 0x97) {
            off_box(g, x, u);
        } else {
            by_lines(g, u, x);
        }
        return 0;
    }
    if (!(both & 8)) { /* $8431: level lances, facing each other */
        uint8_t b = (uint8_t)(game_peek(g, F(u, 0x26)) + game_peek(g, F(u, 0xB)) - game_peek(g, F(x, 0xB)));
        int near;
        if (game_peek(g, F(x, 0x17)) > 0x0C) {
            b = (uint8_t)(b - 8);
            if (b & 0x80) b = (uint8_t)-b;
            near = b <= 3;
        } else {
            b = (uint8_t)(b - 0x0A);
            if (b & 0x80) b = (uint8_t)-b;
            near = b <= 2;
        }
        if (near && ((game_peek(g, F(u, 0xF)) ^ game_peek(g, F(x, 0xF))) & 0x80)) {
            int right = !(game_peek(g, F(u, 0xF)) & 0x80);
            if ((game_word(g, DX) & 0x8000) ? !right : right) goto x_loses;
        }
    }
    /* $8466: by their lance lines */
    int d = (int)sp(g, x, 0x26) - sp(g, u, 0x26) + (int)(int16_t)game_word(g, F(x, 0xA)) -
            (int)(int16_t)game_word(g, F(u, 0xA));
    if ((uint16_t)d & 0x8000) { /* $84C6: u unhorsed */
        if (unhorse(g, u, x)) return -1;
        if (game_peek(g, F(x, 2)) == 0x97) off_box(g, x, u);
        else bounce(g, u, x, 1);
        return 1;
    }
    if ((uint16_t)d) goto x_loses;
    game_sound(g, 0xEFCE);                /* $847D: a tie */
    bounce(g, u, x, 1);
    return 1;
x_loses: /* $850B */
    if (unhorse(g, x, u)) return -1;
    if (game_peek(g, F(u, 2)) == 0x97) off_box(g, u, x);
    else bounce(g, u, x, 0);
    return 0;
}

/* $83D8: 0 apart (or met, the walk going on), 1 met (on to the next pair), -1 a path not ported */
int collide_pair(Game *g, uint16_t u, uint16_t x)
{
    game_set_word(g, OTHER, x);
    Acc r;
    ldd(&r, game_word(g, (uint16_t)(u + 7)));
    subd(&r, game_word(g, (uint16_t)(x + 0x29)));
    if (gt(&r)) return 0;
    ldd(&r, game_word(g, (uint16_t)(x + 7)));
    Acc cmp = r;
    subd(&cmp, game_word(g, (uint16_t)(u + 0x29)));
    if (gt(&cmp)) return 0;
    subd(&r, game_word(g, (uint16_t)(u + 7)));
    game_set_word(g, DX, r.d);
    uint8_t top = game_peek(g, (uint16_t)(x + 0x27)), b = game_peek(g, (uint16_t)(x + 0x28));
    if (b > game_peek(g, (uint16_t)(u + 0x27))) return 0;
    if (top < game_peek(g, (uint16_t)(u + 0x28))) return 0;
    uint16_t y = game_read_word(g, game_word(g, (uint16_t)(x + 0x1C)));
    uint16_t xx = game_read_word(g, game_word(g, (uint16_t)(u + 0x1C)));
    b = (uint8_t)((uint8_t)(b - game_peek(g, (uint16_t)(u + 0x28))) << 2);
    if (b & 0x80) {
        b = (uint8_t)-b;
        y = (uint16_t)(y + (int8_t)b); /* LEAY B,Y */
    } else {
        xx = (uint16_t)(xx + b); /* ABX */
    }
    if (!collide_rows(g, xx, y, r.d)) return 0;
    return outcome(g, u, x);
}

/* $837F: every pair of riders whose types share bit 0 */
static StepResult pairs(Game *g, uint16_t u)
{
    for (;;) {
        u = game_word(g, u);
        if (!u) return task_sleep(g, 1, PAIRS);
        if (!(game_peek(g, (uint16_t)(u + 2)) & 0x80)) continue;
        game_set_word(g, WITH, u);
        for (uint16_t x = game_word(g, u); x; x = game_word(g, x)) {
            uint8_t a = game_peek(g, (uint16_t)(x + 2));
            if (!(a & 0x80) || !(a & game_peek(g, (uint16_t)(u + 2)) & 1)) continue;
            int r = collide_pair(g, u, x);
            if (r < 0) return STEP_UNPORTED;
            if (r) break; /* BCS: the next pair */
        }
    }
}

/* $83A4: the countdowns, then each task of type $89 against the riders after it */
static StepResult versus(Game *g, uint16_t u)
{
    if (game_peek(g, COUNTDOWN_1)) game_poke(g, COUNTDOWN_1, (uint8_t)(game_peek(g, COUNTDOWN_1) - 1));
    if (game_peek(g, COUNTDOWN_2)) game_poke(g, COUNTDOWN_2, (uint8_t)(game_peek(g, COUNTDOWN_2) - 1));
    for (;;) {
        u = game_word(g, u);
        if (!u) return task_sleep(g, 1, VERSUS);
        if (game_peek(g, (uint16_t)(u + 2)) != 0x89) continue;
        game_set_word(g, WITH, u);
        uint16_t x = u;
        do { /* $83BE: the first rider after it; none ends the step */
            x = game_word(g, x);
            if (!x) return task_sleep(g, 1, VERSUS);
        } while (!(game_peek(g, (uint16_t)(x + 2)) & 0x80));
        int r = collide_pair(g, u, x);
        if (r < 0) return STEP_UNPORTED;
        if (r) continue; /* BCS $83B0 */
        for (x = game_word(g, x); x; x = game_word(g, x)) { /* $83CA: the others */
            if (!(game_peek(g, (uint16_t)(x + 2)) & 0x80)) continue;
            r = collide_pair(g, u, x);
            if (r < 0) return STEP_UNPORTED;
            if (r) break;
        }
    }
}

StepResult collide_step(Game *g, uint16_t task, uint16_t at)
{
    switch (at) {
    case PAIRS_START: return task_sleep(g, 1, PAIRS);
    case PAIRS: return pairs(g, task);
    case VERSUS_START: return task_sleep(g, 1, VERSUS);
    case VERSUS: return versus(g, task);
    }
    return STEP_UNPORTED;
}
