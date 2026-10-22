#include "sim/operator.h"

#include "sim/hiscores.h"
#include "sim/start.h"
#include "sim/text.h"

#include <stddef.h>

/* ---- the machine's pieces ---------------------------------------------------- */

static uint8_t peek(const Game *g, uint16_t a) { return game_peek(g, a); }
static void poke(Game *g, uint16_t a, uint8_t v) { game_poke(g, a, v); }
static uint16_t word(const Game *g, uint16_t a) { return game_word(g, a); }
static void set_word(Game *g, uint16_t a, uint16_t v) { game_set_word(g, a, v); }
static uint8_t rd(const Game *g, uint16_t a) { return game_read(g, a); }
static uint16_t rd_word(const Game *g, uint16_t a) { return game_read_word(g, a); }

static void colour(Game *g, int i, uint8_t v) { g->palette[i & 15] = v; }
static void colours_all(Game *g, uint8_t v) /* $F75A */
{
    for (int i = 0; i < 16; i++) g->palette[i] = v;
}
static void test_palette(Game *g) /* $F45A */
{
    for (int i = 0; i < 16; i++) g->palette[i] = rd(g, (uint16_t)(0xF015 + i));
}

static int advance(const Game *g) { return (rd(g, 0xC80C) & 0x02) != 0; }
static void sound_latch(Game *g, uint8_t v)
{
    if (g->on_sound) g->on_sound(g->user, v);
}

/* $F945: a delay of a units, in frames ((a + 1) x $300 loops of 13 cycles) */
static long delay(uint8_t a) { return ((long)a + 1) * 3 / 5 + 1; }

static uint8_t cmos(const Game *g, uint16_t x) { return game_cmos_byte(g, x); }
static uint16_t put(Game *g, uint16_t x, uint8_t a) /* $40FA */
{
    poke(g, (uint16_t)(x + 1), a);
    poke(g, x, (uint8_t)(a >> 4));
    return (uint16_t)(x + 2);
}
static uint16_t store(Game *g, uint16_t x, uint16_t y, int n) /* $3BCF */
{
    for (; n > 0; n--) y = put(g, y, rd(g, x++));
    return y;
}
static void copy(Game *g, uint16_t x, uint16_t y, int n) /* $40B1 */
{
    for (; n > 0; n--) poke(g, y++, rd(g, x++));
}
static uint8_t daa_add(uint8_t a, uint8_t b, int cin, int *c)
{
    int h, cc;
    uint8_t r = game_add(a, b, cin, &cc, &h);
    r = game_daa(r, &cc, h);
    if (c) *c = cc;
    return r;
}

/* ---- the CMOS's sums and tables ------------------------------------------------- */

static uint8_t nibble_sum(Game *g, uint16_t x, uint16_t end) /* $3D16 */
{
    set_word(g, 0xA0D5, end);
    uint8_t a = 0;
    for (; x != end; x++) a = (uint8_t)(a + (rd(g, x) & 0x0F));
    return (uint8_t)(a + 0x37);
}
static uint8_t adjust_sum(Game *g) { return nibble_sum(g, 0xCC00, 0xCC24); }
static uint8_t message_sum(Game *g) { return nibble_sum(g, 0xCC24, 0xCC8C); }
static void adjust_check(Game *g) { put(g, 0xCC8C, adjust_sum(g)); } /* $3CF6 */

static uint8_t best_sum(const Game *g) /* $3F91 */
{
    uint8_t a = 0;
    for (uint16_t x = 0xCD3E; x != 0xCD74; x++)
        if (x != 0xCD6C) a = (uint8_t)(a + rd(g, x));
    return a;
}
static void best_check(Game *g) { poke(g, 0xCD6C, best_sum(g)); } /* $3F88 */

static uint8_t entry_sum(const Game *g, uint16_t y) /* $3FAF */
{
    uint8_t a = 0;
    for (int i = 0; i < 14; i++)
        if (i != 6) a = (uint8_t)(a + rd(g, (uint16_t)(y + i)));
    return a;
}
static void entry_check(Game *g, uint16_t y) { poke(g, (uint16_t)(y + 6), entry_sum(g, y)); } /* $3FA7 */

static void delete_entry(Game *g, uint16_t y) /* $4091 */
{
    for (uint16_t x = (uint16_t)(y + 0x0E); x < 0xCFA4; x = (uint16_t)(x + 0x0E), y = (uint16_t)(y + 0x0E))
        copy(g, x, y, 0x0E);
    store(g, 0x3F75, y, 7);
    entry_check(g, y);
}

/* $404B: a characters ($0A-$24) and b BCD score bytes at y: 1 if bad */
static int entry_bad(const Game *g, uint16_t y, int a, int b)
{
    uint16_t x = y;
    for (; a > 0; a--, x = (uint16_t)(x + 2)) {
        uint8_t v = cmos(g, x);
        if (v < 0x0A || v > 0x24) return 1;
    }
    uint8_t v = cmos(g, x);
    x = (uint16_t)(x + 2);
    if ((v & 0x0F) > 9) return 1;
    for (b--; b > 0; b--, x = (uint16_t)(x + 2)) {
        v = cmos(g, x);
        if ((v & 0x0F) > 9 || (v & 0xF0) > 0x99) return 1;
    }
    return 0;
}

static void best_from_first(Game *g) /* $4018 */
{
    for (uint16_t x = 0xCD3E; x < 0xCD66;) x = put(g, x, 0x0A);
    copy(g, 0xCD74, 0xCD3E, 6);
    copy(g, 0xCD74, 0xCD66, 6);
    copy(g, 0xCD7A, 0xCD6C, 8);
    best_check(g);
    delete_entry(g, 0xCD74);
}

void operator_tables_validate(Game *g) /* $3FC1 */
{
    uint8_t fixes = 0x32;
    uint16_t y = 0xCD74;
    while (y < 0xCFA4) {
        if (((entry_sum(g, y) ^ rd(g, (uint16_t)(y + 6))) & 0x0F) || entry_bad(g, y, 3, 4)) {
            delete_entry(g, y);
            poke(g, 0xCD00, 0);
            poke(g, 0xCD01, 0);
            if (!--fixes) break;
            continue;
        }
        y = (uint16_t)(y + 0x0E);
    }
    store(g, 0x3F0C, 0xCFA4, 0x2A);
    if (((best_sum(g) ^ rd(g, 0xCD6C)) & 0x0F) || entry_bad(g, 0xCD3E, 0x17, 4)) best_from_first(g);
}

