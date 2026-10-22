#include "sim/ptero.h"

#include "sim/brains.h"
#include "sim/display.h"
#include "sim/platform.h"
#include "sim/points.h"
#include "sim/rider.h"
#include "sim/tasks.h"

#define START 0x68A8
#define START_CLEAR 0x68A5
#define FLYING 0x6925
#define GONE 0x6816
#define DYING_LIT 0x6826
#define DYING_DARK 0x6844
#define BODY_DRAWN 0x686B
#define BODY_WAIT 0x687A
#define BODY_GONE 0x688C

#define LOOK 0x6640
#define SWOOP 0x6695
#define CLIMB 0x6795
#define AWAY 0x67B3
#define DEATH 0x6828

#define BEATS 0x69BF  /* the wing beat: frame, frames */
#define SPEEDS 0x69CF /* x per frame (8.8) by +$14 */

int ptero_ported(uint16_t at)
{
    return at == START || at == START_CLEAR || at == FLYING || at == GONE || at == DYING_LIT ||
           at == DYING_DARK || at == BODY_DRAWN || at == BODY_WAIT || at == BODY_GONE;
}

static uint16_t F(uint16_t u, int off) { return (uint16_t)(u + off); }
static uint8_t peek(const Game *g, uint16_t a) { return game_peek(g, a); }
static void poke(Game *g, uint16_t a, uint8_t v) { game_poke(g, a, v); }
static uint16_t word(const Game *g, uint16_t a) { return game_word(g, a); }

/* +$11-12 as a signed word */
static int16_t vel(const Game *g, uint16_t u) { return (int16_t)word(g, F(u, 0x11)); }
static void set_vel(Game *g, uint16_t u, int16_t v) { game_set_word(g, F(u, 0x11), (uint16_t)v); }

/* `ASRA; RORB` twice */
static void vel_quarter(Game *g, uint16_t u) { set_vel(g, u, (int16_t)(vel(g, u) >> 2)); }

/* $6704-$672C / $67D1-$67FB: toward line `a`, up to a line a frame */
static void toward(Game *g, uint16_t u, uint8_t a)
{
    uint8_t d = (uint8_t)(a - peek(g, F(u, 0xB)));
    int16_t v = vel(g, u);
    if (!d) vel_quarter(g, u);
    else if (d & 0x80) {
        if (v > -0x100) set_vel(g, u, (int16_t)(v - 0x20));
    } else if (v < 0x100) {
        set_vel(g, u, (int16_t)(v + 0x20));
    }
}

/* $6736 (slow, 4) / $6755 (fast, 8): the speed index and facing by the
   push or the way it flies; no controls */
static void speed(Game *g, uint16_t u, uint8_t s)
{
    int8_t push = (int8_t)peek(g, F(u, 0x22));
    uint8_t a = s;
    if (push) {
        if (push < 0) a = (uint8_t)-s;
    } else if (peek(g, F(u, 0x14)) & 0x80) {
        a = (uint8_t)-s;
    }
    poke(g, F(u, 0x14), a);
    poke(g, F(u, 0xF), a);
    game_set_word(g, 0xA032, 0);
}

static void fast(Game *g, uint16_t u)
{
    poke(g, F(u, 0x14), peek(g, F(u, 0xF)));
    speed(g, u, 8);
}

/* $6788: a climb for 2 frames */
static void climb(Game *g, uint16_t u)
{
    game_set_word(g, F(u, 0x24), CLIMB);
    poke(g, F(u, 0x2D), 2);
    speed(g, u, 4);
}

/* $66F4: no target: the middle of its third of the screen */
static void cruise(Game *g, uint16_t u)
{
    uint8_t a = 0x43, b = peek(g, F(u, 0xB));
    if (b >= 0x65) {
        if (b >= 0xA1) a = 0xCE; /* $D78C */
        else if (!peek(g, F(u, 0x34))) a = 0x7F;
        else {
            int16_t x = (int16_t)word(g, F(u, 7));
            a = (x > 0x010B || x - 0x010B <= (int16_t)0xFF9A) ? 0x88 : 0x7F;
        }
    }
    toward(g, u, a);
    speed(g, u, 4);
}

