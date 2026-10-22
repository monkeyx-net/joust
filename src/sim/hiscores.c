#include "sim/hiscores.h"

#include "sim/start.h"
#include "sim/tasks.h"
#include "sim/text.h"

static uint8_t peek(const Game *g, uint16_t a) { return game_peek(g, a); }
static void poke(Game *g, uint16_t a, uint8_t v) { game_poke(g, a, v); }
static uint16_t word(const Game *g, uint16_t a) { return game_word(g, a); }

/* $FFC4 / $FFBF: a solid block at x in the next colour */
static void border_block(Game *g, uint16_t x, uint8_t w_reg, uint8_t h_reg)
{
    uint8_t a = (uint8_t)(peek(g, 0xA0D5) + 0x11);
    if (a < 0x11) a = 0x88; /* the add's carry */
    poke(g, 0xA0D5, a);
    game_blit(g, 0x12, a, 0, x, w_reg, h_reg);
}

void hiscores_border(Game *g)
{
    poke(g, 0xA0D5, 0x77);
    uint16_t x = 0x0912;
    do {
        border_block(g, x, 0x02, 0x0C);
        x = (uint16_t)(x + 0x600);
    } while (x < 0x8D00);
    do {
        border_block(g, x, 0x00, 0x08);
        x = (uint16_t)(x + 0x0C);
    } while (x < 0x8DE8);
    x = (uint16_t)(x - 0x608);
    for (;;) {
        border_block(g, x, 0x02, 0x0C);
        int borrow = x < 0x09E3;
        x = (uint16_t)(x - 0x600);
        if (borrow || !x) break;
    }
    x = (uint16_t)(x + 0x1FC);
    do {
        border_block(g, x, 0x00, 0x08);
        x = (uint16_t)(x - 0x0C);
    } while (x > 0x050E);
}

/* the colour for the entry at y: the newest ones' own */
static uint8_t colour_of(const Game *g, uint16_t y, uint8_t b)
{
    if (y == word(g, 0xA0EA)) return 0x55;
    if (y == word(g, 0xA0EE)) return 0x77;
    return b;
}

typedef uint16_t (*Printer)(Game *, uint8_t, uint8_t, uint16_t);

/* $4246: <$DC rows from x: the rank (<$E2), initials, the score (leading
   zeros blank), <$E7 columns and <$E1 lines apart; y moves on */
static uint16_t rows(Game *g, uint16_t x, uint16_t y, uint8_t b, Printer number, Printer chr)
{
    game_set_word(g, 0xA0D9, x);
    do {
        poke(g, 0xA0E9, 3);
        poke(g, 0xA0E6, 4);
        uint8_t c = b;
        if (y == word(g, 0xA0EA)) c = 0x55;
        else if (y == word(g, 0xA0EC)) c = 0x55;
        if (y == word(g, 0xA0EE) || y == word(g, 0xA0F0)) c = 0x77;
        uint8_t a = peek(g, 0xA0E2);
        if (!(a & 0xF0)) a |= 0xF0;
        x = number(g, a, c, x);
        x = chr(g, 0x2B, c, x);
        x = chr(g, 0x0A, c, x);
        do { /* the initials */
            x = chr(g, game_cmos_byte(g, y), c, x);
            y = (uint16_t)(y + 2);
            poke(g, 0xA0E9, (uint8_t)(peek(g, 0xA0E9) - 1));
        } while (peek(g, 0xA0E9));
        game_set_word(g, 0xA0D5, x);
        x = (uint16_t)((uint8_t)((word(g, 0xA0D9) >> 8) + peek(g, 0xA0E7)) << 8 | peek(g, 0xA0D6));
        poke(g, 0xA0DD, 0);
        do { /* the score */
            a = game_cmos_byte(g, y);
            y = (uint16_t)(y + 2);
            if (!peek(g, 0xA0DD) && (peek(g, 0xA0E6) == 4 || !(a & 0xF0))) { /* leading zeros blank */
                a |= 0xF0;
                if (!(a & 0x0F)) a |= 0x0F;
            }
            poke(g, 0xA0DD, a);
            poke(g, 0xA0DD, (uint8_t)~peek(g, 0xA0DD));
            x = number(g, a, c, x);
            poke(g, 0xA0E6, (uint8_t)(peek(g, 0xA0E6) - 1));
        } while (peek(g, 0xA0E6));
        game_set_word(g, 0xA0D5, x);
        x = (uint16_t)((word(g, 0xA0D9) & 0xFF00) | (uint8_t)(peek(g, 0xA0D6) + peek(g, 0xA0E1)));
        {
            int cy, h;
            uint8_t r = game_add(peek(g, 0xA0E2), 1, 0, &cy, &h);
            poke(g, 0xA0E2, game_daa(r, &cy, h));
        }
        poke(g, 0xA0DC, (uint8_t)(peek(g, 0xA0DC) - 1));
    } while (peek(g, 0xA0DC));
    return y;
}