void operator_tables_reset(Game *g) /* $3E09 */
{
    store(g, 0x3E42, 0xCD3E, 0x7D);
    store(g, 0x3EBF, 0xCE38, 0xB6);
    best_check(g);
    for (uint16_t y = 0xCD74; y < 0xCFA4; y = (uint16_t)(y + 0x0E)) entry_check(g, y);
    text_block(g, 0x0E, 0, 0);
}

static void audits_clear(Game *g) /* $3D98 */
{
    text_block(g, 0x0D, 0, 0);
    for (uint16_t x = 0xCD02; x < 0xCD3E; x++) poke(g, x, 0);
}

int operator_cmos_check_now(Game *g) /* $3D36 */
{
    if (message_sum(g) != cmos(g, 0xCC8E)) { /* $3BF9 */
        store(g, 0x3C23, 0xCC24, 0x34);
        put(g, 0xCC8E, message_sum(g));
    }
    if (adjust_sum(g) == cmos(g, 0xCC8C)) {
        operator_tables_validate(g);
        return 0;
    }
    store(g, 0x3C11, 0xCC00, 0x12); /* $3BEA */
    adjust_check(g);
    start_clear_screen(g);
    int bad = 4; /* $3D82 */
    for (uint16_t x = 0xCD02; x != 0xCD3E; x++)
        if ((rd(g, x) & 0x0F) > 9 && !--bad) {
            audits_clear(g);
            break;
        }
    bad = 8; /* $3DE9 */
    for (uint16_t y = 0xCD74; y < 0xCFA4; y = (uint16_t)(y + 0x0E))
        if (((entry_sum(g, y) ^ rd(g, (uint16_t)(y + 6))) & 0x0F) && !--bad) {
            operator_tables_reset(g);
            break;
        }
    operator_tables_validate(g);
    text_block(g, adjust_sum(g) == cmos(g, 0xCC8C) ? 0x0C : 0x0B, 0, 0);
    return 1;
}

/* ---- the RAM, ROM and CMOS tests --------------------------------------------------- */

static uint16_t lfsr(uint16_t d) /* $F0B0 */
{
    unsigned b = d & 9;
    return (uint16_t)((d >> 1) | ((b == 9 || b == 0) ? 0x8000 : 0));
}

/* $F0A2's pass over $0000-$BFFF (written; in C it always reads back right) */
static uint16_t ram_pass(Game *g, uint16_t seed)
{
    g->romsel = 0;
    uint16_t d = seed;
    for (unsigned x = 0; x < 0xC000; x += 2) {
        d = lfsr(d);
        g->ram[x] = (uint8_t)(d >> 8);
        g->ram[x + 1] = (uint8_t)d;
    }
    g->romsel = 1;
    return d;
}

static int rom_test(const Game *g) /* $F290 */
{
    for (uint16_t x = 0xF320; x != 0xF340; x = (uint16_t)(x + 2)) {
        uint8_t want = rd(g, (uint16_t)(x + 1));
        if (!want) continue;
        uint8_t sum = 0;
        uint16_t u = (uint16_t)(rd(g, x) << 8);
        do sum = (uint8_t)(sum + rd(g, u++));
        while ((u >> 8) != rd(g, (uint16_t)(x + 2)));
        if (sum != want) return 0;
    }
    return 1;
}

static uint8_t cmos_random(Game *g) /* $FEAD */
{
    uint8_t b = (uint8_t)(peek(g, 0xA0D9) * 3 + 0x11);
    uint8_t a = (uint8_t)(((peek(g, 0xA0DB) >> 3) ^ peek(g, 0xA0DB)));
    uint8_t da = peek(g, 0xA0DA), db = peek(g, 0xA0DB);
    poke(g, 0xA0DA, (uint8_t)((a & 1) << 7 | da >> 1)); /* LSRA's carry into ROR <$DA */
    poke(g, 0xA0DB, (uint8_t)((da & 1) << 7 | db >> 1));
    unsigned s = (unsigned)b + peek(g, 0xA0DB);
    b = (uint8_t)((uint8_t)s + peek(g, 0xA0DA) + (s > 0xFF));
    poke(g, 0xA0D9, b);
    return b;
}

int operator_cmos_test(Game *g) /* $FE4A */
{
    for (int i = 0; i < 0x400; i++) poke(g, (uint16_t)(0x9C00 + i), g->cmos[i]);
    int ok = 1;
    for (int pass = 6; pass && ok; pass--) {
        uint16_t u = word(g, 0xA0DA), y = word(g, 0xA0D9);
        for (uint16_t x = 0xCC00; x != 0xD000; x++) poke(g, x, cmos_random(g));
        set_word(g, 0xA0D9, y);
        set_word(g, 0xA0DA, u);
        for (uint16_t x = 0xCC00; x != 0xD000; x++)
            if ((cmos_random(g) ^ rd(g, x)) & 0x0F) {
                ok = 0;
                break;
            }
    }
    for (int i = 0; i < 0x400; i++) poke(g, (uint16_t)(0xCC00 + i), peek(g, (uint16_t)(0x9C00 + i)));
    return ok;
}

/* ---- the test screens ---------------------------------------------------------- */

void operator_sound_show(Game *g) /* $FDDC (but its waits) */
{
    sound_latch(g, 0x3F);
    sound_latch(g, 0x00);
    sound_latch(g, 0x3F);
    sound_latch(g, peek(g, 0xA0F8) & 0x3F);
    uint16_t x = text_message_small(g, 0x22, 0x99, 0x3A80);
    text_number_small(g, peek(g, 0xA0F9) | 0xF0, 0x99, x);
}

/* $FE2F: the number erased, the next command */
static void sound_next(Game *g)
{
    text_number_small(g, peek(g, 0xA0F9) | 0xF0, 0x00, 0x5780);
    uint8_t a = (uint8_t)(peek(g, 0xA0F8) << 1 | 1), b = (uint8_t)(peek(g, 0xA0F9) + 1);
    if (b >= 7) {
        a = 0xFE;
        b = 0x01;
    }
    set_word(g, 0xA0F8, (uint16_t)(a << 8 | b));
}

void operator_sound_step(Game *g)
{
    operator_sound_show(g);
    poke(g, 0xA0F7, 0x40);
    while (peek(g, 0xA0F7)) poke(g, 0xA0F7, (uint8_t)(peek(g, 0xA0F7) - 1)); /* (64 waits, the advance not pressed) */
    if (peek(g, 0xA0D7) || (rd(g, 0xC80C) & 0x01)) sound_next(g);
}

static void switches_read(Game *g, uint16_t u) /* $F7AE */
{
    for (;; u = (uint16_t)(u + 4)) {
        uint16_t x = rd_word(g, u);
        if (!x) return;
        uint16_t y = rd_word(g, (uint16_t)(u + 2));
        poke(g, (uint16_t)(y + 1), (uint8_t)(rd(g, x) ^ peek(g, y)));
    }
}