/* $6695: the swoop: on until +$33 runs out or it is past the x it went for */
static void swoop(Game *g, uint16_t u)
{
    if (peek(g, F(u, 0x34))) { /* $D770 */
        uint8_t t = (uint8_t)(peek(g, F(u, 0x2F)) - 1);
        poke(g, F(u, 0x2F), t);
        if (!t) poke(g, F(u, 0x2F), 1);
    }
    uint8_t left = (uint8_t)(peek(g, F(u, 0x33)) - 1);
    poke(g, F(u, 0x33), left);
    int on = 0;
    if (left) {
        uint16_t d = (uint16_t)(word(g, F(u, 0x2B)) - word(g, F(u, 7)));
        if (d & 0x8000) on = (uint16_t)(d + 0x0D) & 0x8000;
        else on = !((uint16_t)(d - 0x0D) & 0x8000);
    }
    if (!on) { /* $66AD */
        game_set_word(g, F(u, 0x24), LOOK);
        poke(g, F(u, 0x2E), 0);
        fast(g, u);
        return;
    }
    if (peek(g, F(u, 0x17)) == 0x18) poke(g, F(u, 0x15), 0x0A);
    uint8_t a = peek(g, F(u, 0xB)), to = peek(g, F(u, 0x2E));
    if (a == to) poke(g, F(u, 0x11), 0);
    else if (a < to) {
        if ((int8_t)peek(g, F(u, 0x11)) <= 0) set_vel(g, u, (int16_t)(vel(g, u) + 0x10));
    } else if (!(peek(g, F(u, 0x11)) & 0x80)) {
        set_vel(g, u, (int16_t)(vel(g, u) - 0x10));
    }
    fast(g, u);
}

/* $6640: looking for a player every +$2F frames */
static void look(Game *g, uint16_t u)
{
    uint8_t t = (uint8_t)(peek(g, F(u, 0x2F)) - 1);
    poke(g, F(u, 0x2F), t);
    if (!t) {
        poke(g, F(u, 0x2F), peek(g, F(u, 0x35)));
        poke(g, F(u, 0x2E), 0);
        uint16_t x;
        if (!brain_target(g, u, &x)) {
            cruise(g, u);
            return;
        }
        if (peek(g, F(u, 0x35)) > 0x0F) poke(g, F(u, 0x35), (uint8_t)(peek(g, F(u, 0x35)) - 1));
        game_set_word(g, F(u, 0x2B), word(g, F(x, 7)));
        uint8_t b = peek(g, F(x, 0xB));
        if (b >= 0xD2) b = 0xD2;
        b = (uint8_t)(b - 5);
        if (peek(g, F(u, 0x34))) b = (uint8_t)(b + 2); /* $D7BD */
        poke(g, F(u, 0x2E), b);
    }
    uint8_t b = peek(g, F(u, 0x2E)), line = peek(g, F(u, 0xB));
    if (!b) {
        cruise(g, u);
        return;
    }
    if (b < line) { /* $6779: below it: up */
        if (vel(g, u) > (int16_t)0xFF40) set_vel(g, u, (int16_t)(vel(g, u) - 0x40));
        climb(g, u);
        return;
    }
    if ((uint8_t)(b - 0x0F) > line) { /* $67A2: above it: down */
        if (vel(g, u) < 0x100) set_vel(g, u, (int16_t)(vel(g, u) + 0x10));
        climb(g, u);
        return;
    }
    poke(g, F(u, 0x33), 0x80); /* $D7A8 */
    if (peek(g, F(u, 0x34))) {
        for (int i = 0; i < 2; i++) { /* ASR +$11, ROR +$12 */
            uint8_t hi = peek(g, F(u, 0x11)), lo = peek(g, F(u, 0x12));
            poke(g, F(u, 0x11), (uint8_t)((hi >> 1) | (hi & 0x80)));
            poke(g, F(u, 0x12), (uint8_t)((lo >> 1) | (hi & 1) << 7));
        }
    }
    game_sound(g, 0xEF7D);
    game_set_word(g, F(u, 0x24), SWOOP);
    swoop(g, u);
}

/* $6951: the picture by frame and facing, the box, the draw node, then
   off the screen with no enemies left, and the platforms beside it */
