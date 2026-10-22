#include "sim/irq.h"

#include "sim/start.h"

static uint8_t peek(const Game *g, uint16_t a) { return game_peek(g, a); }
static void poke(Game *g, uint16_t a, uint8_t v) { game_poke(g, a, v); }
static uint16_t word(const Game *g, uint16_t a) { return game_word(g, a); }
static void set_word(Game *g, uint16_t a, uint16_t v) { game_set_word(g, a, v); }

/* $437B: a BCD byte to binary */
static uint8_t binary(uint8_t b) { return (uint8_t)((b >> 4) * 10 + (b & 0x0F)); }

/* $4365: a / b as BCD (0 when b is 0); *rest the remainder */
static uint8_t divide(uint8_t a, uint8_t b, uint8_t *rest)
{
    if (!b) {
        *rest = b;
        return 0;
    }
    uint8_t q = 0x99;
    int r = a;
    do {
        int c, h;
        q = game_add(q, 1, 0, &c, &h);
        q = game_daa(q, &c, h);
        r -= b;
    } while (r >= 0);
    *rest = (uint8_t)(r + b);
    return q;
}

/* $42E7: a (BCD) credits more, at most 99, the CMOS's copy, the sound and
   the credits shown ($632E) */
static void credit(Game *g, uint8_t a)
{
    int c, h;
    uint8_t n = game_add(a, peek(g, 0xA0F2), 0, &c, &h);
    n = game_daa(n, &c, h);
    if (c) n = 0x99;
    poke(g, 0xA0F2, n);
    poke(g, 0xCD01, n);
    poke(g, 0xCD00, (uint8_t)(n >> 4));
    game_sound(g, 0xEF1B);
    poke(g, 0xA0B3, (uint8_t)(peek(g, 0xA0B3) + 1));
    start_credits(g, 0x12, 0x00);
}

/* $430D: a coin in slot n (1 left, 2 centre, 3 right) */
static void coin(Game *g, uint8_t n)
{
    game_audit(g, n);
    uint8_t units = binary(game_cmos_byte(g, (uint16_t)(0xCC06 + 2 * n)));
    poke(g, 0xA0F4, (uint8_t)(peek(g, 0xA0F4) + units));
    uint8_t a = (uint8_t)(peek(g, 0xA0F3) + units);
    poke(g, 0xA0F3, a);
    if (a < binary(game_cmos_byte(g, 0xCC12))) return; /* not the minimum yet */
    uint8_t rest, credits = divide(a, binary(game_cmos_byte(g, 0xCC0E)), &rest);
    poke(g, 0xA0F3, rest);
    uint8_t bonus = divide(peek(g, 0xA0F4), binary(game_cmos_byte(g, 0xCC10)), &rest);
    if (bonus) {
        poke(g, 0xA0F3, 0);
        poke(g, 0xA0F4, 0);
    }
    int c, h;
    a = game_add(bonus, credits, 0, &c, &h);
    a = game_daa(a, &c, h);
    game_audit_add(g, 4, a);
    credit(g, a);
}

/* $3DBD: the high score reset, if held: the best one back to the ROM's ($3E42) */
static void reset_best(Game *g)
{
    poke(g, 0xA0E2, 0x18);
    if (g->on_sound) g->on_sound(g->user, 0x3F);
    for (;;) { /* (24 delays of $F945's length, the switch read after each) */
        if (!(game_read(g, 0xC80C) & 0x08)) return;
        poke(g, 0xA0E2, (uint8_t)(peek(g, 0xA0E2) - 1));
        if (!peek(g, 0xA0E2)) break;
    }
    uint16_t y = 0xCD3E;
    for (uint16_t x = 0x3E42; x < 0x3E42 + 0x17; x++) {
        uint8_t a = game_read(g, x);
        poke(g, (uint16_t)(y + 1), a);
        poke(g, y, (uint8_t)(a >> 4));
        y = (uint16_t)(y + 2);
    }
    uint8_t sum = 0; /* $3F88 */
    for (uint16_t x = 0xCD3E; x != 0xCD74; x++)
        if (x != 0xCD6C) sum = (uint8_t)(sum + game_read(g, x));
    poke(g, 0xCD6C, sum);
    if (g->on_sound) g->on_sound(g->user, 0x00);
}