static void column(Game *g, uint16_t x, uint8_t n, uint8_t rank)
{
    poke(g, 0xA0DC, n);
    poke(g, 0xA0E2, rank);
    (void)x;
}

void hiscores_table(Game *g)
{
    hiscores_border(g);
    text_block(g, 0x0F, 0, 0);
    poke(g, 0xA0D5, 0);
    uint16_t x = 0x1170, y = 0xCD3E;
    uint8_t b = colour_of(g, y, 0x22);
    x = text_number_small(g, 0xF1, b, x);
    x = text_char_small(g, 0x2B, b, x);
    x = (uint16_t)(x + 0x300);
    poke(g, 0xA0DC, 0x15);
    for (;;) { /* the best one's name */
        poke(g, 0xA0DC, (uint8_t)(peek(g, 0xA0DC) - 1));
        if (!peek(g, 0xA0DC)) break;
        uint8_t a = game_cmos_byte(g, y);
        y = (uint16_t)(y + 2);
        if ((int8_t)a > 0x0A) poke(g, 0xA0D5, 0);
        else if (!peek(g, 0xA0D5)) game_set_word(g, 0xA0D5, x);
        x = text_char_small(g, a, b, x);
    }
    if (peek(g, 0xA0D5)) x = word(g, 0xA0D5);
    poke(g, 0xA0DC, 4);
    y = 0xCD6C;
    uint8_t a = game_cmos_byte(g, y);
    y = (uint16_t)(y + 2);
    a |= 0xF0;
    if (!(a & 0x0F)) a |= 0x0F;
    x = text_number_small(g, a, b, x);
    poke(g, 0xA0DC, (uint8_t)(peek(g, 0xA0DC) - 1));
    do {
        x = text_number_small(g, game_cmos_byte(g, y), b, x);
        y = (uint16_t)(y + 2);
        poke(g, 0xA0DC, (uint8_t)(peek(g, 0xA0DC) - 1));
    } while (peek(g, 0xA0DC));
    /* the table: three columns of 13 in the large font */
    game_set_word(g, 0xA0DF, 0x4A5F);
    poke(g, 0xA0DC, 0x0D);
    poke(g, 0xA0E2, 0x02);
    poke(g, 0xA0E1, 0x07);
    poke(g, 0xA0E7, 0x0E);
    y = rows(g, 0x1380, y, 0x33, text_number_large, text_char_large);
    column(g, 0, 0x0D, 0x15);
    y = rows(g, 0x3D80, y, 0x33, text_number_large, text_char_large);
    column(g, 0, 0x0D, 0x28);
    rows(g, 0x6780, y, 0x33, text_number_large, text_char_large);
    /* today's: two columns of 3 in the small */
    game_set_word(g, 0xA0DF, 0x4A56);
    poke(g, 0xA0DC, 0x03);
    poke(g, 0xA0E2, 0x01);
    poke(g, 0xA0E1, 0x0A);
    poke(g, 0xA0E7, 0x15);
    y = rows(g, 0x1336, 0xCFA4, 0x11, text_number_small, text_char_small);
    column(g, 0, 0x03, 0x04);
    rows(g, 0x5336, y, 0x11, text_number_small, text_char_small);
    task_create(g, game_word(g, TASK_CURRENT), 0xF012, 0x40, 0x00);
}