static void switches_show(Game *g) /* $F7BE / $F7E8 */
{
    uint16_t u = 0xF85F;
    for (uint16_t y = 0xA0F7; y <= 0xA100; y = (uint16_t)(y + 2))
        for (int bit = 1; bit < 0x100; bit <<= 1, u = (uint16_t)(u + 3)) {
            if (!(bit & peek(g, (uint16_t)(y + 1)))) continue;
            sound_latch(g, 0x3F);
            poke(g, y, (uint8_t)(peek(g, y) ^ bit));
            uint8_t row = rd(g, (uint16_t)(u + 2));
            if (!row) continue;
            uint16_t x = (uint16_t)(0x40 << 8 | row);
            if (peek(g, y) & bit) {
                x = text_message(g, rd(g, u), 0xBB, x);
                text_number_large(g, rd(g, (uint16_t)(u + 1)), 0xBB, x);
                sound_latch(g, 0x3C);
            } else {
                game_blit(g, 0x12, 0x00, 0, x, 0x34, 0x02);
            }
        }
}

/* $F781: one look at the switches */
void operator_switches(Game *g)
{
    switches_read(g, 0xF847);
    game_poke(g, 0xC807, 0x34);
    switches_read(g, 0xF855);
    game_poke(g, 0xC807, 0x3C);
    switches_show(g);
}

/* $F715 / $F4EA's fill: 16 bars of 15 columns from x = 0, each $900 on ($D00 after a 0 first) */
static void fill_bars(Game *g, uint16_t first, int word_step)
{
    uint16_t x = 0, d = first;
    for (int n = 0;; n++) {
        uint16_t from = x;
        set_word(g, 0xA0D5, from);
        x = (uint16_t)(x + 0x0F00);
        uint8_t a = word_step ? (uint8_t)(d >> 8) : rd(g, (uint16_t)(0xF74A + n));
        uint8_t b = word_step ? (uint8_t)d : a;
        while (x != from) {
            x = (uint16_t)(x - 2);
            poke(g, x, a);
            poke(g, (uint16_t)(x + 1), b);
        }
        x = (uint16_t)(x + 0x0900);
        if (!a) x = 0x0D00;
        if (word_step) {
            unsigned s = (unsigned)d + 0x1111;
            if (s > 0xFFFF) return;
            d = (uint16_t)s;
        } else if (n == 15) {
            return;
        }
    }
}

void operator_colour_bars(Game *g) { fill_bars(g, 0, 0); } /* $F715 */

void operator_bars(Game *g) /* $F4EA */
{
    for (int i = 0; i < 16; i++) colour(g, i, rd(g, (uint16_t)(0xF52A + i)));
    fill_bars(g, 0x0000, 1);
}

void operator_crosshatch(Game *g) /* $F53A */
{
    start_clear_screen(g);
    colours_all(g, 0);
    g->romsel = 0;
    colour(g, 1, 0xFF);
    colour(g, 2, 0xC0);
    colour(g, 3, 0x38);
    colour(g, 4, 0x07);
    for (uint16_t y = 0xF659; y != 0xF681; y = (uint16_t)(y + 4))
        for (uint16_t x = rd_word(g, y); x != rd_word(g, (uint16_t)(y + 2)); x = (uint16_t)(x + 2)) {
            poke(g, x, 0x01);
            poke(g, (uint16_t)(x + 1), 0x01);
        }
    for (uint16_t y = 0xF639; y != 0xF659; y = (uint16_t)(y + 4)) {
        uint16_t x = rd_word(g, y);
        set_word(g, 0xA0D5, x);
        do {
            poke(g, x, 0x11);
            poke(g, 0xA0D5, (uint8_t)(peek(g, 0xA0D5) + 1));
            x = word(g, 0xA0D5);
        } while (x != rd_word(g, (uint16_t)(y + 2)));
    }
    for (uint16_t y = 0xF681; y != 0xF6BD; y = (uint16_t)(y + 5)) {
        uint16_t x = rd_word(g, y);
        set_word(g, 0xA0D5, x);
        do {
            poke(g, x, rd(g, (uint16_t)(y + 4)));
            poke(g, 0xA0D5, (uint8_t)(peek(g, 0xA0D5) + 1));
            x = word(g, 0xA0D5);
        } while (x != rd_word(g, (uint16_t)(y + 2)));
    }
    for (uint16_t y = 0xF6BD; y != 0xF6D1; y = (uint16_t)(y + 5)) {
        uint16_t x = rd_word(g, y);
        do poke(g, x++, rd(g, (uint16_t)(y + 4)));
        while (x != rd_word(g, (uint16_t)(y + 2)));
    }
    poke(g, 0x437E, 0x21);
    poke(g, 0x937E, 0x20);
    for (uint16_t x = 0x4B0A; x != 0x4B6D; x++) poke(g, x, (uint8_t)((rd(g, x) & 0xF0) | 0x02));
    for (uint16_t x = 0x4B90; x != 0x4BF3; x++) poke(g, x, (uint8_t)((rd(g, x) & 0xF0) | 0x02));
    set_word(g, 0xA0D5, 0x0B18);
    for (;;) {
        uint16_t x = word(g, 0xA0D5);
        poke(g, x, (uint8_t)((rd(g, x) & 0xF0) | 0x01));
        unsigned b = (unsigned)peek(g, 0xA0D6) + 0x22;
        if (b <= 0xFF) {
            poke(g, 0xA0D6, (uint8_t)b);
            continue;
        }
        poke(g, 0xA0D6, 0x18);
        uint8_t c = (uint8_t)(peek(g, 0xA0D5) + 0x10);
        poke(g, 0xA0D5, c);
        if (c == 0x9B) break;
    }
    g->romsel = 1;
}

/* ---- the bookkeeping ($F962-$FA20) and its averages ($FA27) ------------------- */

static int bcd_add(Game *g, uint16_t x, uint16_t y, uint16_t u, int n) /* $FA7A (4) / $FA80 (3) */
{
    int c = 0;
    while (n--) {
        x--, y--, u--;
        poke(g, u, daa_add(peek(g, x), peek(g, y), c, &c));
    }
    return c;
}

static void bcd_negate(Game *g, uint16_t y) /* $FAE2: the 3 bytes before y */
{
    for (int i = 1; i <= 3; i++) poke(g, (uint16_t)(y - i), (uint8_t)(0x99 - peek(g, (uint16_t)(y - i))));
    int c = 1;
    for (int i = 1; i <= 3; i++) poke(g, (uint16_t)(y - i), daa_add(peek(g, (uint16_t)(y - i)), 0, c, &c));
}

