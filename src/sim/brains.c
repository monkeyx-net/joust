#include "sim/brains.h"

#define CONTROLS 0xA032

static uint16_t F(uint16_t u, int off) { return (uint16_t)(u + off); }
static uint8_t peek(const Game *g, uint16_t a) { return game_peek(g, a); }
static uint16_t word(const Game *g, uint16_t a) { return game_word(g, a); }
static uint16_t ram_word(const Game *g, uint16_t a) { return game_read_word(g, a); }

/* DEC n,U and the signed result */
static int8_t dec(Game *g, uint16_t a)
{
    uint8_t v = (uint8_t)(peek(g, a) - 1);
    game_poke(g, a, v);
    return (int8_t)v;
}

static void controls(Game *g, uint8_t a, uint8_t b)
{
    game_poke(g, CONTROLS, a);
    game_poke(g, CONTROLS + 1, b);
}

/* the target line by where the rider is ($7A9C, as $EEC2's) */
static uint8_t target_line(uint8_t line) { return line < 0x65 ? 0x45 : line < 0xA1 ? 0x81 : 0xD0; }

int brain_target(Game *g, uint16_t u, uint16_t *px)
{
    uint16_t x = word(g, 0xA073);
    *px = x;
    if (!x) return 0;
    if (peek(g, 0xA0A8)) {
        x = word(g, 0xA075);
        *px = x;
        if (!x) return 0;
        return !peek(g, 0xA0A9);
    }
    uint16_t y = word(g, 0xA075);
    if (!y || peek(g, 0xA0A9)) return 1;
    /* the nearer by line (bigger is nearer) or by x within 256 */
    uint8_t s0, s1, b;
    uint8_t xb = peek(g, F(x, 0xB)), ub = peek(g, F(u, 0xB));
    b = (uint8_t)(xb - ub);
    if (xb >= ub) b = (uint8_t)-b;
    s0 = b;
    uint16_t xx = word(g, F(x, 7)), ux = word(g, F(u, 7)), d = (uint16_t)(xx - ux);
    if (xx >= ux) d = (uint16_t)(~d - 1);
    b = (uint8_t)d;
    if (!(d >> 8) && b <= s0) s0 = b;
    uint8_t yb = peek(g, F(y, 0xB));
    b = (uint8_t)(yb - ub);
    if (yb >= ub) b = (uint8_t)-b;
    s1 = b;
    uint16_t yx = word(g, F(y, 7));
    d = (uint16_t)(yx - ux);
    if (yx >= ux) d = (uint16_t)(~d - 1);
    b = (uint8_t)d;
    if (!(d >> 8) && b <= s1) s1 = b;
    (void)s1;
    if (b < s0) return 1; /* CMPB 1,S: the last B against player 1's */
    *px = word(g, 0xA075);
    return 1;
}

/* the platform map beside the rider: $AEA2 + x against $E829 + line */
static uint8_t edge_ahead(const Game *g, uint16_t u)
{
    return game_read(g, (uint16_t)(word(g, F(u, 7)) - 0x515E)) & game_read(g, (uint16_t)(word(g, F(u, 0xA)) - 0x17D7));
}

/* a platform where the rider will be in 8 frames, on its left (-$517D) or right (-$513F) */
static uint8_t wall_ahead(const Game *g, uint16_t u, int right)
{
    uint16_t v = word(g, F(u, 0x11));
    int8_t a = (int8_t)(v << 3 >> 8);
    uint16_t y = (uint16_t)(word(g, F(u, 0xA)) + a);
    return game_read(g, (uint16_t)(word(g, F(u, 7)) - (right ? 0x513F : 0x517D))) & game_read(g, (uint16_t)(y - 0x17C9));
}