static void frame(Game *g, uint16_t u)
{
    uint8_t b = peek(g, F(u, 0x17));
    if (peek(g, F(u, 0xF)) & 0x80) b = (uint8_t)(b + 6);
    uint16_t rec = (uint16_t)(game_read_word(g, 0x0034) + b);
    game_set_word(g, F(u, 0x1C), rec);
    b = peek(g, F(u, 0xB));
    if (b >= 0xD2) {
        poke(g, F(u, 0x11), 0);
        poke(g, F(u, 0x12), 0);
        b = 0xD2;
        poke(g, F(u, 0xB), b);
    }
    poke(g, F(u, 0x27), b);
    poke(g, F(u, 0x28), (uint8_t)(b - 0x0A));
    game_set_word(g, F(u, 0x29), (uint16_t)(word(g, F(u, 7)) + 0x1B));
    if (rec) display_fill(g, display_object(g, u, 1, 1), rec); /* $8E83 */
    int away = !peek(g, 0xA0AA);
    if (!away && peek(g, 0xA0AC) && peek(g, F(u, 0x34))) {
        poke(g, F(u, 0x34), 0);
        poke(g, 0xA0AB, (uint8_t)(peek(g, 0xA0AB) - 1));
        away = 1;
    }
    if (away && word(g, F(u, 0x24)) < AWAY) game_set_word(g, F(u, 0x24), AWAY);
    uint16_t x = word(g, F(u, 7)), y = word(g, F(u, 0xA));
    uint8_t bits = (uint8_t)((game_read(g, (uint16_t)(x - 0x515E)) | game_read(g, (uint16_t)(x - 0x5154))) &
                             game_read(g, (uint16_t)(y - 0x17C3)) & game_read(g, (uint16_t)(y - 0x17C2)));
    if (bits) platform_hit(g, u, bits);
}

/* $6928: the wing beat, the flight, the frame, a frame's sleep */
static StepResult fly(Game *g, uint16_t u)
{
    uint8_t n = peek(g, F(u, 0x15));
    poke(g, F(u, 0x15), (uint8_t)(n - 1));
    if ((int8_t)n <= 1) {
        uint8_t b = (uint8_t)(peek(g, F(u, 0x10)) - 2);
        if (b & 0x80) b = 6;
        poke(g, F(u, 0x10), b);
        uint16_t d = game_read_word(g, (uint16_t)(BEATS + (int8_t)b));
        poke(g, F(u, 0x17), (uint8_t)(d >> 8));
        poke(g, F(u, 0x15), (uint8_t)d);
    }
    if (rider_fly(g, u, word(g, F(u, 0x11)), SPEEDS)) return STEP_UNPORTED;
    frame(g, u);
    return task_sleep(g, 1, FLYING);
}

/* $68A8: a new pterodactyl */
static StepResult start(Game *g, uint16_t u)
{
    uint8_t a = peek(g, F(u, 0x34));
    for (int b = 7; b < 0x36; b++) poke(g, F(u, b), 0);
    poke(g, F(u, 0x34), a);
    poke(g, F(u, 0x14), 2);
    uint16_t x = 0xFFF7;
    int carry;
    (void)game_random(g, &carry);
    if (carry) {
        poke(g, F(u, 0xF), (uint8_t)~peek(g, F(u, 0xF)));
        poke(g, F(u, 0x14), (uint8_t)-peek(g, F(u, 0x14)));
        x = 0x0123;
    }
    game_set_word(g, F(u, 7), x);
    game_set_word(g, 0xA088, 0);
    poke(g, 0xA08A, 0);
    for (uint16_t p = 0xA073; p <= 0xA075; p += 2) { /* $D800: the players by thirds */
        uint16_t r = word(g, p);
        if (!r) break;
        uint8_t line = peek(g, F(r, 0xB));
        uint16_t third = line >= 0xA9 ? 0xA08A : line >= 0x51 ? 0xA089 : 0xA088;
        poke(g, third, (uint8_t)(peek(g, third) + 1));
    }
    x = 0x00D1;
    if (peek(g, 0xA08A)) x = peek(g, 0xA089) ? 0x003D : 0x0080;
    game_set_word(g, F(u, 0xA), x);
    game_set_word(g, F(u, 0x24), LOOK);
    game_sound(g, 0xEF78);
    poke(g, F(u, 2), (uint8_t)(peek(g, F(u, 2)) | 0x80));
    poke(g, F(u, 0x26), 0x80);
    game_set_word(g, F(u, 0xD), 0x8A14);
    poke(g, F(u, 0x35), 0x78);
    poke(g, F(u, 0x2F), peek(g, F(u, 0x34)) ? 0x8A : 0x3C); /* $D781 */
    poke(g, F(u, 0x13), 0);
    return fly(g, u);
}

void ptero_killed(Game *g, uint16_t u)
{
    poke(g, F(u, 0x2D), 7);
    game_set_word(g, F(u, 0x24), DEATH);
    game_sound(g, 0xEF6D);
}

/* $6834: dark, then lit after 4 frames */
static StepResult dying(Game *g, uint16_t u)
{
    (void)rider_pictures(g, u, 0);
    poke(g, F(u, 0x17), 0x18);
    frame(g, u);
    return task_sleep(g, 4, DYING_DARK);
}