static void shift_digit(Game *g, uint16_t first, uint16_t last) /* ASL last, ROL down to first, 4 times */
{
    for (int k = 0; k < 4; k++) {
        int c = 0;
        for (uint16_t a = last;; a--) {
            uint8_t v = peek(g, a);
            poke(g, a, (uint8_t)(v << 1 | c));
            c = v >> 7;
            if (a == first) break;
        }
    }
}

static void divide(Game *g) /* $FA99 */
{
    if (!(peek(g, 0xA0E2) | peek(g, 0xA0E3)) && !peek(g, 0xA0E4)) {
        set_word(g, 0xA0DF, 0);
        poke(g, 0xA0E1, 0);
        return;
    }
    poke(g, 0xA0E5, 0x07);
    bcd_negate(g, 0xA0E5);
    for (;;) {
        poke(g, 0xA0E5, (uint8_t)(peek(g, 0xA0E5) - 1));
        if (!peek(g, 0xA0E5)) {
            bcd_negate(g, 0xA0E5);
            return;
        }
        shift_digit(g, 0xA0DC, 0xA0E1);
        while (bcd_add(g, 0xA0DF, 0xA0E5, 0xA0E9, 3)) {
            set_word(g, 0xA0DC, word(g, 0xA0E6));
            poke(g, 0xA0DE, peek(g, 0xA0E8));
            poke(g, 0xA0E1, (uint8_t)(peek(g, 0xA0E1) + 1));
        }
    }
}

uint8_t operator_average(Game *g) /* $FA27 */
{
    set_word(g, 0xA0DC, 0); /* $FA92 */
    poke(g, 0xA0DE, 0);
    divide(g);
    if (word(g, 0xA0DF)) {
        poke(g, 0xA0E1, 0x99);
        poke(g, 0xA0E0, 0);
        poke(g, 0xA0DF, 0);
        return 0x99;
    }
    uint8_t a = peek(g, 0xA0E1);
    set_word(g, 0xA0DF, word(g, 0xA0DC));
    poke(g, 0xA0E1, peek(g, 0xA0DE));
    set_word(g, 0xA0DC, 0);
    poke(g, 0xA0DE, 0);
    shift_digit(g, 0xA0DE, 0xA0E1);
    bcd_add(g, 0xA0E2, 0xA0E2, 0xA0E2, 4);
    set_word(g, 0xA0E5, word(g, 0xA0DE));
    set_word(g, 0xA0E7, word(g, 0xA0E0));
    bcd_add(g, 0xA0E9, 0xA0E2, 0xA0E2, 4);
    bcd_add(g, 0xA0E9, 0xA0E2, 0xA0E2, 4);
    divide(g);
    return a;
}

void operator_bookkeeping(Game *g) /* $F962-$FA20 */
{
    text_block(g, 0x07, 0, 0);
    uint16_t u = 0xCD02, x = 0x1A30;
    uint8_t msg = 0x24;
    do {
        text_message_small(g, msg, 0x88, x);
        uint8_t b = cmos(g, u);
        if (!(b & 0xF0)) {
            b |= 0xF0;
            if (!(b & 0x0F)) b |= 0x0F;
        }
        uint8_t a2 = cmos(g, (uint16_t)(u + 2)), b2 = cmos(g, (uint16_t)(u + 4));
        u = (uint16_t)(u + 6);
        if (b == 0xFF && !(a2 & 0xF0)) {
            a2 |= 0xF0;
            if (!(a2 & 0x0F)) {
                a2 |= 0x0F;
                if (!(b2 & 0xF0)) b2 |= 0xF0;
            }
        }
        uint16_t nx = (uint16_t)(0x6A << 8 | (x & 0xFF));
        nx = text_number_small(g, b, 0x88, nx);
        nx = text_number_small(g, a2, 0x88, nx);
        text_number_small(g, b2, 0x88, nx);
        x = (uint16_t)(x + 0x10);
        msg++;
    } while (u <= 0xCD38);
    text_message_small(g, 0x2E, 0x88, x);
    uint16_t nx = (uint16_t)(0x6E << 8 | (x & 0xFF));
    poke(g, 0xA0DF, cmos(g, 0xCD20));
    poke(g, 0xA0E0, cmos(g, 0xCD22));
    poke(g, 0xA0E1, cmos(g, 0xCD24));
    poke(g, 0xA0E2, cmos(g, 0xCD38));
    poke(g, 0xA0E3, cmos(g, 0xCD3A));
    poke(g, 0xA0E4, cmos(g, 0xCD3C));
    uint8_t a = operator_average(g);
    if (!(a & 0xF0)) a |= 0xF0;
    nx = text_number_small(g, a, 0x88, nx);
    nx = text_char_small(g, 0x32, 0x88, nx);
    text_number_small(g, peek(g, 0xA0E1), 0x88, nx);
}

/* ---- the adjustments ($FB02-$FD96) --------------------------------------------- */

/* $FD29: the coinage's preset a into $CC08-$CC12, drawn */
static void coin_preset(Game *g, uint8_t a, uint16_t u, uint16_t y)
{
    game_blit(g, 0x12, 0x00, 0, 0x6A48, 0x00, 0x3C);
    uint16_t x = 0xFD97;
    if (a == 9) text_message(g, 0x6E, peek(g, 0xA0E9), 0x703E);
    x = (uint16_t)(x + (uint8_t)((uint8_t)(a << 1) + (uint8_t)(a << 2)));
    u = (uint16_t)(u + 2);
    while (u != 0xCC14) {
        u = put(g, u, rd(g, x++));
        y = (uint16_t)(y + 0x0A);
        uint8_t v = cmos(g, (uint16_t)(u - 2));
        if (!(v & 0xF0)) v |= 0xF0;
        text_number_large(g, v, peek(g, 0xA0E9), (uint16_t)(0x6A << 8 | (y & 0xFF)));
    }
}

void operator_adjust_value(Game *g, uint16_t u, uint16_t y) /* $FCBA */
{
    uint8_t kind = rd(g, (uint16_t)(0xFD17 + (uint8_t)((uint8_t)u >> 1)));
    uint16_t x = (uint16_t)(0x6A << 8 | (y & 0xFF));
    if ((kind & 0x80) && !(kind & 1)) { /* yes / no */
        text_message(g, cmos(g, u) ? 0x44 : 0x0D, peek(g, 0xA0E9), x);
        return;
    }
    uint8_t a = cmos(g, u);
    if (u == 0xCC06) {
        poke(g, 0xA0D9, a);
        if (!peek(g, 0xA0DC)) coin_preset(g, a, u, y);
    }
    if (!(a & 0xF0)) a |= 0xF0;
    x = text_number_large(g, a, peek(g, 0xA0E9), x);
    if (kind & 0x80) text_char_large(g, 0x2F, peek(g, 0xA0E9), x);
}

