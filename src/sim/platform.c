#include "sim/platform.h"

#include "sim/collide.h"
#include "sim/troll.h"

#define DX 0xA034   /* <$34 */
#define MET 0xA04B  /* <$4B */
#define SOUND 0xEF8F

static uint16_t field(uint16_t u, int off) { return (uint16_t)(u + off); }

/* $DBF9: the rider's collision data against the platform's at *py */
static int against(Game *g, uint16_t u, uint16_t d, uint16_t from, uint16_t *px, uint16_t *py)
{
    game_set_word(g, DX, (uint16_t)(d - game_word(g, field(u, 7))));
    uint8_t a = (uint8_t)(from >> 8), b = (uint8_t)(from - game_peek(g, field(u, 0x28)));
    uint16_t x = game_read_word(g, game_word(g, field(u, 0x1C))), y = *py;
    int below = b & 0x80;
    if (below) b = (uint8_t)-b;
    int c = (b >> 6) & 1; /* ASLB; ASLB; ROLA: the second shift's carry */
    b = (uint8_t)(b << 2);
    a = (uint8_t)(a << 1 | c);
    uint16_t off = (uint16_t)(a << 8 | b);
    if (below) y = (uint16_t)(y + off);
    else x = (uint16_t)(x + off);
    *px = x;
    *py = y;
    return collide_rows_at(g, px, py, game_word(g, DX));
}

/* the overlap's sign: the rows' widths and twice <$34 */
static uint16_t side(const Game *g, uint16_t x, uint16_t y)
{
    return (uint16_t)(game_read_word(g, y) + game_read_word(g, (uint16_t)(y + 2)) - game_read_word(g, x) -
                      game_read_word(g, (uint16_t)(x + 2)) + 2 * game_word(g, DX));
}

/* the first row hit with any of the next four rows' left edges negative */
static int edge(const Game *g, uint16_t x)
{
    return (game_read(g, (uint16_t)(x + 4)) & 0x80) || (game_read(g, (uint16_t)(x + 8)) & 0x80) ||
           (game_read(g, (uint16_t)(x + 0xC)) & 0x80) || (game_read(g, (uint16_t)(x + 0x10)) & 0x80);
}

/* $DB85: from below: the velocity reversed and halved if it was rising */
static void from_below(Game *g, uint16_t u)
{
    uint16_t v = game_word(g, field(u, 0x11));
    if (v & 0x8000) game_set_word(g, field(u, 0x11), (uint16_t)((int16_t)-v >> 1));
    game_poke(g, field(u, 0x23), 2);
    game_sound(g, SOUND);
}

/* $DBA2: pushed sideways by `push`, at most $FFC0 up */
static void pushed(Game *g, uint16_t u, uint8_t push)
{
    game_poke(g, field(u, 0x22), push);
    uint16_t v = game_word(g, field(u, 0x11));
    if ((uint16_t)(v - 0xFFC0) & 0x8000) v = 0xFFC0; /* CMPD #$FFC0; BPL */
    game_set_word(g, field(u, 0x11), v);
    game_poke(g, field(u, 0x23), 2);
    game_sound(g, SOUND);
}

/* $DB76: off the platform's left side */
static void off_left(Game *g, uint16_t u)
{
    uint8_t a = game_peek(g, field(u, 0x14));
    if (a & 0x80) {
        a = (uint8_t)(-a - 2);
        game_poke(g, field(u, 0x14), a);
    }
    pushed(g, u, (uint8_t)(a + 6));
}

/* $DB95: off its right side */
static void off_right(Game *g, uint16_t u)
{
    uint8_t a = game_peek(g, field(u, 0x14));
    if ((int8_t)a > 0) {
        a = (uint8_t)(-a + 2);
        game_poke(g, field(u, 0x14), a);
    }
    pushed(g, u, (uint8_t)(a - 6));
}

/* $DBBF: on its top edge from above */
static void from_above(Game *g, uint16_t u, uint16_t x, uint16_t y)
{
    uint16_t v = game_word(g, field(u, 0x11));
    if (!(v & 0x8000)) game_set_word(g, field(u, 0x11), (uint16_t)((int16_t)(uint16_t)~v >> 1));
    game_poke(g, field(u, 0x23), 0xFE);
    if (!game_peek(g, field(u, 0x14))) game_poke(g, field(u, 0x14), side(g, x, y) & 0x8000 ? 2 : 0xFE);
    game_sound(g, SOUND);
}