int brain_enemy(Game *g, uint16_t u, uint16_t at)
{
    uint8_t a = 0, b = 0;
    uint16_t d = 0, x = 0;
    for (int guard = 0; guard < 64; guard++) {
        switch (at) {
        /* ---- the first family ---------------------------------------------- */
        case 0x7A83:
            if (peek(g, 0xA08E) < peek(g, 0xA08F)) { at = 0x7AD5; break; }
            if (dec(g, F(u, 0x32)) > 0) { at = 0x7A9C; break; }
            game_poke(g, F(u, 0x32), peek(g, 0xB1B4));
            x = word(g, 0xA02C);
            if (peek(g, F(x, 2)) == 0x15) { at = 0x7AB6; break; }
            /* fall through */
        case 0x7A9C:
            b = 0;
            if (!((uint8_t)(target_line(peek(g, F(u, 0xB))) - peek(g, F(u, 0xB))) & 0x80)) { at = 0x7ABE; break; }
            if (peek(g, F(u, 0x11)) & 0x80) { at = 0x7ABE; break; }
            /* fall through */
        case 0x7AB6:
            game_set_word(g, F(u, 0x24), 0x7ACC);
            b = 1;
            /* fall through */
        case 0x7ABE:
            controls(g, peek(g, F(u, 0xF)) & 0x80 ? 0xFF : 0x01, b);
            return 0;
        case 0x7ACC:
            game_set_word(g, F(u, 0x24), 0x7A83);
            b = 0;
            at = 0x7ABE;
            break;
        case 0x7AD5:
            game_poke(g, 0xA08E, (uint8_t)(peek(g, 0xA08E) + 1));
            game_poke(g, F(u, 0x34), (uint8_t)(peek(g, F(u, 0x34)) + 1));
            x = ram_word(g, (uint16_t)(word(g, F(u, 0xD)) + 2));
            game_set_word(g, F(u, 0x24), x);
            at = x;
            break;
        case 0x7B0B:
            if (dec(g, F(u, 0x32)) > 0) { at = 0x7B1E; break; }
            game_poke(g, F(u, 0x32), peek(g, 0xB1B4));
            x = word(g, 0xA02C);
            if (peek(g, F(x, 2)) == 0x15) { at = 0x7B5C; break; }
            /* fall through */
        case 0x7B1E:
            if (!brain_target(g, u, &x)) { at = 0x7C07; break; }
            d = (uint16_t)(word(g, F(x, 0xA)) - word(g, F(u, 0xA)));
            if ((int16_t)word(g, F(x, 0xA)) < (int16_t)word(g, F(u, 0xA))) { at = 0x7B86; break; }
            if ((int16_t)d < (int16_t)word(g, 0xB1BC)) { at = 0x7C07; break; }
            game_set_word(g, F(u, 0x2B), word(g, 0xB1BF));
            game_set_word(g, F(u, 0x24), 0x7B42);
            at = 0x7B54;
            break;
        case 0x7B42:
            if (word(g, F(u, 0x20))) { at = 0x7B0B; break; }
            d = word(g, F(u, 0x11));
            if (d & 0x8000) { at = 0x7BC5; break; }
            d = (uint16_t)(d + word(g, F(u, 0x2B)));
            if (!(d & 0x8000)) { at = 0x7B0B; break; }
            game_set_word(g, F(u, 0x2B), d);
            /* fall through */
        case 0x7B54:
            if ((uint16_t)(word(g, F(u, 0x11)) - word(g, 0xB1C2)) & 0x8000) { at = 0x7BC5; break; }
            /* fall through */
        case 0x7B5C:
            game_set_word(g, F(u, 0x24), 0x7B66);
            a = 2;
            at = 0x7BBE;
            break;
        case 0x7B66:
            d = word(g, F(u, 0x11));
            if (!(d & 0x8000)) {
                d = (uint16_t)(d + word(g, F(u, 0x2B)));
                if (!(d & 0x8000)) { at = 0x7B0B; break; }
                game_set_word(g, F(u, 0x2B), d);
            }
            b = 1;
            if (dec(g, F(u, 0x2D)) > 0) { at = 0x7BC6; break; }
            game_set_word(g, F(u, 0x24), 0x7B42);
            b = 0;
            at = 0x7BC6;
            break;
        case 0x7B86: /* (d from $7B1E) */
            if ((int16_t)d > (int16_t)word(g, 0xB1C5)) { at = 0x7C07; break; }
            x = word(g, F(u, 7));
            if (edge_ahead(g, u)) { at = 0x7C07; break; }
            game_set_word(g, F(u, 0x2B), word(g, 0xB1C8));
            at = 0x7BB5;
            break;
        case 0x7BA3:
            d = word(g, F(u, 0x11));
            if (d & 0x8000) {
                d = (uint16_t)(d + word(g, F(u, 0x2B)));
                if (d & 0x8000) { at = 0x7B0B; break; }
                game_set_word(g, F(u, 0x2B), d);
            }
            if (dec(g, F(u, 0x2D)) > 0) { at = 0x7BC5; break; }
            /* fall through */
        case 0x7BB5:
            game_set_word(g, F(u, 0x24), 0x7BE1);
            a = peek(g, 0xB1CC);
            /* fall through */
        case 0x7BBE:
            game_poke(g, F(u, 0x2D), a);
            b = 1;
            at = 0x7BC6;
            break;
        case 0x7BC5:
            b = 0;
            /* fall through */
        case 0x7BC6:
            if (peek(g, F(u, 0xB)) >= 0xD3 && !(peek(g, F(u, 0x11)) & 0x80)) { at = 0x7C7D; break; }
            /* fall through */
        case 0x7BD3:
            controls(g, peek(g, F(u, 0xF)) & 0x80 ? 0xFF : 0x01, b);
            return 0;
        case 0x7BE1:
            d = word(g, F(u, 0x11));
            if (d & 0x8000) {
                d = (uint16_t)(d + word(g, F(u, 0x2B)));
                if (d & 0x8000) { at = 0x7B0B; break; }
                game_set_word(g, F(u, 0x2B), d);
            }
            b = 1;
            if (dec(g, F(u, 0x2D)) > 0) { at = 0x7BC6; break; }
            game_poke(g, F(u, 0x2D), peek(g, 0xB1CF));
            game_set_word(g, F(u, 0x24), 0x7BA3);
            b = 0;
            at = 0x7BC6;
            break;
        case 0x7C07: /* (x from before) */
            game_set_word(g, F(u, 0x24), 0x7C1E);
            game_set_word(g, F(u, 0x2B), word(g, F(u, 0xA)));
            game_poke(g, F(u, 0x2F), game_read(g, F(x, 0x14)));
            game_poke(g, F(u, 0x2D), peek(g, 0xB1D2));
            /* fall through */
        case 0x7C1E:
            if (dec(g, F(u, 0x2D)) <= 0) { at = 0x7B0B; break; }
            if (peek(g, F(u, 0xB)) < 0xD3) { at = 0x7C40; break; }
            if (!brain_target(g, u, &x)) { at = 0x7C4C; break; }
            d = (uint16_t)(word(g, F(x, 7)) + 0x3F);
            if ((int16_t)d < (int16_t)word(g, F(u, 7))) { at = 0x7C4C; break; }
            d = (uint16_t)(d + 0xFF82);
            if ((int16_t)d > (int16_t)word(g, F(u, 7))) { at = 0x7C4C; break; }
            /* fall through */
        case 0x7C40:
            if (peek(g, F(u, 0x11)) & 0x80) { at = 0x7C5C; break; }
            if (peek(g, F(u, 0x2C)) < peek(g, F(u, 0xB))) { at = 0x7C5C; break; }
            /* fall through */
        case 0x7C4C:
            game_set_word(g, F(u, 0x24), 0x7C56);
            b = 1;
            at = 0x7C5D;
            break;
        case 0x7C56:
            game_set_word(g, F(u, 0x24), 0x7C1E);
            /* fall through */
        case 0x7C5C:
            b = 0;
            /* fall through */
        case 0x7C5D:
            if (peek(g, F(u, 0x2F)) == peek(g, F(u, 0x14)) && dec(g, F(u, 0x2E)) >= 0) {
                game_poke(g, F(u, 0x2E), 0);
                game_poke(g, F(u, 0xF), (uint8_t)~peek(g, F(u, 0xF)));
            }
            at = 0x7BD3;
            break;
        case 0x7C72:
            if (peek(g, F(u, 0xB)) < 0xD3 || (peek(g, F(u, 0x11)) & 0x80)) { at = 0x7C90; break; }
            /* fall through */
        case 0x7C7D:
            game_set_word(g, F(u, 0x24), 0x7C87);
            b = 1;
            at = 0x7BD3;
            break;
        case 0x7C87:
            game_set_word(g, F(u, 0x24), 0x7C72);
            b = 0;
            at = 0x7BD3;
            break;
        case 0x7C90:
            at = ram_word(g, (uint16_t)(word(g, F(u, 0xD)) + 2));
            break;
        /* ---- the second family --------------------------------------------- */
        case 0x7C95:
            if (dec(g, F(u, 0x32)) > 0) { at = 0x7CA8; break; }
            game_poke(g, F(u, 0x32), peek(g, 0xB1B4));
            x = word(g, 0xA02C);
            if (peek(g, F(x, 2)) == 0x15) { at = 0x7CE5; break; }
            /* fall through */
        case 0x7CA8:
            if (!brain_target(g, u, &x)) { at = 0x7D58; break; }
            d = (uint16_t)(word(g, F(x, 0xA)) - word(g, F(u, 0xA)));
            if ((int16_t)word(g, F(x, 0xA)) < (int16_t)word(g, F(u, 0xA))) { at = 0x7D16; break; }
            if ((int16_t)d < (int16_t)word(g, 0xB1D4)) { at = 0x7D58; break; }
            game_set_word(g, F(u, 0x2B), word(g, 0xB1D7));
            game_set_word(g, F(u, 0x24), 0x7CF5);
            at = 0x7D0F;
            break;
        case 0x7CCB:
            if (word(g, F(u, 0x20))) { at = 0x7C95; break; }
            d = word(g, F(u, 0x11));
            if (d & 0x8000) { at = 0x7D0F; break; }
            d = (uint16_t)(d + word(g, F(u, 0x2B)));
            if (!(d & 0x8000)) { at = 0x7C95; break; }
            game_set_word(g, F(u, 0x2B), d);
            if ((uint16_t)(word(g, F(u, 0x11)) - word(g, 0xB1DA)) & 0x8000) { at = 0x7D0F; break; }
            /* fall through */
        case 0x7CE5:
            game_set_word(g, F(u, 0x24), 0x7CF5);
            game_poke(g, F(u, 0x2D), 2);
            b = 1;
            at = 0x7DBF;
            break;
        case 0x7CF5:
            d = word(g, F(u, 0x11));
            if (!(d & 0x8000)) {
                d = (uint16_t)(d + word(g, F(u, 0x2B)));
                if (!(d & 0x8000)) { at = 0x7C95; break; }
                game_set_word(g, F(u, 0x2B), d);
            }
            b = 1;
            if (dec(g, F(u, 0x2D)) > 0) { at = 0x7DBF; break; }
            game_set_word(g, F(u, 0x24), 0x7CCB);
            /* fall through */
        case 0x7D0F:
            b = 0;
            at = 0x7DBF;
            break;
        case 0x7D16: /* (d from $7CA8) */
            if ((int16_t)d > (int16_t)word(g, 0xB1DD)) { at = 0x7D58; break; }
            x = word(g, F(u, 7));
            if (edge_ahead(g, u)) { at = 0x7E83; break; }
            /* fall through */
        case 0x7D2D:
            game_set_word(g, F(u, 0x2B), word(g, 0xB1E0));
            at = 0x7D49;
            break;
        case 0x7D35:
            d = word(g, F(u, 0x11));
            if (d & 0x8000) {
                d = (uint16_t)(d + word(g, F(u, 0x2B)));
                if (d & 0x8000) { at = 0x7C95; break; }
                game_set_word(g, F(u, 0x2B), d);
            }
            b = 1;
            if (dec(g, F(u, 0x2D)) > 0) { at = 0x7DBF; break; }
            /* fall through */
        case 0x7D49:
            game_poke(g, F(u, 0x2D), peek(g, 0xB1E7));
            game_set_word(g, F(u, 0x24), 0x7E47);
            b = 0;
            at = 0x7DBF;
            break;
        case 0x7D58: /* (x from before) */
            game_set_word(g, F(u, 0x24), 0x7D6F);
            game_set_word(g, F(u, 0x2B), word(g, F(u, 0xA)));
            game_poke(g, F(u, 0x2F), game_read(g, F(x, 0x14)));
            game_poke(g, F(u, 0x2D), peek(g, 0xB1ED));
            /* fall through */
        case 0x7D6F:
            if (dec(g, F(u, 0x2D)) <= 0) { at = 0x7C95; break; }
            if (peek(g, F(u, 0x11)) & 0x80) { at = 0x7E40; break; }
            if (peek(g, F(u, 0xB)) < 0xD3) { at = 0x7D9A; break; }
            if (!brain_target(g, u, &x)) { at = 0x7DA3; break; }
            d = (uint16_t)(word(g, F(x, 7)) + 0x2D);
            if ((int16_t)d < (int16_t)word(g, F(u, 7))) { at = 0x7DA3; break; }
            d = (uint16_t)(d + 0xFFA6);
            if ((int16_t)d > (int16_t)word(g, F(u, 7))) { at = 0x7DA3; break; }
            /* fall through */
        case 0x7D9A:
            if (peek(g, F(u, 0x2C)) < peek(g, F(u, 0xB))) { at = 0x7E40; break; }
            /* fall through */
        case 0x7DA3:
            game_set_word(g, F(u, 0x24), 0x7E3A);
            b = 1;
            /* fall through */
        case 0x7DAB:
            if (peek(g, F(u, 0x2F)) == peek(g, F(u, 0x14)) && dec(g, F(u, 0x2E)) >= 0) {
                game_poke(g, F(u, 0x2E), 0);
                game_poke(g, F(u, 0xF), (uint8_t)~peek(g, F(u, 0xF)));
            }
            at = 0x7DCD;
            break;
        case 0x7DBF:
            if (peek(g, F(u, 0xB)) >= 0xD3 && !(peek(g, F(u, 0x11)) & 0x80)) { at = 0x7C7D; break; }
            /* fall through */
        case 0x7DCD:
            a = peek(g, F(u, 0x14));
            if (!a) { at = 0x7E25; break; }
            if (a & 0x80) {
                if (!wall_ahead(g, u, 0)) { at = 0x7E25; break; }
                game_poke(g, F(u, 0xF), 0);
            } else {
                if (!wall_ahead(g, u, 1)) { at = 0x7E25; break; }
                game_poke(g, F(u, 0xF), 0xFF);
            }
            game_set_word(g, F(u, 0x24), 0x7E7A);
            game_poke(g, F(u, 0x2D), 8);
            b = 1;
            /* fall through */
        case 0x7E25:
            a = peek(g, F(u, 0x22));
            if (a) game_poke(g, F(u, 0xF), a);
            controls(g, peek(g, F(u, 0xF)) & 0x80 ? 0xFF : 0x01, b);
            return 0;
        case 0x7E3A:
            game_set_word(g, F(u, 0x24), 0x7D6F);
            /* fall through */
        case 0x7E40:
            b = 0;
            at = 0x7DAB;
            break;
        case 0x7E47:
            d = word(g, F(u, 0x11));
            if (d & 0x8000) {
                d = (uint16_t)(d + word(g, F(u, 0x2B)));
                if (d & 0x8000) { at = 0x7C95; break; }
                game_set_word(g, F(u, 0x2B), d);
            }
            if (dec(g, F(u, 0x2D)) > 0) { at = 0x7E76; break; }
            game_poke(g, F(u, 0x2D), (uint8_t)(peek(g, F(u, 0x2D)) + 1));
            if ((int16_t)word(g, F(u, 0x11)) < (int16_t)word(g, 0xB1E9)) { at = 0x7E76; break; }
            game_set_word(g, F(u, 0x24), 0x7D35);
            game_poke(g, F(u, 0x2D), peek(g, 0xB1E4));
            b = 1;
            at = 0x7DBF;
            break;
        case 0x7E76:
            b = 0;
            at = 0x7DBF;
            break;
        case 0x7E7A:
            b = 0;
            if (dec(g, F(u, 0x2D)) > 0) { at = 0x7E25; break; }
            at = 0x7C95;
            break;
        case 0x7E83:
            game_set_word(g, F(u, 0x24), 0x7E93);
            game_set_word(g, F(u, 0x2B), word(g, F(u, 0xA)));
            game_poke(g, F(u, 0x2D), 0x15);
            /* fall through */
        case 0x7E93:
            if (!edge_ahead(g, u)) { at = 0x7D2D; break; }
            if (dec(g, F(u, 0x2D)) <= 0) { at = 0x7C95; break; }
            if ((uint16_t)(word(g, F(u, 0x11)) + 0xFFC0) & 0x8000) { at = 0x7ECB; break; }
            if (peek(g, F(u, 0x2C)) < peek(g, F(u, 0xB))) { at = 0x7ECB; break; }
            game_set_word(g, F(u, 0x24), 0x7EC5);
            b = 1;
            at = 0x7E25;
            break;
        case 0x7EC5:
            game_set_word(g, F(u, 0x24), 0x7E93);
            /* fall through */
        case 0x7ECB:
            b = 0;
            at = 0x7E25;
            break;
        /* ---- the third family ---------------------------------------------- */
        case 0x7ECF:
            if (dec(g, F(u, 0x32)) > 0) { at = 0x7EE2; break; }
            game_poke(g, F(u, 0x32), peek(g, 0xB1B4));
            x = word(g, 0xA02C);
            if (peek(g, F(x, 2)) == 0x15) { at = 0x7F23; break; }
            /* fall through */
        case 0x7EE2:
            if (!brain_target(g, u, &x)) { at = 0x7F99; break; }
            d = (uint16_t)(word(g, F(x, 0xA)) - word(g, F(u, 0xA)));
            if ((int16_t)word(g, F(x, 0xA)) < (int16_t)word(g, F(u, 0xA))) { at = 0x7F0C; break; }
            if ((int16_t)d < (int16_t)word(g, 0xB1EF)) { at = 0x7F41; break; }
            game_set_word(g, F(u, 0x24), 0x80AF);
            b = 0;
            if (peek(g, F(u, 0xB)) >= 0xD3 && !(peek(g, F(u, 0x14)) & 0x80)) b++;
            at = 0x803F;
            break;
        case 0x7F0C: /* (d from $7EE2) */
            if ((int16_t)d > (int16_t)word(g, 0xB1F2)) { at = 0x7F41; break; }
            x = word(g, F(u, 7));
            if (edge_ahead(g, u)) { at = 0x8063; break; }
            /* fall through */
        case 0x7F23:
            game_set_word(g, F(u, 0x24), 0x7F2D);
            /* fall through */
        case 0x7F29:
            b = 0;
            at = 0x803F;
            break;
        case 0x7F2D:
            game_set_word(g, F(u, 0x24), 0x7ECF);
            if ((int16_t)word(g, F(u, 0x11)) < (int16_t)word(g, 0xB1F5)) { at = 0x7F29; break; }
            b = 1;
            at = 0x803F;
            break;
        case 0x7F41: /* (x the target) */
            game_set_word(g, F(u, 0x24), 0x7F58);
            game_poke(g, F(u, 0x2C), game_read(g, F(x, 0xB)));
            game_poke(g, F(u, 0x2F), game_read(g, F(x, 0x14)));
            game_poke(g, F(u, 0x2D), peek(g, 0xB1F9));
            /* fall through */
        case 0x7F58:
            if (dec(g, F(u, 0x2D)) <= 0) { at = 0x7ECF; break; }
            b = peek(g, F(u, 0xB));
            if (b >= 0xD3 && !(peek(g, F(u, 0x11)) & 0x80)) { at = 0x7F6F; break; }
            if (b <= peek(g, F(u, 0x2C))) { at = 0x7F7F; break; }
            /* fall through */
        case 0x7F6F:
            game_set_word(g, F(u, 0x24), 0x7F79);
            b = 1;
            at = 0x7F80;
            break;
        case 0x7F79:
            game_set_word(g, F(u, 0x24), 0x7F58);
            /* fall through */
        case 0x7F7F:
            b = 0;
            /* fall through */
        case 0x7F80:
            if (peek(g, F(u, 0x2F)) == peek(g, F(u, 0x14)) && dec(g, F(u, 0x2E)) >= 0) {
                game_poke(g, F(u, 0x2E), 0);
                game_poke(g, F(u, 0xF), (uint8_t)~peek(g, F(u, 0xF)));
            }
            at = 0x802F;
            break;
        case 0x7F99:
            game_set_word(g, F(u, 0x24), 0x7FAA);
            game_poke(g, F(u, 0x2C), peek(g, F(u, 0xB)));
            game_poke(g, F(u, 0x2D), peek(g, 0xB1FC));
            /* fall through */
        case 0x7FAA:
            if (dec(g, F(u, 0x2D)) <= 0) { at = 0x7ECF; break; }
            if (peek(g, F(u, 0x11)) & 0x80) { at = 0x8053; break; }
            if (peek(g, F(u, 0x2C)) < peek(g, F(u, 0xB))) { at = 0x8053; break; }
            game_set_word(g, F(u, 0x24), 0x804D);
            b = 1;
            /* fall through */
        case 0x7FC9:
            if (peek(g, F(u, 0xB)) >= 0xD0 && !(peek(g, F(u, 0x11)) & 0x80)) { at = 0x7C7D; break; }
            a = peek(g, F(u, 0x14));
            if (!a) { at = 0x802F; break; }
            if (a & 0x80) {
                if (!wall_ahead(g, u, 0)) { at = 0x802F; break; }
                game_poke(g, F(u, 0xF), 0);
            } else {
                if (!wall_ahead(g, u, 1)) { at = 0x802F; break; }
                game_poke(g, F(u, 0xF), 0xFF);
            }
            game_set_word(g, F(u, 0x24), 0x805A);
            game_poke(g, F(u, 0x2D), peek(g, 0xB1FF));
            b = 1;
            /* fall through */
        case 0x802F:
            a = peek(g, F(u, 0x22));
            if (a) game_poke(g, F(u, 0xF), a);
            controls(g, peek(g, F(u, 0xF)) & 0x80 ? 0xFF : 0x01, b);
            return 0;
        case 0x803F:
            if (!peek(g, F(u, 0x14))) { at = 0x802F; break; }
            controls(g, 0, b);
            return 0;
        case 0x804D:
            game_set_word(g, F(u, 0x24), 0x7FAA);
            /* fall through */
        case 0x8053:
            b = 0;
            at = 0x7FC9;
            break;
        case 0x805A:
            b = 0;
            if (dec(g, F(u, 0x2D)) > 0) { at = 0x802F; break; }
            at = 0x7ECF;
            break;
        case 0x8063:
            game_set_word(g, F(u, 0x24), 0x8073);
            game_poke(g, F(u, 0x2C), peek(g, F(u, 0xB)));
            game_poke(g, F(u, 0x2D), 0x15);
            /* fall through */
        case 0x8073:
            if (!edge_ahead(g, u)) { at = 0x7F23; break; }
            if (dec(g, F(u, 0x2D)) <= 0) { at = 0x7ECF; break; }
            if ((uint16_t)(word(g, F(u, 0x11)) + 0xFFC0) & 0x8000) { at = 0x80AB; break; }
            if (peek(g, F(u, 0x2C)) < peek(g, F(u, 0xB))) { at = 0x80AB; break; }
            game_set_word(g, F(u, 0x24), 0x80A5);
            b = 1;
            at = 0x802F;
            break;
        case 0x80A5:
            game_set_word(g, F(u, 0x24), 0x8073);
            /* fall through */
        case 0x80AB:
            b = 0;
            at = 0x802F;
            break;
        case 0x80AF:
            game_set_word(g, F(u, 0x24), 0x7ECF);
            b = 0;
            if (!peek(g, F(u, 0x14))) { at = 0x802F; break; }
            if (peek(g, F(u, 0x20)) &&
                (game_read(g, (uint16_t)(word(g, F(u, 7)) - 0x515E)) &
                 game_read(g, (uint16_t)(word(g, F(u, 0xA)) - 0x17C9))))
                b = 1;
            controls(g, 0, b);
            return 0;
        default:
            return -1;
        }
    }
    return -1;
}
