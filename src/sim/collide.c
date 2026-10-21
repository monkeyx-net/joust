#include "sim/collide.h"

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

/* $83D8: 0 apart, -1 a collision (the handlers: not ported) */
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
    return collide_rows(g, xx, y, r.d) ? -1 : 0;
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
            if (collide_pair(g, u, x)) return STEP_UNPORTED;
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
        if (collide_pair(g, u, x)) return STEP_UNPORTED;
        for (x = game_word(g, x); x; x = game_word(g, x)) { /* $83CA: the others */
            if (!(game_peek(g, (uint16_t)(x + 2)) & 0x80)) continue;
            if (collide_pair(g, u, x)) return STEP_UNPORTED;
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