/* ---- the entry ------------------------------------------------------------ */

#define ENTRY_BEST 0x48E9  /* the best ever: a name of 20 */
#define ENTRY 0x4765       /* a place in the tables: initials */
#define ENTRY_ON 0x4A05    /* every 3 frames */
#define ENTRY_WAIT 0x47B2  /* player 1's waits for player 2's best */
#define ENTRY_SHOWN 0x486C /* after "too many" */
#define OVER_SCREEN 0x3B73
#define OVER_WAIT 0x3BAE
#define OVER_DONE 0x3BCD

int hiscores_ported(uint16_t at)
{
    return at == ENTRY_BEST || at == ENTRY || at == ENTRY_ON || at == ENTRY_WAIT || at == ENTRY_SHOWN ||
           at == OVER_SCREEN || at == OVER_WAIT || at == OVER_DONE;
}

static uint16_t F(uint16_t u, int off) { return (uint16_t)(u + off); }

/* $40FA: a byte to two CMOS nibbles at x */
static uint16_t put(Game *g, uint16_t x, uint8_t a)
{
    poke(g, F(x, 1), a);
    poke(g, x, (uint8_t)(a >> 4));
    return (uint16_t)(x + 2);
}

/* $3BCF: n bytes from x into the CMOS at y */
static uint16_t store(Game *g, uint16_t x, uint16_t y, uint8_t n)
{
    for (; n; n--) y = put(g, y, game_read(g, x++));
    return y;
}

/* $40B1: n raw bytes from x to y */
static void copy(Game *g, uint16_t x, uint16_t y, uint8_t n)
{
    for (; n; n--) poke(g, y++, game_read(g, x++));
}

/* $3F91 / $3F88: the best one's check nibble at $CD6C */
static void best_check(Game *g)
{
    uint8_t a = 0;
    for (uint16_t x = 0xCD3E; x != 0xCD74; x++)
        if (x != 0xCD6C) a = (uint8_t)(a + game_read(g, x));
    poke(g, 0xCD6C, a);
}

/* $3FAF / $3FA7: an entry's check nibble at +6 */
static void entry_check(Game *g, uint16_t y)
{
    uint8_t a = 0;
    for (int i = 0; i < 14; i++)
        if (i != 6) a = (uint8_t)(a + game_read(g, (uint16_t)(y + i)));
    poke(g, F(y, 6), a);
}

/* $4091: the entry at y deleted, those below moved up, a blank one last */
static void delete(Game *g, uint16_t y)
{
    for (uint16_t x = (uint16_t)(y + 0x0E); x < 0xCFA4; x = (uint16_t)(x + 0x0E), y = (uint16_t)(y + 0x0E))
        copy(g, x, y, 0x0E);
    store(g, 0x3F75, y, 7);
    entry_check(g, y);
}

/* $48D3: room made at y, the entries down to `end` moved down one */
static void make_room(Game *g, uint16_t end, uint16_t y)
{
    for (uint16_t at = end; at != y; at = (uint16_t)(at - 0x0E)) copy(g, (uint16_t)(at - 0x0E), at, 0x0E);
}

/* $4999: is the score at x (RAM bytes, or CMOS nibbles from $C000) above
   the one at y (CMOS: an entry's +6, its first nibble its check)? */