void operator_adjust_page(Game *g) /* $FB17-$FB6F: the page and the cursor */
{
    text_block(g, 0x08, 0, 0);
    uint16_t u = 0xCC00, x = 0x1A20;
    uint8_t msg = 0x30;
    do {
        uint16_t y = text_message(g, msg, 0x22, x);
        poke(g, 0xA0E9, 0x22);
        poke(g, 0xA0DC, 0x22);
        operator_adjust_value(g, u, y);
        u = (uint16_t)(u + 2);
        x = (uint16_t)(x + 0x0A);
        msg++;
    } while ((int16_t)u < (int16_t)0xCC24);
    text_block(g, 0x09, 0, 1);
    poke(g, 0xA0DC, 0);
    if ((rd(g, 0xCC07) & 0x0F) == 9) text_message(g, 0x6E, peek(g, 0xA0E9), 0x703E);
    set_word(g, 0xA0F7, 0x3033);
    text_char_large(g, 0x30, 0x33, 0x1620);
}

/* $FB9F's act ($FBCD): the cursor (y, u) moved by the stick a */
void operator_cursor_move(Game *g, uint8_t a, uint16_t *y, uint16_t *u)
{
    if (a & 2) {
        if (*y == 0x16CA) return;
        text_char_large(g, peek(g, 0xA0F7), 0, *y);
        *y = (uint16_t)(*y + 0x0A);
        *u = (uint16_t)(*u + 2);
        if (*u == 0xCC08 && peek(g, 0xA0D9)) {
            *y = (uint16_t)(*y + 0x3C);
            *u = (uint16_t)(*u + 0x0C);
        }
    } else {
        if (!(a & 1) || *y == 0x1620) return;
        text_char_large(g, peek(g, 0xA0F7), 0, *y);
        *y = (uint16_t)(*y - 0x0A);
        *u = (uint16_t)(*u - 2);
        if (*u == 0xCC12 && peek(g, 0xA0D9)) {
            *y = (uint16_t)(*y - 0x3C);
            *u = (uint16_t)(*u - 0x0C);
        }
    }
    text_char_large(g, peek(g, 0xA0F7), peek(g, 0xA0F8), *y);
}

/* $FC51: the adjustment at u stepped by the stick a (within $FC96's limits) */
void operator_value_change(Game *g, uint8_t a, uint16_t u, uint16_t y)
{
    uint8_t v = cmos(g, u);
    poke(g, 0xA0E9, 0);
    operator_adjust_value(g, u, y);
    uint16_t lim = (uint16_t)(0xFC96 + (uint8_t)u);
    a &= 3;
    if (a & 2) {
        if (v != rd(g, (uint16_t)(lim + 1))) put(g, u, daa_add(v, 0x01, 0, NULL));
    } else if (a & 1) {
        if (v != rd(g, lim)) put(g, u, daa_add(v, 0x99, 0, NULL));
    }
    poke(g, 0xA0E9, 0x22);
    operator_adjust_value(g, u, y);
}

/* ---- the operator's message and the best one's name ($4520, $4557, $43A7) ---- */

/* $4520: the input block at $B000 for 25 letters at x, player 1, colour $77 */
static void message_input(Game *g, uint16_t x, uint8_t letters)
{
    uint16_t y = 0xB000;
    set_word(g, (uint16_t)(y + 8), x);
    set_word(g, (uint16_t)(y + 6), 0x43F6);
    poke(g, y, letters);
    for (int i = 0; i < 0x14; i++) poke(g, (uint16_t)(y + 0x0F + i), 0x0A);
    poke(g, (uint16_t)(y + 1), 0);
    poke(g, (uint16_t)(y + 2), 0);
    poke(g, (uint16_t)(y + 0x0F), 0);
    poke(g, (uint16_t)(y + 3), 0x32);
    poke(g, (uint16_t)(y + 4), 0x3C);
    poke(g, (uint16_t)(y + 5), 0x77);
}

/* $43A7: the input block for the best one's name ([$CC16] letters at $2080) */
static void best_input(Game *g)
{
    uint16_t y = 0xB000;
    uint8_t n = cmos(g, 0xCC16), b = 0; /* $437B */
    while (n >= 0x10) {
        b = (uint8_t)(b + 0x0A);
        n = (uint8_t)(n - 0x10);
    }
    poke(g, y, (uint8_t)(b + n));
    for (int i = 0; i < 0x14; i++) poke(g, (uint16_t)(y + 0x0F + i), 0x0A);
    set_word(g, (uint16_t)(y + 6), 0x43F6);
    set_word(g, (uint16_t)(y + 8), 0x2080);
    poke(g, (uint16_t)(y + 2), 0x0A);
    poke(g, (uint16_t)(y + 1), 0);
    poke(g, (uint16_t)(y + 3), 0x25);
    poke(g, (uint16_t)(y + 4), 0x3C);
    poke(g, (uint16_t)(y + 5), 0x77);
}

/* $44E0 / $4500: an operator's line (25 characters from x, its column at
   [at], the line `line`) in colour <$E9 */
static void operator_line(Game *g, uint16_t at, uint8_t line, uint16_t x, uint16_t end)
{
    uint16_t y = (uint16_t)(cmos(g, at) << 8 | line);
    for (; x != end; x = (uint16_t)(x + 2)) y = text_char_small(g, cmos(g, x), peek(g, 0xA0E9), y);
}

/* $45A7: "move with the stick" (messages $51, $52) in colour b */
static void move_note(Game *g, uint8_t b)
{
    text_message_small(g, 0x51, b, 0x2590);
    text_message_small(g, 0x52, b, 0x25A0);
}

/* ---- the program's states --------------------------------------------------- */