/* $E3C4-$E44F: the top of the frame's work; 1 the advance pressed */
static int top(Game *g)
{
    for (int i = 0; i < 16; i++) g->palette[i] = peek(g, (uint16_t)(0xA00F + i));
    if (peek(g, 0xA090) & 0x80) { /* a minute of play */
        int16_t d = (int16_t)(word(g, 0xA099) - 1);
        if (d <= 0) {
            game_audit(g, 6);
            d = 0x0E10;
        }
        set_word(g, 0xA099, (uint16_t)d);
    }
    if (peek(g, 0xA08D)) poke(g, 0xA08D, (uint8_t)(peek(g, 0xA08D) - 1));
    set_word(g, 0xA0BB, word(g, 0xA0BA));
    set_word(g, 0xA0B9, word(g, 0xA0B8));
    set_word(g, 0xA0B7, word(g, 0xA0B6));
    set_word(g, 0xA0B5, word(g, 0xA0B4));
    uint8_t a = game_read(g, 0xC80C);
    poke(g, 0xA0B4, a);
    if (a & 0x40) poke(g, 0xA08D, 0x78);
    uint8_t b = 0;
    for (uint16_t x = 0xA0B4; x <= 0xA0BC; x++) b |= peek(g, x);
    b &= peek(g, 0xA08B);
    poke(g, 0xA08C, b);
    a = (uint8_t)((a & peek(g, 0xA0B5)) | peek(g, 0xA08C));
    b = peek(g, 0xA08B);
    poke(g, 0xA08B, a);
    b = (uint8_t)(~b & peek(g, 0xA08B) & 0x3E);
    if (!b) return 0;
    b >>= 1;
    if (b & 1) return 1; /* the advance: the test mode */
    if (peek(g, 0xA08D)) { /* tilted: (the ROM's shifts then take the right coin's bit for the reset's) */
        if (b & 2) reset_best(g);
        return 0;
    }
    if (b & 2) coin(g, 3);
    if (b & 4) reset_best(g);
    if (!peek(g, 0xA08D)) {
        if (b & 8) coin(g, 1);
        if (b & 0x10) coin(g, 2);
    }
    return 0;
}

/* $E451: the frame before's display list, band by band behind the beam */
static void draw(Game *g)
{
    uint16_t x = word(g, 0xA025);
    uint8_t a = peek(g, 0xA023);
    uint8_t flag = peek(g, x);
    if (!flag) return;
    if (!(flag & 0x80)) { /* a new list */
        poke(g, x, (uint8_t)~flag);
        set_word(g, 0xA021, (uint16_t)(x + 1));
        a = 0;
    }
    for (;;) {
        poke(g, 0xA023, a);
        if (!((a ^ game_read(g, 0xCB00)) & 0xC0)) return; /* the beam is in it */
        x = word(g, 0xA021);
        uint16_t u = word(g, x);
        set_word(g, 0xA021, (uint16_t)(x + 2));
        for (; u; u = word(g, (uint16_t)(u + 8)))
            game_blit(g, peek(g, u), peek(g, (uint16_t)(u + 1)), word(g, (uint16_t)(u + 2)), word(g, (uint16_t)(u + 4)),
                      peek(g, (uint16_t)(u + 6)), peek(g, (uint16_t)(u + 7)));
        a = (uint8_t)(peek(g, 0xA023) + 0x40);
        if (a) continue;
        poke(g, word(g, 0xA025), 0);
        poke(g, 0xA024, (uint8_t)~peek(g, 0xA024));
        set_word(g, 0xA025, peek(g, 0xA024) & 0x80 ? 0xAC21 : 0xA9C0);
        return;
    }
}

int irq_run(Game *g)
{
    set_word(g, 0xA01F, g->irq_s);
    poke(g, 0xA00D, (uint8_t)(peek(g, 0xA00D) + 1));
    poke(g, 0xA00E, (uint8_t)(peek(g, 0xA00E) - 1));
    uint8_t sel = g->romsel;
    g->romsel = peek(g, 0xBFFF) | 1;
    int advance = 0;
    if (!(game_read(g, 0xCB00) & 0xC0)) advance = top(g);
    draw(g);
    g->romsel = peek(g, 0xBFFF);
    (void)sel;
    return advance;
}