static int above(const Game *g, uint16_t x, uint16_t y)
{
    uint16_t t = y; /* (EXG: the table's entry is read through $49B8) */
    uint16_t p = x;
    for (int b = 4; b; b--) {
        uint8_t a;
        if (t >= 0xC000) {
            a = game_cmos_byte(g, t);
            t = (uint16_t)(t + 2);
        } else {
            a = game_read(g, t++);
        }
        if (b == 4) a &= 0x0F;
        uint8_t v = game_read(g, p++);
        if (a > v) return 0;
        if (a < v) return 1;
    }
    return 0;
}

/* $4963 (today's, from $CFA4) / $4980 (the table, from the best one): the
   place for the score at +$2D, *y the entry it takes */
static int place(const Game *g, uint16_t u, int today, uint16_t *y)
{
    uint16_t x = word(g, F(u, 0x2D));
    uint16_t at = today ? 0xCFAA : 0xCD6C, end = today ? 0xCFF8 : 0xCF96;
    for (; at < end; at = (uint16_t)(at + 0x0E))
        if (above(g, x, at)) {
            *y = (uint16_t)(at - 6);
            return 1;
        }
    return 0;
}

/* $489E: the entry at x has these initials (+$16) */
static int same_initials(const Game *g, uint16_t u, uint16_t x)
{
    uint16_t y = F(u, 0x16);
    for (int b = 3; b; b--) {
        if (game_cmos_byte(g, x) != game_read(g, y++)) return 0;
        x = (uint16_t)(x + 2);
    }
    return 1;
}

/* $486F / $4873: the a-th entry with these initials (counting from $CD74):
   1 and *x it, or 0 (*x $CF96) */
static int too_many(Game *g, uint16_t u, uint8_t a, uint16_t *x)
{
    poke(g, 0xA0D5, a);
    for (uint16_t at = 0xCD74; at < 0xCFA4; at = (uint16_t)(at + 0x0E))
        if (same_initials(g, u, at)) {
            poke(g, 0xA0D5, (uint8_t)(peek(g, 0xA0D5) - 1));
            if (!peek(g, 0xA0D5)) {
                *x = at;
                return 1;
            }
        }
    *x = 0xCF96;
    return 0;
}

/* $48B8: the initials and score into the entry at y, those down to x moved down */
static void insert(Game *g, uint16_t u, uint16_t y, uint16_t x)
{
    make_room(g, x, y);
    uint16_t at = store(g, F(u, 0x16), y, 3);
    store(g, word(g, F(u, 0x2D)), at, 4);
    entry_check(g, y);
}

static void mark(Game *g, uint16_t u, uint16_t p1, uint16_t p2, uint16_t y)
{
    game_set_word(g, peek(g, F(u, 0x31)) ? p2 : p1, y);
}

/* the input block at u + 7 */
#define LEFT 0    /* letters left */
#define INDEX 1   /* letters entered */
#define LOW 2     /* the letters' range */
#define HIGH 3
#define PIA 4     /* $C807 for this player's controls */
#define COLOUR 5
#define ROUTINE 6 /* $43F6 or $4438 */
#define START 8   /* the screen address */
#define CURSOR 0xA
#define DONE 0xC  /* where it carries on */
#define DELAY 0xE
#define BUFFER 0xF

/* $49D4: the input set up for b letters at x in colour a, then its first look */
static StepResult input(Game *g, uint16_t u, uint8_t a, uint8_t b, uint16_t x);

/* $44A6: the letter a into the buffer and drawn; returns B (the colour) */
static uint8_t letter(Game *g, uint16_t y, uint8_t a)
{
    poke(g, (uint16_t)(y + (int8_t)(uint8_t)(BUFFER + peek(g, F(y, INDEX)))), a);
    text_char_small(g, a, peek(g, F(y, COLOUR)), word(g, F(y, CURSOR)));
    return peek(g, F(y, COLOUR));
}

/* $43F6 / $4438: one look at the controls; *a and *b as the ROM leaves
   A and B, *x X; 1 done (the carry) */
