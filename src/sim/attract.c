#include "sim/attract.h"

#include "sim/start.h"
#include "sim/tasks.h"
#include "sim/text.h"

#define TITLE 0xD011
#define TITLE_LOGO 0xD05E
#define TITLE_WAIT 0xD0C6
#define SWEEP 0xD0ED
#define SWEEP_ON 0xD0FD
#define LIGHTS 0xD15B
#define LIGHTS_ON 0xD1E7

int attract_ported(uint16_t at)
{
    return at == TITLE || at == TITLE_LOGO || at == TITLE_WAIT || at == SWEEP || at == SWEEP_ON || at == LIGHTS ||
           at == LIGHTS_ON;
}

static uint16_t F(uint16_t u, int off) { return (uint16_t)(u + off); }
static uint8_t peek(const Game *g, uint16_t a) { return game_peek(g, a); }
static void poke(Game *g, uint16_t a, uint8_t v) { game_poke(g, a, v); }
static uint16_t word(const Game *g, uint16_t a) { return game_word(g, a); }
static void set_word(Game *g, uint16_t a, uint16_t v) { game_set_word(g, a, v); }
static uint8_t rom(const Game *g, uint16_t a) { return game_read(g, a); }

/* `CLR $BFFF; CLR $C900` / `LDA #1; STA $BFFF; STA $C900` */
static void rom_off(Game *g) { game_select_rom(g, 0); }
static void rom_on(Game *g) { game_select_rom(g, 1); }

/* $44E0 / $4500: an operator's line from the CMOS (25 characters from x,
   its screen address's column at `at`), in colour <$E9 */
static void operator_line(Game *g, uint16_t at, uint8_t line, uint16_t x, uint16_t end)
{
    uint16_t y = (uint16_t)(game_cmos_byte(g, at) << 8 | line);
    uint8_t colour = peek(g, 0xA0E9);
    for (; x != end; x = (uint16_t)(x + 2)) y = text_char_small(g, game_cmos_byte(g, x), colour, y);
}

/* ---- the logo's lines ($D2A8, scratch at $BC00-$BC1F) ------------------------ */

#define X0 0xBC00
#define Y0 0xBC02
#define X1 0xBC04
#define Y1 0xBC06
#define COLOURS 0xBC08
#define FILL 0xBC0A
#define COUNT_HI 0xBC0B
#define COUNT 0xBC0C
#define ERR 0xBC10
#define DX 0xBC12
#define DY 0xBC14
#define SIGNS 0xBC16
#define MASKS 0xBC1A
#define OFFSET 0xBC1E

/* $D254: the pixels below filled in the second colour down to one already set */
static void fill_down(Game *g, uint16_t x)
{
    x = (uint16_t)(x + 1);
    for (;;) {
        uint16_t m = word(g, MASKS);
        uint8_t a = (uint8_t)((m >> 8) & peek(g, F(COLOURS, 1)));
        if ((m >> 8) & game_read(g, x)) return;
        poke(g, x, (uint8_t)(((uint8_t)m & game_read(g, x)) | a));
        x = (uint16_t)((x & 0xFF00) | (uint8_t)(x + 1));
        if (!(uint8_t)x) return;
    }
}

/* $D2A8: a line from (x0, y0) to (x1, y1), pixel by pixel */
static void line(Game *g)
{
    poke(g, SIGNS, 0);
    uint16_t x0 = word(g, X0), d = (uint16_t)(x0 - word(g, X1));
    int borrow = x0 < word(g, X1);
    poke(g, SIGNS, (uint8_t)(borrow << 7 | peek(g, SIGNS) >> 1));
    if (borrow) d = (uint16_t)-d;
    set_word(g, DX, d);
    set_word(g, ERR, d);
    uint16_t y0 = word(g, Y0);
    d = (uint16_t)(y0 - word(g, Y1));
    borrow = y0 < word(g, Y1);
    poke(g, SIGNS, (uint8_t)(borrow << 7 | peek(g, SIGNS) >> 1));
    if (!borrow) d = (uint16_t)-d;
    d = (uint16_t)(d - 1);
    set_word(g, DY, d);
    poke(g, COUNT_HI, 0);
    unsigned n = (uint8_t)~d + (unsigned)word(g, DX);
    if (n > 0xFFFF) poke(g, COUNT_HI, (uint8_t)(peek(g, COUNT_HI) + 1));
    set_word(g, COUNT, (uint16_t)n);
    unsigned sum = (unsigned)word(g, ERR) + word(g, DY);
    for (;;) {
        set_word(g, ERR, (uint16_t)sum);
        int carry = sum > 0xFFFF;
        rom_off(g);
        uint16_t x = word(g, X0);
        uint8_t m = x & 1 ? 0x0F : 0xF0;
        set_word(g, MASKS, (uint16_t)(m << 8 | (uint8_t)~m));
        uint16_t at = (uint16_t)(((x >> 1) & 0xFF) << 8 | peek(g, F(Y0, 1)));
        uint8_t a = (uint8_t)(m & peek(g, COLOURS));
        uint8_t b = (uint8_t)((uint8_t)~m & game_read(g, at));
        if (at >= 0x9800) return; /* (the ROM's stack goes wrong here; no logo line gets there) */
        poke(g, at, (uint8_t)(a | b));
        if (peek(g, FILL) & 0x80) fill_down(g, at);
        rom_on(g);
        uint16_t c = (uint16_t)(word(g, COUNT) - 1);
        if (!c) {
            if (!peek(g, COUNT_HI)) return;
            poke(g, COUNT_HI, (uint8_t)(peek(g, COUNT_HI) - 1));
        }
        set_word(g, COUNT, c);
        if (carry) { /* $D278: x a step */
            set_word(g, X0, (uint16_t)(word(g, X0) + (peek(g, SIGNS) & 0x40 ? 1 : -1)));
            sum = (unsigned)word(g, ERR) + word(g, DY);
        } else { /* $D291: y a step */
            set_word(g, Y0, (uint16_t)(word(g, Y0) + (peek(g, SIGNS) & 0x80 ? 1 : -1)));
            sum = (unsigned)word(g, ERR) + word(g, DX);
        }
    }
}