enum {
    P_RESET = 1, P_RESET_RAM, P_RESET_OK,
    P_CMOS_CHECK, P_CMOS_MESSAGE, P_REBOOT,
    P_TEST, P_TEST_RELEASE, P_TEST_RAM_Q, P_TEST_RAM, P_TEST_CMOS, P_TEST_CMOS_GO, P_TEST_CMOS_DONE,
    P_SOUND, P_SOUND_PLAY, P_SOUND_LISTEN, P_SOUND_END,
    P_SWITCHES, P_SWITCHES_ON,
    P_COLOURS, P_COLOURS_WAIT, P_COLOURS_CYCLE, P_COLOURS_HOLD,
    P_HATCH, P_RED, P_GREEN, P_BLUE, P_BARS,
    P_BOOK, P_BOOK_SHOW,
    P_ADJUST, P_ADJUST_SHOW, P_ADJUST_LOOP, P_ADJUST_P1_WAIT, P_ADJUST_P1_REPEAT, P_ADJUST_P2_WAIT,
    P_ADJUST_P2_REPEAT, P_ADJUST_P2_DONE,
    P_FINISH, P_FINISH_RELEASE, P_FINISH_AUDITS, P_FINISH_SCORES, P_FINISH_MESSAGE, P_FINISH_MESSAGE_ON,
    P_FINISH_PLACE, P_FINISH_PLACE_HELD, P_FINISH_BEST, P_FINISH_BEST_ON, P_FINISH_LAST,
    P_ADV_PRESS, P_ADV_RELEASE, P_WAIT,
};

static void press(Operator *o, int next) /* $F915, then next */
{
    o->pc = P_ADV_PRESS;
    o->n = next;
}

static void wait_then(Operator *o, long frames, int next)
{
    o->wait = frames;
    o->pc = P_WAIT;
    o->n = next;
}

void operator_reset(Operator *o)
{
    *o = (Operator){ 0 };
    o->pc = P_RESET;
}

void operator_test_mode(Operator *o)
{
    *o = (Operator){ 0 };
    o->pc = P_TEST;
}

void operator_cmos_check(Operator *o)
{
    *o = (Operator){ 0 };
    o->pc = P_CMOS_CHECK;
}