/* $6860: its body, in two passes */
static StepResult body(Game *g, uint16_t u)
{
    uint16_t x = word(g, F(u, 7));
    poke(g, F(u, 0x10), (uint8_t)(x >> 1)); /* $6895 */
    poke(g, F(u, 0x11), (uint8_t)(peek(g, F(u, 0xB)) - 0x0A));
    display_pixels(g, u, word(g, F(u, 0x13)));
    return task_sleep(g, 2, BODY_DRAWN);
}

int ptero_brain(Game *g, uint16_t u)
{
    switch (word(g, F(u, 0x24))) {
    case LOOK: look(g, u); return 0;
    case SWOOP: swoop(g, u); return 0;
    case CLIMB: {
        uint8_t n = peek(g, F(u, 0x2D));
        poke(g, F(u, 0x2D), (uint8_t)(n - 1));
        if ((int8_t)n <= 1) game_set_word(g, F(u, 0x24), LOOK);
        speed(g, u, 4);
        return 0;
    }
    case AWAY: {
        int16_t x = (int16_t)word(g, F(u, 7));
        if (x < -7 || x > 0x121) return -1; /* $6808 */
        uint8_t a = 0x43, b = peek(g, F(u, 0xB));
        if (b >= 0x65) a = b >= 0xA1 ? 0xCE : 0x7F;
        toward(g, u, a);
        fast(g, u);
        return 0;
    }
    }
    return -1;
}

/* the brain at +$24 (D83D), then on in the air */
static StepResult flying(Game *g, uint16_t u)
{
    uint16_t state = word(g, F(u, 0x24));
    if (state == DEATH) { /* $6828 */
        poke(g, F(u, 0xF), (uint8_t)~peek(g, F(u, 0xF)));
        if (peek(g, F(u, 0x34))) {
            poke(g, 0xA0AB, (uint8_t)(peek(g, 0xA0AB) - 1));
            poke(g, 0xA0B0, 1); /* $D768 */
            poke(g, F(u, 0x34), 0);
        }
        return dying(g, u);
    }
    if (ptero_brain(g, u)) {
        if (state != AWAY) return STEP_UNPORTED;
        (void)rider_pictures(g, u, 0); /* $6808: off the screen */
        poke(g, F(u, 2), peek(g, F(u, 2)) & 0x7F);
        return task_sleep(g, 0x3C, GONE);
    }
    uint8_t since = (uint8_t)(peek(g, F(u, 0x13)) + 1);
    poke(g, F(u, 0x13), since);
    if (!since) poke(g, F(u, 0x13), 0xFF);
    (void)rider_pictures(g, u, 0);
    return fly(g, u);
}

StepResult ptero_step(Game *g, uint16_t u, uint16_t at)
{
    switch (at) {
    case START_CLEAR:
        poke(g, F(u, 0x34), 0);
        return start(g, u);
    case START: return start(g, u);
    case FLYING: return flying(g, u);
    case GONE: return task_end(g);
    case DYING_LIT: return dying(g, u);
    case DYING_DARK: {
        (void)rider_pictures(g, u, 0);
        if (!(peek(g, F(u, 0x2D)) & 0xFC)) poke(g, F(u, 0xF), 0);
        uint8_t n = (uint8_t)(peek(g, F(u, 0x2D)) - 1);
        poke(g, F(u, 0x2D), n);
        if (!(n & 0x80)) { /* $6819 */
            poke(g, F(u, 0x17), 0x0C);
            frame(g, u);
            return task_sleep(g, 4, DYING_LIT);
        }
        poke(g, F(u, 0x12), 3);
        game_set_word(g, F(u, 0x13), game_read_word(g, 0x0038));
        return body(g, u);
    }
    case BODY_DRAWN: {
        uint16_t x = word(g, F(u, 7));
        poke(g, F(u, 0x10), (uint8_t)(x >> 1));
        poke(g, F(u, 0x11), (uint8_t)(peek(g, F(u, 0xB)) - 0x0A));
        game_set_word(g, F(u, 0x13), display_pixels(g, u, word(g, F(u, 0x13))));
        return task_sleep(g, 6, BODY_WAIT);
    }
    case BODY_WAIT: {
        uint8_t n = (uint8_t)(peek(g, F(u, 0x12)) - 1);
        poke(g, F(u, 0x12), n);
        if (n) return body(g, u);
        uint16_t node = rider_pictures(g, u, 0);
        if (!node) return STEP_UNPORTED;
        poke(g, node, 0x12);
        poke(g, F(node, 1), 0);
        return task_sleep(g, 2, BODY_GONE);
    }
    case BODY_GONE: /* its points */
        game_set_word(g, F(u, 0x2E), 0x0444);
        return points_step(g, u, 0x7861);
    }
    return STEP_UNPORTED;
}