static int look(Game *g, uint16_t y, uint8_t *a, uint8_t *b, uint16_t *x)
{
    uint8_t c;
    if (word(g, F(y, ROUTINE)) == 0x43F6) {
        uint8_t w = 0;
        for (uint8_t k = peek(g, F(y, INDEX)); k; k--) { /* the cursor after the letters so far */
            uint8_t ch = peek(g, (uint16_t)(y + (int8_t)(uint8_t)(k - 1 + BUFFER)));
            uint16_t rec = game_read_word(g, (uint16_t)(game_read_word(g, 0x4A68) + (int8_t)(uint8_t)(ch << 1)));
            w = (uint8_t)(w + game_read(g, rec));
        }
        game_set_word(g, F(y, CURSOR), (uint16_t)(word(g, F(y, START)) + (w << 8)));
        *a = peek(g, (uint16_t)(y + (int8_t)(uint8_t)(BUFFER + peek(g, F(y, INDEX)))));
        text_char_small(g, 0x33, peek(g, F(y, COLOUR)), word(g, F(y, CURSOR)));
        game_poke(g, 0xC807, peek(g, F(y, PIA)));
        c = game_read(g, 0xC804) & 7;
        if (c) {
            *b = c;
            *x = 0x4438;
            game_set_word(g, F(y, ROUTINE), 0x4438);
            return 0;
        }
        if (peek(g, F(y, DELAY))) poke(g, F(y, DELAY), (uint8_t)(peek(g, F(y, DELAY)) - 1));
        goto store;
    }
    /* $4438: held since the last look? */
    game_poke(g, 0xC807, peek(g, F(y, PIA)));
    c = *b;
    if (c != game_read(g, 0xC804)) goto store;
    if (c >> 1 == 0) { /* left: the letter before */
        if (peek(g, F(y, DELAY))) poke(g, F(y, DELAY), (uint8_t)(peek(g, F(y, DELAY)) - 1));
        text_char_small(g, *a, 0, word(g, F(y, CURSOR)));
        *a = *a == peek(g, F(y, LOW)) ? peek(g, F(y, HIGH)) : (uint8_t)(*a - 1);
        goto store;
    }
    if (c >> 2 == 0) { /* right: the letter after */
        if (peek(g, F(y, DELAY))) poke(g, F(y, DELAY), (uint8_t)(peek(g, F(y, DELAY)) - 1));
        text_char_small(g, *a, 0, word(g, F(y, CURSOR)));
        *a = *a == peek(g, F(y, HIGH)) ? peek(g, F(y, LOW)) : (uint8_t)(*a + 1);
        goto store;
    }
    if (peek(g, F(y, DELAY))) { /* flap, held */
        poke(g, F(y, DELAY), (uint8_t)(peek(g, F(y, DELAY)) - 1));
        goto store;
    }
    poke(g, F(y, DELAY), 2);
    *b = 2;
    if (*a == 0x25) { /* the rub-out: back a letter */
        if (!peek(g, F(y, INDEX))) goto store;
        text_char_small(g, *a, 0, word(g, F(y, CURSOR)));
        *b = 0; /* ($44B4's LDB #0) */
        poke(g, F(y, LEFT), (uint8_t)(peek(g, F(y, LEFT)) + 1));
        poke(g, F(y, INDEX), (uint8_t)(peek(g, F(y, INDEX)) - 1));
        text_char_small(g, 0x33, 0, word(g, F(y, CURSOR)));
        *x = 0x43F6;
        game_set_word(g, F(y, ROUTINE), 0x43F6);
        return 0;
    }
    poke(g, F(y, INDEX), (uint8_t)(peek(g, F(y, INDEX)) + 1));
    text_char_small(g, 0x33, 0, word(g, F(y, CURSOR)));
    poke(g, F(y, LEFT), (uint8_t)(peek(g, F(y, LEFT)) - 1));
    if (!peek(g, F(y, LEFT))) {
        *x = 0x43F6;
        game_set_word(g, F(y, ROUTINE), 0x43F6);
        return 1;
    }
store:
    *b = letter(g, y, *a);
    *x = 0x43F6;
    game_set_word(g, F(y, ROUTINE), 0x43F6);
    return 0;
}