void operator_frame(Operator *o, Game *g)
{
    for (int guard = 0; guard < 64; guard++) {
        uint8_t a;
        switch (o->pc) {
        case P_WAIT:
            if (--o->wait > 0) return;
            o->pc = o->n;
            break;
        case P_ADV_PRESS:
            if (!advance(g)) return;
            colour(g, 0, 0);
            start_clear_screen(g);
            o->pc = P_ADV_RELEASE;
            return;
        case P_ADV_RELEASE:
            if (advance(g)) return;
            o->pc = o->n;
            break;

        /* ---- the reset ($F025) ---- */
        case P_RESET:
            g->romsel = 1;
            test_palette(g);
            o->seed = 0;
            o->count = 2;
            o->pc = P_RESET_RAM;
            break;
        case P_RESET_RAM:
            o->seed = ram_pass(g, o->seed);
            wait_then(o, 104, --o->count ? P_RESET_RAM : P_RESET_OK);
            return;
        case P_RESET_OK:
            g->romsel = 1;
            if (!rom_test(g)) { /* (the chip's number blinks on the board's LEDs; here the message, stopped) */
                start_clear_screen(g);
                text_message_small(g, 0x08, 0x22, 0x4090);
                o->pc = 0;
                return;
            }
            start_clear_screen(g);
            text_message_small(g, 0x05, 0x99, 0x3070);
            text_message_small(g, 0x06, 0x99, 0x3A90);
            wait_then(o, 7 * 26, P_CMOS_CHECK);
            return;

        /* ---- the CMOS's check ($3D36), then the game ($E000) ---- */
        case P_CMOS_CHECK:
            g->romsel = 1;
            o->pc = operator_cmos_check_now(g) ? P_CMOS_MESSAGE : P_REBOOT;
            return;
        case P_CMOS_MESSAGE:
            if (!advance(g)) return;
            o->pc = P_REBOOT;
            break;
        case P_REBOOT:
            o->done = 1;
            return;

        /* ---- the test mode ($F342) ---- */
        case P_TEST:
            game_poke(g, 0xC807, 0x04);
            poke(g, 0xA0D8, 0xFF);
            test_palette(g);
            if (rd(g, 0xC80C) & 0x01) {
                o->pc = P_BOOK;
                break;
            }
            start_clear_screen(g);
            sound_latch(g, 0xC0); /* ($F25E: the LEDs, and $C0 to the sound board) */
            o->pc = P_TEST_RELEASE;
            return;
        case P_TEST_RELEASE:
            if (advance(g)) return;
            if (!rom_test(g)) {
                text_message_small(g, 0x08, 0x22, 0x4090);
                o->pc = 0;
                return;
            }
            start_clear_screen(g);
            text_block(g, 0x00, 0, 0);
            o->wait = 150; /* (3 x $7000 looks at the advance) */
            o->pc = P_TEST_RAM_Q;
            return;
        case P_TEST_RAM_Q:
            if (advance(g)) {
                o->pc = P_TEST_CMOS;
                break;
            }
            if (--o->wait > 0) return;
            o->seed = 0;
            o->wait = 0;
            o->pc = P_TEST_RAM;
            break;
        case P_TEST_RAM: /* passes until the advance */
            if (advance(g)) {
                o->pc = P_TEST_CMOS;
                break;
            }
            if (o->wait-- > 0) return;
            o->seed = ram_pass(g, o->seed);
            o->wait = 104;
            return;
        case P_TEST_CMOS: /* $F3B1 */
            g->romsel = 1;
            start_clear_screen(g);
            text_block(g, 0x01, 0, 0);
            o->pc = P_TEST_CMOS_GO;
            return;
        case P_TEST_CMOS_GO:
            if (advance(g)) return;
            for (uint16_t x = 0x9C00; x != 0xBF01; x++) poke(g, x, 0);
            set_word(g, 0xA0DA, 0xA55A);
            poke(g, 0xA0D8, 0xA5);
            sound_latch(g, 0x00);
            press(o, P_TEST_CMOS_DONE);
            break;
        case P_TEST_CMOS_DONE:
            start_clear_screen(g);
            text_block(g, operator_cmos_test(g) ? 0x02 : 0x03, 0, 0);
            press(o, P_SOUND);
            return;
        case P_SOUND: /* $F41D */
            poke(g, 0xA0D7, 0);
            start_clear_screen(g);
            set_word(g, 0xA0F8, 0xFE01);
            o->pc = P_SOUND_PLAY;
            break;
        case P_SOUND_PLAY:
            operator_sound_show(g);
            poke(g, 0xA0F7, 0x40);
            o->wait = delay(1);
            o->pc = P_SOUND_LISTEN;
            return;
        case P_SOUND_LISTEN:
            if (!advance(g)) {
                if (--o->wait > 0) return;
                o->wait = delay(1);
                poke(g, 0xA0F7, (uint8_t)(peek(g, 0xA0F7) - 1));
                if (peek(g, 0xA0F7)) return;
            }
            if (peek(g, 0xA0D7) || (rd(g, 0xC80C) & 0x01)) sound_next(g);
            if (!advance(g)) {
                o->pc = P_SOUND_PLAY;
                break;
            }
            sound_latch(g, 0x3F);
            sound_latch(g, 0x00);
            press(o, P_SWITCHES);
            break;
        case P_SWITCHES: /* $F765 */
            poke(g, 0xA0D5, 0x0A);
            start_clear_screen(g);
            text_block(g, 0x06, 0, 0);
            for (uint16_t x = 0xA0F7; x <= 0xA100; x++) poke(g, x, 0);
            o->pc = P_SWITCHES_ON;
            break;
        case P_SWITCHES_ON:
            operator_switches(g);
            if (advance(g)) {
                poke(g, 0xA0D5, (uint8_t)(peek(g, 0xA0D5) - 1));
                if (!peek(g, 0xA0D5)) {
                    press(o, P_COLOURS);
                    break;
                }
            }
            return;
        case P_COLOURS: /* $F6D1 */
            start_clear_screen(g);
            text_block(g, 0x05, 0, 0);
            o->wait = 0x80 * 3 / 5;
            o->pc = P_COLOURS_WAIT;
            return;
        case P_COLOURS_WAIT:
            if (advance(g)) {
                press(o, P_HATCH);
                break;
            }
            if (--o->wait > 0) return;
            colours_all(g, rd(g, 0xF742));
            operator_colour_bars(g);
            o->x = 0xF742;
            o->pc = P_COLOURS_CYCLE;
            break;
        case P_COLOURS_CYCLE:
            colours_all(g, rd(g, o->x++));
            set_word(g, 0xA0D5, o->x);
            o->wait = 0x80 * 3 / 5;
            o->pc = P_COLOURS_HOLD;
            return;
        case P_COLOURS_HOLD:
            if (advance(g)) {
                press(o, P_HATCH);
                break;
            }
            if (--o->wait > 0) return;
            if (o->x >= 0xF74A) o->x = 0xF742;
            o->pc = P_COLOURS_CYCLE;
            break;
        case P_HATCH: /* $F4C2 */
            operator_crosshatch(g);
            press(o, P_RED);
            return;
        case P_RED:
            start_clear_screen(g);
            colour(g, 0, 0x07);
            press(o, P_GREEN);
            return;
        case P_GREEN:
            colour(g, 0, 0x38);
            press(o, P_BLUE);
            return;
        case P_BLUE:
            colour(g, 0, 0xC0);
            press(o, P_BARS);
            return;
        case P_BARS:
            operator_bars(g);
            press(o, P_BOOK);
            return;

        /* ---- the bookkeeping ($F955) ---- */
        case P_BOOK:
            test_palette(g);
            if (advance(g)) {
                press(o, P_BOOK_SHOW);
                break;
            }
            o->pc = P_BOOK_SHOW;
            break;
        case P_BOOK_SHOW:
            operator_bookkeeping(g);
            press(o, P_ADJUST);
            return;

        /* ---- the adjustments ($FB02) ---- */
        case P_ADJUST:
            for (uint16_t x = 0xCC18; x < 0xCC24; x++) poke(g, x, 0);
            start_clear_screen(g);
            if (advance(g)) {
                press(o, P_ADJUST_SHOW);
                break;
            }
            o->pc = P_ADJUST_SHOW;
            break;
        case P_ADJUST_SHOW:
            operator_adjust_page(g);
            o->y = 0x1620;
            o->u = 0xCC00;
            o->pc = P_ADJUST_LOOP;
            return;
        case P_ADJUST_LOOP: /* $FB72: player 1's stick moves, player 2's changes */
            a = rd(g, 0xC804) & 0x03;
            if (a) { /* $FB9F */
                poke(g, 0xA0FB, a);
                if (a == peek(g, 0xA0F9)) {
                    poke(g, 0xA0FC, 0x05);
                    o->wait = delay(2);
                    o->pc = P_ADJUST_P1_REPEAT;
                    return;
                }
                poke(g, 0xA0FA, 0);
                poke(g, 0xA0F9, a);
                o->wait = delay(2);
                o->pc = P_ADJUST_P1_WAIT;
                return;
            }
            game_poke(g, 0xC807, 0x34);
            a = rd(g, 0xC804) & 0x03;
            if (a) { /* $FC22 */
                poke(g, 0xA0FB, a);
                if (a == peek(g, 0xA0FD)) {
                    poke(g, 0xA100, 0x08);
                    o->wait = delay(0);
                    o->pc = P_ADJUST_P2_REPEAT;
                    return;
                }
                poke(g, 0xA0FE, 0);
                poke(g, 0xA0FD, a);
                o->wait = delay(2);
                o->pc = P_ADJUST_P2_WAIT;
                return;
            }
            game_poke(g, 0xC807, 0x3C);
            if (advance(g)) press(o, P_FINISH);
            return;
        case P_ADJUST_P1_WAIT:
            if (--o->wait > 0) return;
            a = rd(g, 0xC804);
            if (!a) { /* ($FBB5 with nothing held) */
                poke(g, 0xA0F9, 0);
                o->pc = P_ADJUST_LOOP;
                return;
            }
            operator_cursor_move(g, a, &o->y, &o->u);
            wait_then(o, delay(0), P_ADJUST_LOOP);
            return;
        case P_ADJUST_P1_REPEAT: /* held: 5 x 2 units, then again */
            if (!rd(g, 0xC804)) {
                poke(g, 0xA0F9, 0);
                o->pc = P_ADJUST_LOOP;
                return;
            }
            if (--o->wait > 0) return;
            o->wait = delay(2);
            poke(g, 0xA0FC, (uint8_t)(peek(g, 0xA0FC) - 1));
            if (peek(g, 0xA0FC)) return;
            operator_cursor_move(g, rd(g, 0xC804), &o->y, &o->u);
            wait_then(o, delay(0), P_ADJUST_LOOP);
            return;
        case P_ADJUST_P2_WAIT:
            if (--o->wait > 0) return;
            a = rd(g, 0xC804);
            if (!a) {
                poke(g, 0xA100, 0x08);
                poke(g, 0xA0FE, 0);
                o->pc = P_ADJUST_LOOP;
                return;
            }
            operator_value_change(g, a, o->u, o->y);
            wait_then(o, delay(0), P_ADJUST_P2_DONE);
            return;
        case P_ADJUST_P2_DONE: /* ($FB8F) */
            game_poke(g, 0xC807, 0x3C);
            if (advance(g)) press(o, P_FINISH);
            else o->pc = P_ADJUST_LOOP;
            return;
        case P_ADJUST_P2_REPEAT:
            if (!rd(g, 0xC804)) {
                poke(g, 0xA0FE, 0);
                o->pc = P_ADJUST_LOOP;
                return;
            }
            if (--o->wait > 0) return;
            o->wait = delay(0);
            poke(g, 0xA100, (uint8_t)(peek(g, 0xA100) - 1));
            if (peek(g, 0xA100)) return;
            operator_value_change(g, rd(g, 0xC804), o->u, o->y);
            wait_then(o, delay(0), P_ADJUST_P2_DONE);
            return;

        /* ---- after the adjustments ($3C57) ---- */
        case P_FINISH:
            adjust_check(g);
            start_clear_screen(g);
            o->pc = P_FINISH_RELEASE;
            break;
        case P_FINISH_RELEASE:
            if (advance(g)) return;
            o->pc = P_FINISH_AUDITS;
            break;
        case P_FINISH_AUDITS:
            if (rd(g, 0xCC1B) & 0x0F) {
                poke(g, 0xCC1B, 0);
                adjust_check(g);
                start_clear_screen(g);
                audits_clear(g);
                wait_then(o, delay(0x40), P_FINISH_SCORES);
                return;
            }
            o->pc = P_FINISH_SCORES;
            break;
        case P_FINISH_SCORES:
            if (rd(g, 0xCC1D) & 0x0F) {
                poke(g, 0xCC1D, 0);
                adjust_check(g);
                operator_tables_reset(g);
                wait_then(o, delay(0x40), P_FINISH_MESSAGE);
                return;
            }
            o->pc = P_FINISH_MESSAGE;
            break;
        case P_FINISH_MESSAGE: /* the operator's message: two lines of 25 */
            if (!(rd(g, 0xCC21) & 0x0F)) {
                o->pc = P_FINISH_BEST;
                break;
            }
            poke(g, 0xCC21, 0);
            adjust_check(g);
            start_clear_screen(g);
            text_block(g, 0x0A, 0, 0);
            message_input(g, 0x2560, 0x19);
            o->count = 0;
            o->a = 0x77;
            o->b = 0;
            o->pc = P_FINISH_MESSAGE_ON;
            break;
        case P_FINISH_MESSAGE_ON: { /* $4557: a line entered, then moved */
            uint16_t x;
            if (!hiscores_look(g, 0xB000, &o->a, &o->b, &x)) {
                wait_then(o, delay(4), P_FINISH_MESSAGE_ON);
                return;
            }
            uint16_t at = o->count ? 0xCC56 : 0xCC24;
            for (int i = 0; i < 0x19; i++) at = put(g, at, peek(g, (uint16_t)(0xB00F + i)));
            put(g, o->count ? 0xCC8A : 0xCC88, 0x25);
            move_note(g, 0x88);
            text_char_small(g, 0x34, 0x22, o->count ? 0x487A : 0x486A);
            o->pc = P_FINISH_PLACE;
            break;
        }
        case P_FINISH_PLACE: { /* $4584 / $45F1: the line's column with player 1's stick */
            uint16_t col = o->count ? 0xCC8A : 0xCC88;
            if (advance(g)) { /* $45B7 / $4613 */
                if (!o->count) {
                    text_char_small(g, 0x34, 0x00, 0x486A);
                    move_note(g, 0x00);
                    message_input(g, 0x2570, 0x19);
                    o->count = 1;
                    o->a = 0x77;
                    o->b = 0;
                    o->pc = P_FINISH_MESSAGE_ON;
                    break;
                }
                put(g, 0xCC8E, message_sum(g));
                wait_then(o, delay(rd(g, 0x0020)), P_FINISH_BEST);
                return;
            }
            a = rd(g, 0xC804) & 0x03;
            o->a = a;
            o->wait = delay(4);
            o->pc = P_FINISH_PLACE_HELD;
            (void)col;
            return;
        }
        case P_FINISH_PLACE_HELD: {
            if (--o->wait > 0) return;
            uint16_t col = o->count ? 0xCC8A : 0xCC88;
            a = o->a;
            if (a != rd(g, 0xC804) || !a) {
                o->pc = P_FINISH_PLACE;
                return;
            }
            uint8_t b = cmos(g, col);
            if (a >> 1 == 0) {
                if (b == 0x18) {
                    o->pc = P_FINISH_PLACE;
                    return;
                }
                b--;
            } else {
                if (a >> 2 || b == 0x40) {
                    o->pc = P_FINISH_PLACE;
                    return;
                }
                b++;
            }
            poke(g, 0xA0F7, b);
            poke(g, 0xA0E9, 0);
            if (o->count) operator_line(g, 0xCC8A, 0x70, 0xCC56, 0xCC88);
            else operator_line(g, 0xCC88, 0x60, 0xCC24, 0xCC56);
            put(g, col, peek(g, 0xA0F7));
            poke(g, 0xA0E9, 0x22);
            if (o->count) operator_line(g, 0xCC8A, 0x70, 0xCC56, 0xCC88);
            else operator_line(g, 0xCC88, 0x60, 0xCC24, 0xCC56);
            o->pc = P_FINISH_PLACE;
            return;
        }
        case P_FINISH_BEST:
            if (!(rd(g, 0xCC23) & 0x0F)) {
                o->pc = P_FINISH_LAST;
                break;
            }
            poke(g, 0xCC23, 0);
            adjust_check(g);
            start_clear_screen(g);
            text_block(g, 0x10, 0, 0);
            best_input(g);
            o->a = 0x77;
            o->b = 0x0A;
            o->pc = P_FINISH_BEST_ON;
            break;
        case P_FINISH_BEST_ON: {
            uint16_t x;
            if (!hiscores_look(g, 0xB000, &o->a, &o->b, &x)) {
                wait_then(o, delay(4), P_FINISH_BEST_ON);
                return;
            }
            uint16_t at = 0xCD3E;
            for (int i = 0; i < 0x14; i++) at = put(g, at, peek(g, (uint16_t)(0xB00F + i)));
            best_check(g);
            o->pc = P_FINISH_LAST;
            break;
        }
        case P_FINISH_LAST:
            if (rd(g, 0xCC19) & 0x0F) { /* $3CE5: the factory settings at the check */
                poke(g, 0xCC8C, (uint8_t)(peek(g, 0xCC8C) + 1));
                poke(g, 0xCC8C, (uint8_t)(peek(g, 0xCC8C) + 1));
                poke(g, 0xCC19, 0);
            }
            o->pc = P_CMOS_CHECK; /* ($CC1F's endless self-test is not here) */
            break;
        default:
            return;
        }
    }
}