/* $D070: the logo's polylines from $D35D */
static void logo(Game *g)
{
    uint16_t u = 0xD35D;
    poke(g, OFFSET, 0);
    poke(g, F(OFFSET, 1), rom(g, u++));
    for (;;) { /* $D07B: a polyline: its fill flag, its colours, its points */
        poke(g, FILL, rom(g, u++));
        set_word(g, COLOURS, game_read_word(g, u));
        u = (uint16_t)(u + 2);
        for (;;) {
            set_word(g, X0, (uint16_t)(rom(g, u++) + word(g, OFFSET)));
            set_word(g, Y0, rom(g, u++));
            if (!rom(g, u)) break;
            set_word(g, X1, (uint16_t)(rom(g, u) + word(g, OFFSET)));
            set_word(g, Y1, rom(g, F(u, 1)));
            line(g);
        }
        u++;
        if (rom(g, u++)) continue;
        uint8_t a = rom(g, u++);
        if (!a) return;
        poke(g, F(OFFSET, 1), a);
    }
}

/* $D1F2: a border light's pixel in colour a at (x, y), unless one is there */
static void light(Game *g, uint16_t x, uint16_t y, uint8_t a)
{
    game_select_rom(g, 0);
    uint16_t at = (uint16_t)(((x >> 1) & 0xFF) << 8 | (y & 0xFF));
    uint8_t mask = 0x0F, b = a;
    if (!(x & 1)) {
        b = (uint8_t)(b << 4);
        mask = 0xF0;
    }
    if (!(mask & game_read(g, at))) poke(g, at, (uint8_t)(b | game_read(g, at)));
    rom_on(g);
}

/* $D19B (a column, down) / $D1B8 (a row, across): 16 lights from (x, y),
   the colour a step on (both kept, with x and y, in +$D-+$12); then
   +$13 = where the ROM returns, and every third a frame's sleep. 1 it
   slept. */
static int lights_16(Game *g, uint16_t u, uint16_t x, uint16_t y, uint8_t *pa, uint8_t b, int across, uint16_t back,
                     StepResult *r)
{
    uint8_t a = (uint8_t)(((*pa + 1) & 0x0F) | 0x08);
    *pa = a;
    set_word(g, F(u, 0xD), x);
    set_word(g, F(u, 0xF), y);
    poke(g, F(u, 0x11), a);
    poke(g, F(u, 0x12), b);
    for (int i = 0; i < 16; i++) {
        light(g, x, y, a);
        if (across) x++;
        else y++;
        a = (uint8_t)(((a + 1) & 0x0F) | 0x08);
    }
    set_word(g, F(u, 0x13), back);
    uint8_t n = peek(g, F(u, 0x15));
    poke(g, F(u, 0x15), (uint8_t)(n - 1));
    if ((int8_t)n > 1) return 0;
    poke(g, F(u, 0x15), 3);
    *r = task_sleep(g, 1, LIGHTS_ON);
    return 1;
}

/* $D15B-$D198: lights round the border, from where +$13 says */
static StepResult lights_run(Game *g, uint16_t u, uint16_t at, uint16_t x, uint16_t y, uint8_t a, uint8_t b)
{
    StepResult r = STEP_UNPORTED;
    for (int guard = 0; guard < 4096; guard++) {
        switch (at) {
        case 0xD167:
            x++;
            if (lights_16(g, u, x, y, &a, b, 0, 0xD16B, &r)) return r;
            /* fall through */
        case 0xD16B:
            if (x < 0x012E) { at = 0xD167; break; }
            x = (uint16_t)(x - 0x10);
            y = (uint16_t)(y + 0x0F);
            /* fall through */
        case 0xD174:
            y++;
            if (lights_16(g, u, x, y, &a, b, 1, 0xD178, &r)) return r;
            /* fall through */
        case 0xD178:
            if (y < 0x00F2) { at = 0xD174; break; }
            y = (uint16_t)(y - 0x10);
            /* fall through */
        case 0xD180:
            x--;
            if (lights_16(g, u, x, y, &a, b, 0, 0xD184, &r)) return r;
            /* fall through */
        case 0xD184:
            if (x > 0) { at = 0xD180; break; }
            /* fall through */
        case 0xD18E:
            y--;
            if (lights_16(g, u, x, y, &a, b, 1, 0xD192, &r)) return r;
            /* fall through */
        case 0xD192:
            if (y > 0x0010) { at = 0xD18E; break; }
            return task_end(g);
        default:
            return STEP_UNPORTED;
        }
    }
    return STEP_UNPORTED;
}