/* $DA7E, $DAE8 and $DB47: the three ways a hit is judged */
enum { FORM_A, FORM_B, FORM_C };

static void judge(Game *g, uint16_t u, int form, uint16_t data, uint16_t d, uint16_t from)
{
    uint16_t x, y = data;
    if (!against(g, u, d, from, &x, &y)) return;
    if (y == data && edge(g, x)) {
        from_above(g, u, x, y);
        return;
    }
    if (game_read(g, (uint16_t)(y + 4)) & 0x80) {
        from_below(g, u);
        return;
    }
    int left = side(g, x, y) & 0x8000;
    if (form == FORM_A) {
        if (left) off_left(g, u);
        else from_below(g, u);
    } else if (form == FORM_B) {
        if (left) from_below(g, u);
        else off_right(g, u);
    } else {
        if (left) off_left(g, u);
        else off_right(g, u);
    }
}

static uint16_t data_of(const Game *g, uint16_t n) { return game_read_word(g, game_read_word(g, n)); }

void platform_hit(Game *g, uint16_t u, uint8_t bits)
{
    game_poke(g, MET, bits | game_peek(g, MET));
    if (bits & 0x80) judge(g, u, FORM_C, data_of(g, 0x0E), 0x0036, 0x00D3);
    else if (bits < 8) {
        if (bits == 2) judge(g, u, FORM_B, data_of(g, 0x02), 0x00FC, 0x0045);
        else if (bits > 2) judge(g, u, FORM_C, data_of(g, 0x04), 0x0056, 0x0051);
        else judge(g, u, FORM_A, data_of(g, 0x00), 0xFFE0, 0x0045);
    } else if (bits == 8) {
        judge(g, u, FORM_B, data_of(g, 0x0A), 0x00CA, 0x0081);
    } else if ((bits & 0x70) > 0x20) {
        judge(g, u, FORM_C, data_of(g, 0x0C), 0x006A, 0x00A3);
    } else if ((bits & 0x70) < 0x20) {
        judge(g, u, FORM_A, data_of(g, 0x08), 0xFFE0, 0x008A);
    } else { /* $DAC5: the middle platform's two sides */
        uint16_t x, data = data_of(g, 0x0A), y = data;
        if (!against(g, u, 0x00CA, 0x0081, &x, &y)) return;
        if (game_peek(g, field(u, 0xB)) <= 0x93 && edge(g, x)) {
            from_above(g, u, x, y);
            return;
        }
        if (game_read(g, (uint16_t)(y + 4)) & 0x80 || (side(g, x, y) & 0x8000)) from_below(g, u);
        else off_right(g, u);
    }
}

static int land(Game *g, uint16_t u, uint8_t line)
{
    game_poke(g, field(u, 0xB), line);
    return 0;
}

int platform_class(Game *g, uint16_t u, uint8_t top)
{
    {
        if (top < 8) return land(g, u, top == 2 ? 0x50 : top > 2 ? 0x80 : 0x44);
        if (top == 8) return land(g, u, 0x89);
        if (top < 0x20) return land(g, u, 0xA2);
        if (top & 0x20) return land(g, u, 0xD2);
        /* $D9FC: the lava's line */
        if (!game_peek(g, 0xA0A0) && !game_peek(g, 0xA096)) {
            uint8_t type = game_peek(g, field(u, 2));
            if (type == 0x89 || type == 0x8F) troll_hand(g, u);
        }
        return game_peek(g, field(u, 0xB)) ? 1 : 0;
    }
}

int platform_below(Game *g, uint16_t u)
{
    uint16_t x = game_word(g, field(u, 7)), y = game_word(g, field(u, 0xA));
    uint8_t top = game_read(g, (uint16_t)(x - 0x4FFE)) & game_read(g, (uint16_t)(y - 0x1569));
    if (top) return platform_class(g, u, top);
    uint8_t beside = game_read(g, (uint16_t)(x - 0x515E)) & game_read(g, (uint16_t)(y - 0x17C9));
    if (beside) platform_hit(g, u, beside);
    return 1;
}