static StepResult carry_on(Game *g, uint16_t u, uint8_t a, uint8_t b, uint16_t x);
static StepResult initials(Game *g, uint16_t u);

/* $49F4: a look, then a sleep of 3 frames (A, B in +$32, Y in +$36) */
static StepResult entry_look(Game *g, uint16_t u, uint8_t a, uint8_t b)
{
    uint16_t y = F(u, 7), x = 0;
    if (look(g, y, &a, &b, &x)) return carry_on(g, u, a, b, x);
    game_set_word(g, F(u, 0x36), y);
    poke(g, F(u, 0x32), a);
    poke(g, F(u, 0x33), b);
    return task_sleep(g, 3, ENTRY_ON);
}

static StepResult input(Game *g, uint16_t u, uint8_t a, uint8_t b, uint16_t x)
{
    uint16_t y = F(u, 7);
    poke(g, F(y, COLOUR), a);
    poke(g, F(y, LEFT), b);
    game_set_word(g, F(y, START), x);
    for (int i = 0; i < 0x14; i++) poke(g, (uint16_t)(y + BUFFER + i), 0x0A);
    game_set_word(g, F(y, ROUTINE), 0x43F6);
    poke(g, F(y, INDEX), 0);
    poke(g, F(y, LOW), 0x0A);
    poke(g, F(y, HIGH), 0x25);
    return entry_look(g, u, 0x25, 0x0A);
}

/* $4854: "too many", 96 frames */
static StepResult too_many_shown(Game *g, uint16_t u)
{
    text_message_small(g, 0x6A, peek(g, F(u, 7 + COLOUR)), (uint16_t)(word(g, F(u, 0x2F)) - 0x150A));
    return task_sleep(g, 0x60, ENTRY_SHOWN);
}

/* $47E6: the best one's initials at $CD66, then the limit of 5 */
static StepResult best_initials(Game *g, uint16_t u)
{
    store(g, F(u, 0x16), 0xCD66, 3);
    best_check(g);
    uint16_t x;
    if (!too_many(g, u, 5, &x)) return task_end(g);
    delete(g, x);
    return too_many_shown(g, u);
}

/* $47C4: entered: into today's table and the table */
static StepResult entered(Game *g, uint16_t u)
{
    uint16_t y, x;
    if (place(g, u, 1, &y)) {
        mark(g, u, 0xA0EC, 0xA0F0, y);
        insert(g, u, y, 0xCFEA);
    }
    if (!place(g, u, 0, &y)) return task_end(g);
    if (peek(g, F(u, 0x2C))) return best_initials(g, u);
    if (y == 0xCD66) { /* $4809: the best one, by initials only */
        make_room(g, 0xCF96, y);
        store(g, word(g, F(u, 0x2D)), (uint16_t)(y + 6), 4);
        mark(g, u, 0xA0EA, 0xA0EE, 0xCD3E);
        store(g, F(u, 0x16), 0xCD3E, 0x14);
        return best_initials(g, u);
    }
    /* $4835 */
    uint8_t a = same_initials(g, u, 0xCD66) ? 4 : 5;
    int many = too_many(g, u, a, &x);
    if (y <= x) {
        insert(g, u, y, x);
        mark(g, u, 0xA0EA, 0xA0EE, y);
    }
    if (!many) return task_end(g);
    return too_many_shown(g, u);
}