StepResult attract_step(Game *g, uint16_t u, uint16_t at)
{
    switch (at) {
    case ATTRACT_PAGES: /* $D000 */
        task_kill_type(g, 0x00, 0x08);
        start_palette(g, game_read_word(g, 0x5EE9));
        return task_sleep(g, 2, TITLE);
    case TITLE: {
        start_clear_screen(g);
        start_palette(g, 0xD224);
        poke(g, 0xA0E9, 0x66); /* $44D4: the operator's two lines */
        operator_line(g, 0xCC88, 0x88, 0xCC24, 0xCC56);
        operator_line(g, 0xCC8A, 0x9A, 0xCC56, 0xCC88);
        text_message_small(g, 0x6C, 0x55, 0x1CBD);
        start_credits(g, 0x12, 0x11);
        uint8_t a = game_cmos_byte(g, 0xCC00);
        if (a) { /* the extra life interval */
            uint16_t x = text_message_small(g, 0xF7, 0x33, 0x1FAB);
            a = game_cmos_byte(g, 0xCC00);
            if (!(a & 0xF0)) a |= 0xF0;
            x = text_number_small(g, a, 0xCC, x);
            text_message_small(g, 0xF8, 0xCC, x);
        }
        return task_sleep(g, 1, TITLE_LOGO);
    }
    case TITLE_LOGO:
        task_create(g, u, LIGHTS, 0x11, 0xFF);
        task_create(g, u, SWEEP, 0x10, 0xFF);
        logo(g);
        poke(g, F(u, 0xD), 0x6F);
        return task_sleep(g, 0x0A, TITLE_WAIT);
    case TITLE_WAIT: {
        uint8_t n = (uint8_t)(peek(g, F(u, 0xD)) - 1);
        poke(g, F(u, 0xD), n);
        if (n) return task_sleep(g, 0x0A, TITLE_WAIT);
        task_kill_type(g, 0x00, 0x40);
        set_word(g, 0xA010, 0xFFFF);
        for (uint16_t y = 0xA012; y < 0xA01E; y += 2) set_word(g, y, 0);
        poke(g, 0xA01E, 0);
        return start_step(g, u, START_DEMO);
    }
    case SWEEP:
        set_word(g, F(u, 0xD), 0xFFFF);
        set_word(g, F(u, 0x10), 0xD22C);
        return task_sleep(g, 2, SWEEP_ON);
    case SWEEP_ON: { /* the colours <$17-<$1E sweep along, a new one in */
        uint16_t x = word(g, 0xA01E);
        uint16_t d = word(g, F(u, 0xD));
        if (d & 0x8000) {
            if (game_read(g, F(word(g, u), 2)) == 0x11) goto sweep; /* while the border lights go round */
            d = 0x0010;
        }
        if (d) {
            d = (uint16_t)(d - 1);
            set_word(g, F(u, 0xD), d);
            if (d) goto sweep;
            uint16_t y = (uint16_t)(word(g, F(u, 0x10)) + 8);
            if (y >= 0xD254) y = 0xD22C;
            set_word(g, F(u, 0x10), y);
            set_word(g, F(u, 0x12), y);
            poke(g, F(u, 0xF), 8);
        }
        {
            uint16_t y = word(g, F(u, 0x12));
            x = game_read_word(g, y);
            set_word(g, F(u, 0x12), (uint16_t)(y + 1));
            uint8_t n = (uint8_t)(peek(g, F(u, 0xF)) - 1);
            poke(g, F(u, 0xF), n);
            if (!n) set_word(g, F(u, 0xD), 0x0057);
        }
    sweep:
        set_word(g, 0xA01D, word(g, 0xA01C));
        set_word(g, 0xA01B, word(g, 0xA01A));
        set_word(g, 0xA019, word(g, 0xA018));
        set_word(g, 0xA017, (uint16_t)((x & 0xFF00) | peek(g, 0xA017)));
        return task_sleep(g, 2, SWEEP_ON);
    }
    case LIGHTS:
        poke(g, F(u, 0x15), 0);
        return lights_run(g, u, 0xD167, 0x000F, 0x0010, 0x08, g->b); /* (B: as the step before left it) */
    case LIGHTS_ON:
        return lights_run(g, u, word(g, F(u, 0x13)), word(g, F(u, 0xD)), word(g, F(u, 0xF)), peek(g, F(u, 0x11)),
                          peek(g, F(u, 0x12)));
    }
    return STEP_UNPORTED;
}