/* $479D: the initials done ([+$C]: $479D, or $4915 for the best one's name) */
static StepResult carry_on(Game *g, uint16_t u, uint8_t a, uint8_t b, uint16_t x)
{
    uint16_t y = F(u, 7);
    if (word(g, F(y, DONE)) == 0x4915) { /* the name: the best one moves down */
        store(g, F(y, BUFFER), 0xCD3E, 0x14);
        make_room(g, 0xCF96, 0xCD66);
        entry_check(g, 0xCD74);
        store(g, word(g, F(u, 0x2D)), 0xCD6C, 4);
        store(g, 0x3F75, 0xCD66, 3);
        best_check(g);
        mark(g, u, 0xA0EA, 0xA0EE, 0xCD3E);
        return initials(g, u);
    }
    (void)a;
    a = peek(g, F(u, 2)); /* (LDA 2,U: the type in A) */
    if (a == 0x42) return entered(g, u);
    poke(g, F(u, 0x32), a);
    poke(g, F(u, 0x33), b);
    game_set_word(g, F(u, 0x34), x);
    game_set_word(g, F(u, 0x36), y);
    return task_sleep(g, 1, ENTRY_WAIT);
}

/* $4768: "enter your initials" and 3 letters (the player by +$31) */
static StepResult initials(Game *g, uint16_t u)
{
    int p2 = peek(g, F(u, 0x31)) != 0;
    uint8_t colour = p2 ? 0x77 : 0x55;
    text_message_small(g, 0x68, colour, p2 ? 0x58B0 : 0x0AB0);
    uint16_t y = F(u, 7);
    poke(g, F(y, DELAY), 2);
    poke(g, F(y, PIA), p2 ? 0x34 : 0x3C);
    game_set_word(g, F(y, DONE), 0x479D);
    return input(g, u, colour, 3, word(g, F(u, 0x2F)));
}

/* $4715: a place in today's table or the table: an entry task for the
   player a (type b) */
static void entry_for(Game *g, uint16_t u, uint8_t a, uint8_t b)
{
    uint16_t y;
    uint8_t sleep;
    if (place(g, u, 1, &y)) sleep = 0x10;
    else if (place(g, u, 0, &y)) sleep = 0x08;
    else return;
    poke(g, 0xA0F5, (uint8_t)(peek(g, 0xA0F5) + 1));
    y = task_create(g, u, ENTRY, b, 0x00);
    poke(g, F(y, 4), sleep);
    poke(g, F(y, 0x31), a);
    game_set_word(g, F(y, 0x2D), word(g, F(u, 0x2D)));
    game_set_word(g, F(y, 0x2F), word(g, F(u, 0x2F)));
}

/* the best one's entry task for the player a */
static void best_for(Game *g, uint16_t u, uint8_t a, uint16_t score, uint16_t at)
{
    poke(g, 0xA0F5, (uint8_t)(peek(g, 0xA0F5) + 1));
    uint16_t y = task_create(g, u, ENTRY_BEST, 0x42, 0x00);
    poke(g, F(y, 4), 0x10);
    poke(g, F(y, 0x31), a);
    game_set_word(g, F(y, 0x2D), score);
    game_set_word(g, F(y, 0x2F), at);
}

static void aim(Game *g, uint16_t u, uint16_t score, uint16_t at)
{
    game_set_word(g, F(u, 0x2D), score);
    game_set_word(g, F(u, 0x2F), at);
}

/* $4641: a score past 9999999 shown as 99999999 */
static void cap(Game *g, uint16_t x)
{
    if (game_peek(g, x) & 0xF0) {
        game_set_word(g, x, 0x9999);
        game_set_word(g, F(x, 2), 0x9999);
    }
}

/* $3B64: wait for the entries, then the attract mode */
static StepResult over(Game *g, uint16_t u)
{
    if (!peek(g, 0xA0F5)) {
        task_kill_type(g, 0x42, 0xFF);
        return task_sleep(g, 3, OVER_DONE);
    }
    task_kill_type(g, 0x00, 0x40);
    return task_sleep(g, 4, OVER_SCREEN);
}

StepResult hiscores_game_over(Game *g, uint16_t u)
{
    poke(g, 0xA0F5, 0);
    if (!(game_read(g, 0xCC05) & 1)) return start_step(g, u, START_ATTRACT);
    cap(g, 0xA04C);
    cap(g, 0xA056);
    if (peek(g, 0xA060) && !above(g, 0xA04C, 0xA056)) { /* player 2's the higher */
        if (above(g, 0xA056, 0xCD6C)) {
            best_for(g, u, 1, 0xA056, 0x70A0);
        } else {
            aim(g, u, 0xA056, 0x70A0);
            entry_for(g, u, 1, 0x42);
        }
        aim(g, u, 0xA04C, 0x20A0);
        entry_for(g, u, 0, 0x41);
        return over(g, u);
    }
    if (above(g, 0xA04C, 0xCD6C)) {
        best_for(g, u, 0, 0xA04C, 0x20A0);
    } else {
        aim(g, u, 0xA04C, 0x20A0);
        entry_for(g, u, 0, 0x42);
    }
    if (peek(g, 0xA060)) {
        aim(g, u, 0xA056, 0x70A0);
        entry_for(g, u, 1, 0x41);
    }
    return over(g, u);
}

StepResult hiscores_step(Game *g, uint16_t u, uint16_t at)
{
    switch (at) {
    case ENTRY:
        poke(g, F(u, 0x2C), 0);
        return initials(g, u);
    case ENTRY_BEST: { /* $48E9: the best one's name */
        poke(g, F(u, 0x2C), (uint8_t)(peek(g, F(u, 0x2C)) + 1));
        uint16_t y = F(u, 7);
        int p2 = peek(g, F(u, 0x31)) != 0;
        poke(g, F(y, PIA), p2 ? 0x34 : 0x3C);
        uint8_t colour = p2 ? 0x77 : 0x55;
        text_message_small(g, 0x67, colour, 0x295B);
        game_set_word(g, F(y, DONE), 0x4915);
        uint8_t b = game_cmos_byte(g, 0xCC16), n = 0; /* $437B: its length, BCD */
        while (b >= 0x10) {
            n = (uint8_t)(n + 0x0A);
            b = (uint8_t)(b - 0x10);
        }
        return input(g, u, colour, (uint8_t)(n + b), 0x2643);
    }
    case ENTRY_ON: return entry_look(g, u, peek(g, F(u, 0x32)), peek(g, F(u, 0x33)));
    case ENTRY_WAIT:
        if (game_read(g, F(word(g, u), 2)) == 0x42) return task_sleep(g, 1, ENTRY_WAIT);
        return entered(g, u);
    case ENTRY_SHOWN: return task_end(g);
    case OVER_SCREEN:
        game_blit(g, 0x12, 0x00, 0, 0x0000, 0x9C, 0xD7);
        text_message_small(g, 0x69, 0x11, 0x3920);
        text_message(g, 0x6B, 0x99, 0x14CA);
        game_sound(g, game_read_word(g, 0x5EDB));
        poke(g, F(u, 0x32), 0);
        return task_sleep(g, 0x1E, OVER_WAIT);
    case OVER_WAIT: {
        poke(g, F(u, 0x32), (uint8_t)(peek(g, F(u, 0x32)) - 1));
        if (!peek(g, F(u, 0x32))) {
            task_kill_type(g, 0x42, 0xFF);
            return task_sleep(g, 3, OVER_DONE);
        }
        uint8_t t = game_read(g, F(word(g, u), 2));
        if (t == 0x41 || t == 0x42) return task_sleep(g, 0x1E, OVER_WAIT);
        return start_step(g, u, START_ATTRACT);
    }
    case OVER_DONE: return start_step(g, u, START_ATTRACT);
    }
    return STEP_UNPORTED;
}
