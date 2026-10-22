#include "sim/waves.h"

#include <stddef.h>

#include "sim/ptero.h"
#include "sim/score.h"
#include "sim/tasks.h"
#include "sim/text.h"

/* where the wave task resumes */
#define FIRST_WAIT 0x6BDA
#define NEXT_WAVE 0x6CD8
#define MESSAGES_ON 0x6E8E
#define PLATFORM_ON 0x6F9B
#define ENEMIES_ON 0x6E25
#define EGGS_ON 0x7480
#define MANAGER 0x6D91

#define PTEROS 0x7200
#define PTEROS_ON 0x7205

int waves_ported(uint16_t at)
{
    return at == WAVES_START || at == FIRST_WAIT || at == NEXT_WAVE || at == MESSAGES_ON || at == PLATFORM_ON ||
           at == ENEMIES_ON || at == EGGS_ON || at == PTEROS || at == PTEROS_ON;
}

static uint16_t F(uint16_t u, int off) { return (uint16_t)(u + off); }
static uint8_t peek(const Game *g, uint16_t a) { return game_peek(g, a); }
static void poke(Game *g, uint16_t a, uint8_t v) { game_poke(g, a, v); }
static uint16_t word(const Game *g, uint16_t a) { return game_word(g, a); }
static void set_word(Game *g, uint16_t a, uint16_t v) { game_set_word(g, a, v); }
static uint8_t rom(const Game *g, uint16_t a) { return game_read(g, a); }
static uint16_t rom_word(const Game *g, uint16_t a) { return game_read_word(g, a); }

/* the return addresses on the task's stack within a step (the ROM keeps
   those it needs across a sleep in +$F and +$11) */
typedef struct {
    uint16_t s[16];
    int n;
} Stack;

static int push(Stack *st, uint16_t a)
{
    if (st->n == 16) return -1;
    st->s[st->n++] = a;
    return 0;
}

/* $6EB8: the message slot at y (msg, colour, the screen address), if set */
static void slot_show(Game *g, uint16_t y)
{
    uint8_t a = peek(g, y);
    if (a) text_message_small(g, a, peek(g, F(y, 1)), word(g, F(y, 2)));
}

/* $6EA3: the message at x (4 bytes) into the first free slot (+$23 on), shown */
static void message(Game *g, uint16_t u, uint16_t x)
{
    uint16_t y = F(u, 0x1F);
    do y = (uint16_t)(y + 4);
    while (peek(g, y));
    if (!peek(g, 0xA090)) return;
    set_word(g, y, rom_word(g, x));
    set_word(g, F(y, 2), rom_word(g, F(x, 2)));
    slot_show(g, y);
}

/* $6FAC / $6FB4: "WAVE" and its number (<$9E, a leading 0 blank), or their erase */
static void wave_number(Game *g, uint16_t u, int show)
{
    uint8_t colour = show ? 0x11 : 0;
    poke(g, F(u, 0x1F), show ? 0x59 : 0);
    uint16_t x = text_message_small(g, 0x59, colour, 0x3A60);
    uint8_t a = peek(g, 0xA09E);
    if (!(a & 0xF0)) a |= 0xF0;
    text_number_small(g, a, colour, x);
}

/* $6EC4: every message erased */
static void messages_off(Game *g, uint16_t u)
{
    if (peek(g, F(u, 0x1F))) wave_number(g, u, 0);
    for (int off = 0x23; off <= 0x33; off += 4) {
        uint16_t y = F(u, off);
        uint8_t a = peek(g, y);
        if (!a) continue;
        poke(g, y, 0);
        text_message_small(g, a, 0, word(g, F(y, 2)));
    }
}

/* $6FA1: n bytes from x ANDed with a */
static void and_bytes(Game *g, uint16_t x, uint16_t n, uint8_t a)
{
    for (; n; n--, x++) poke(g, x, (uint8_t)(a & peek(g, x)));
}

/* $6F52 / $6F77: a platform's bits (a) put back into a map from its template */
static void map_put(Game *g, uint16_t x, uint16_t end, uint16_t y, uint8_t a)
{
    for (; x < end; x++, y++) {
        poke(g, x, (uint8_t)(~a & peek(g, x)));
        poke(g, x, (uint8_t)((a & rom(g, y)) + peek(g, x)));
    }
}

/* $6E01: a lava bridge's task ($8638, type $6A) after the one before this */
static uint16_t bridge(Game *g)
{
    uint16_t y = task_create(g, word(g, TASK_PREVIOUS), 0x8638, 0x6A, 0xFF);
    set_word(g, F(y, 0xA), 0x00DF);
    poke(g, F(y, 0x10), 0);
    return y;
}

/* $7417: an egg of the egg wave in slot a */
static void egg(Game *g, uint16_t u, uint8_t a)
{
    poke(g, 0xA0AA, (uint8_t)(peek(g, 0xA0AA) + 1));
    uint16_t y = task_create(g, u, 0x769F, 0x82, 0xFF);
    uint16_t x = 0x7486;
    do x = (uint16_t)(x + 4);
    while (a >= rom(g, x));
    poke(g, F(y, 0x35), 8);
    uint16_t d = (uint16_t)(a * 8 + rom_word(g, F(x, 2)));
    set_word(g, F(y, 7), d);
    set_word(g, F(y, 0x29), (uint16_t)(d + 6));
    uint8_t b = rom(g, F(x, 1));
    set_word(g, F(y, 0xA), b);
    poke(g, F(y, 0x27), b);
    poke(g, F(y, 0x28), (uint8_t)(b - 6));
    set_word(g, F(y, 0xD), word(g, F(u, 0xD)));
    b = peek(g, F(u, 0x1A));
    poke(g, F(y, 0x2D), b);
    uint8_t n = (uint8_t)(peek(g, F(u, 0x1D)) - 1);
    poke(g, F(u, 0x1D), n);
    if (!(n & 0x80)) { /* all but the first two hatch sooner, by chance */
        uint8_t r = game_random(g, NULL);
        poke(g, F(y, 0x2D), (uint8_t)((uint8_t)-((r * b) >> 8) + peek(g, F(y, 0x2D))));
    }
    set_word(g, F(y, 0x1C), 0);
    set_word(g, F(y, 0x2B), 0);
    poke(g, F(y, 0x34), 0);
    poke(g, F(y, 0x33), 4);
}

/* The wave task from `at` until it sleeps: the ROM's blocks by address. */
static StepResult run(Game *g, uint16_t u, uint16_t at)
{
    Stack st = { { 0 }, 0 };
    uint8_t a = 0, b = 0;
    uint16_t x = 0, y = 0;
    for (int guard = 0; guard < 4096; guard++) {
        switch (at) {
        case WAVES_START: /* $6BC3: a game's first wave */
            poke(g, 0xA09E, 0);
            set_word(g, F(u, 0x13), 0x71C8);
            set_word(g, F(u, 7), 0x702F);
            poke(g, F(u, 0x1C), 2);
            return task_sleep(g, 0x20, FIRST_WAIT);
        case FIRST_WAIT: at = 0x6C38; break;
        case WAVES_END: { /* $6BDC: the wave's end: the settings ramp up */
            if (peek(g, 0xA03C)) poke(g, 0xA03C, (uint8_t)(peek(g, 0xA03C) - 1));
            uint8_t level = game_cmos_byte(g, 0xCC14);
            x = 0xDE11;
            y = 0xB1AD;
            do {
                b = peek(g, F(y, 2));
                if (b) {
                    poke(g, F(y, 2), (uint8_t)(b - 1));
                    if (b != 1) goto next;
                }
                b = (uint8_t)(level + 0x10);
                int odd = b & 1;
                b = rom(g, (uint16_t)(x + (int8_t)(uint8_t)((int8_t)b >> 1)));
                if (!odd) b >>= 4;
                poke(g, F(y, 2), b & 0x0F);
                {
                    int16_t d = (int16_t)((int8_t)rom(g, F(x, 0xD)) + (int16_t)word(g, y));
                    int16_t lim = (int16_t)rom_word(g, F(x, 6));
                    if ((int8_t)rom(g, F(x, 0xD)) < 0 ? d <= lim : d >= lim) d = lim;
                    set_word(g, y, (uint16_t)d);
                }
            next:
                x = (uint16_t)(x + 0xE);
                y = (uint16_t)(y + 3);
            } while (x < 0xDF99);
            a = peek(g, 0xA09B);
            if (a > 0xE0) poke(g, 0xA09B, (uint8_t)(a - 5));
            at = 0x6C38;
            break;
        }
        case 0x6C38:
            if (peek(g, 0xA09F)) { /* the lava bridges burn */
                poke(g, 0xA09F, (uint8_t)(peek(g, 0xA09F) - 1));
                if (!peek(g, 0xA09F)) {
                    y = bridge(g);
                    set_word(g, F(y, 0x2B), 0xFFE0);
                    poke(g, F(y, 0x14), 1);
                    poke(g, F(y, 0x2D), 1);
                    y = bridge(g);
                    set_word(g, F(y, 0x2B), 0x013F);
                    poke(g, F(y, 0x14), 7);
                    poke(g, F(y, 0x2D), 0xFF);
                }
            } else if (peek(g, 0xA0A0)) {
                poke(g, 0xA0A0, (uint8_t)(peek(g, 0xA0A0) - 1));
                if (!peek(g, 0xA0A0)) { /* the lava's flames, after the list's first task */
                    uint16_t first = word(g, TASK_HEAD);
                    y = task_create(g, first, 0x86D0, 0x69, 0xFF);
                    set_word(g, F(y, 0xA), 0x00EF);
                    set_word(g, F(y, 7), 0x0014);
                    poke(g, F(y, 0x10), 0);
                    y = task_create(g, first, 0x86D0, 0x69, 0xFF);
                    set_word(g, F(y, 0xA), 0x00EF);
                    set_word(g, F(y, 7), 0x0106);
                    poke(g, F(y, 0x10), 2);
                    poke(g, F(y, 4), 4);
                }
            }
            /* $6CA6 */
            poke(g, 0xA036, 0);
            poke(g, 0xA037, 0);
            poke(g, 0xA08F, 0);
            poke(g, 0xA0A2, 0xFF);
            poke(g, 0xA0B0, 1);
            for (int off = 0x1F; off <= 0x33; off += 4) poke(g, F(u, off), 0);
            {
                int c, h;
                uint8_t s = game_add(peek(g, 0xA09E), 1, 0, &c, &h);
                poke(g, 0xA09E, game_daa(s, &c, h));
            }
            if (push(&st, 0x6CD0)) return STEP_UNPORTED;
            at = word(g, F(u, 0x13)); /* the last wave's ending */
            break;
        case 0x6CD0:
            poke(g, 0xA0A3, 0); /* $D761 */
            poke(g, 0xA0A4, 0);
            messages_off(g, u);
            return task_sleep(g, 1, NEXT_WAVE);
        case NEXT_WAVE:
            wave_number(g, u, 1);
            set_word(g, F(u, 0x13), 0x71C8);
            x = word(g, F(u, 7));
            a = rom(g, F(x, 3));
            x = (uint16_t)(x + 4);
            if (x >= 0x719B) x = 0x7173;
            set_word(g, F(u, 7), x);
            a = (uint8_t)((a ^ rom(g, F(x, 3))) & 0xF0);
            if (!a) {
                at = 0x6D11;
                break;
            }
            b = rom(g, F(x, 3));
            x = 0x6FCB;
            if (push(&st, 0x6CFF)) return STEP_UNPORTED;
            at = 0x6EF0;
            break;
        case 0x6CFF: x = 0x6FD2; if (push(&st, 0x6D05)) return STEP_UNPORTED; at = 0x6EF0; break;
        case 0x6D05: x = 0x6FD9; if (push(&st, 0x6D0B)) return STEP_UNPORTED; at = 0x6EF0; break;
        case 0x6D0B: x = 0x6FE0; if (push(&st, 0x6D11)) return STEP_UNPORTED; at = 0x6EF0; break;
        case 0x6D11:
            x = word(g, F(u, 7));
            if (rom(g, F(x, 3)) & 1) poke(g, 0xA08F, rom(g, F(x, 1)) & 0x0F);
            b = rom(g, F(x, 3)) & 0x0E;
            if (push(&st, 0x6D28)) return STEP_UNPORTED;
            at = rom_word(g, (uint16_t)(0x71BC + b)); /* the wave's kind */
            break;
        case 0x6D28: a = 0x1E; if (push(&st, 0x6D2D)) return STEP_UNPORTED; at = 0x6E64; break;
        case 0x6D2A: if (push(&st, 0x6D2D)) return STEP_UNPORTED; at = 0x6E64; break;
        case 0x6D2D: if (push(&st, 0x6D30)) return STEP_UNPORTED; at = 0x6E53; break;
        case 0x6D30:
            messages_off(g, u);
            /* $6D33: the enemies, a task each */
            a = (uint8_t)(rom(g, word(g, F(u, 7))) >> 4);
            if (a) {
                x = 0x8981;
                if (push(&st, 0x6D42)) return STEP_UNPORTED;
                at = 0x6E17;
                break;
            }
            /* fall through */
        case 0x6D42:
            a = rom(g, word(g, F(u, 7))) & 0x0F;
            if (a) {
                x = 0x89B2;
                if (push(&st, 0x6D4F)) return STEP_UNPORTED;
                at = 0x6E17;
                break;
            }
            /* fall through */
        case 0x6D4F:
            x = word(g, F(u, 7));
            a = (uint8_t)(rom(g, F(x, 1)) >> 4);
            if (a) {
                x = 0x89E3;
                if (push(&st, 0x6D5F)) return STEP_UNPORTED;
                at = 0x6E17;
                break;
            }
            /* fall through */
        case 0x6D5F:
            if (peek(g, 0xA090)) poke(g, 0xA08F, rom(g, F(word(g, F(u, 7)), 1)) & 0x0F);
            a = peek(g, F(u, 0x1C));
            if (a) poke(g, F(u, 0x1C), (uint8_t)(a - 1));
            a = rom(g, (uint16_t)(0x6DFE + (int8_t)a));
            poke(g, F(u, 0x1B), a);
            a = (uint8_t)((uint8_t)(a + 1) << 1);
            set_word(g, 0xA0AD, rom_word(g, (uint16_t)(0x6DE2 + (int8_t)a)));
            poke(g, 0xA0AB, 0);
            poke(g, 0xA0AC, 0);
            poke(g, F(u, 0xB), 0x70);
            return task_sleep(g, 8, MANAGER);

        /* ---- the messages ---------------------------------------------------- */
        case 0x6E53: /* shown until the tasks after this one are gone */
            if (!st.n) return STEP_UNPORTED;
            set_word(g, F(u, 0xF), st.s[--st.n]);
            /* fall through */
        case 0x6E57:
            set_word(g, F(u, 0x11), 0x6E59);
            at = 0x6E78;
            break;
        case 0x6E59:
            if (peek(g, 0xA090) && word(g, u)) {
                at = 0x6E57;
                break;
            }
            at = word(g, F(u, 0xF));
            break;
        case 0x6E64: /* shown a times */
            if (!st.n) return STEP_UNPORTED;
            set_word(g, F(u, 0xF), st.s[--st.n]);
            poke(g, F(u, 0xB), a);
            /* fall through */
        case 0x6E6A:
            set_word(g, F(u, 0x11), 0x6E6C);
            at = 0x6E78;
            break;
        case 0x6E6C: {
            uint8_t n = (uint8_t)(peek(g, F(u, 0xB)) - 1);
            poke(g, F(u, 0xB), n);
            at = n ? 0x6E6A : word(g, F(u, 0xF));
            break;
        }
        case 0x6E78: /* $6E73: the messages again, one a frame */
            if (!peek(g, 0xA090)) {
                at = word(g, F(u, 0x11));
                break;
            }
            poke(g, F(u, 0x1E), 0x23);
            if (peek(g, F(u, 0x1F))) wave_number(g, u, 1);
            return task_sleep(g, 1, MESSAGES_ON);
        case MESSAGES_ON:
            a = peek(g, F(u, 0x1E));
            if (a > 0x33) {
                at = word(g, F(u, 0x11));
                break;
            }
            poke(g, F(u, 0x1E), (uint8_t)(a + 4));
            slot_show(g, (uint16_t)(u + (int8_t)a));
            return task_sleep(g, 1, MESSAGES_ON);

        /* ---- a platform comes or goes ($6EF0: x its record, a the change, b the new) */
        case 0x6EF0: {
            uint8_t mask = rom(g, x);
            if (!(a & mask)) goto ret;
            uint16_t flag = rom_word(g, F(x, 5));
            if (b & mask) { /* $6EFA: it burns away */
                if (flag) poke(g, flag, 0xFF);
                uint8_t top = rom(g, F(x, 1));
                if (top) {
                    and_bytes(g, 0xAFE2, 0x160, (uint8_t)~top);
                    uint8_t side = (uint8_t)~rom(g, F(x, 2));
                    poke(g, 0xA04B, (uint8_t)(side & peek(g, 0xA04B)));
                    and_bytes(g, 0xAE82, 0x160, side);
                }
                game_sound(g, 0xEF6A);
                y = task_create(g, u, 0x81A0, 0x27, 0xFF);
                set_word(g, F(y, 0xE), rom_word(g, rom_word(g, F(x, 3))));
                goto ret;
            }
            /* $6F40: it is there again: its bits from the templates, a frame's wait */
            if (flag) poke(g, flag, 0);
            map_put(g, 0xAFE2, 0xB142, 0xEB98, rom(g, F(x, 1)));
            uint8_t side = rom(g, F(x, 2));
            poke(g, 0xA04B, (uint8_t)(side | peek(g, 0xA04B)));
            map_put(g, 0xAE82, 0xAFE2, 0xE937, side);
            set_word(g, F(u, 0x15), (uint16_t)(a << 8 | b));
            if (!st.n) return STEP_UNPORTED;
            set_word(g, F(u, 0xF), st.s[--st.n]);
            return task_sleep(g, 1, PLATFORM_ON);
        }
        case PLATFORM_ON: {
            uint16_t d = word(g, F(u, 0x15));
            a = (uint8_t)(d >> 8);
            b = (uint8_t)d;
            at = word(g, F(u, 0xF));
            break;
        }

        /* ---- the enemies: a count (a) of the table at x, one every $3D frames */
        case 0x6E17:
            poke(g, F(u, 0xB), a);
            if (!st.n) return STEP_UNPORTED;
            set_word(g, F(u, 0x11), st.s[--st.n]);
            set_word(g, F(u, 0xD), x);
            return task_sleep(g, 0x3D, ENEMIES_ON);
        case ENEMIES_ON: {
            y = task_create(g, u, 0x8B0E, 0x0F, 0xFF);
            poke(g, 0xA0A1, (uint8_t)(peek(g, 0xA0A1) + 1));
            poke(g, 0xA0AA, (uint8_t)(peek(g, 0xA0AA) + 1));
            set_word(g, F(y, 0xD), word(g, F(u, 0xD)));
            set_word(g, F(y, 0x18), rom_word(g, 0x001A));
            poke(g, F(y, 0x33), 4);
            poke(g, F(y, 0x34), 0);
            if (peek(g, 0xA09F)) poke(g, 0xA0B0, 2);
            uint8_t n = (uint8_t)(peek(g, F(u, 0xB)) - 1);
            poke(g, F(u, 0xB), n);
            if (n) return task_sleep(g, 0x3D, ENEMIES_ON);
            at = word(g, F(u, 0x11));
            break;
        }

        /* ---- the wave kinds ($71BC) and their endings (+$13) ----------------- */
        case 0x71C8: goto ret;
        case 0x71C9: /* the first wave's */
            message(g, u, 0x6FE7);
            a = 0x0F;
            if (push(&st, 0x71D4)) return STEP_UNPORTED;
            at = 0x6E64;
            break;
        case 0x71D4:
            message(g, u, 0x6FEB);
            a = 0x0F;
            at = 0x6D2A;
            break;
        case 0x71DF: /* the pterodactyls' */
            message(g, u, 0x702B);
            a = 0x1E;
            if (push(&st, 0x71EA)) return STEP_UNPORTED;
            at = 0x6E64;
            break;
        case 0x71EA: if (push(&st, 0x71ED)) return STEP_UNPORTED; at = 0x6E53; break;
        case 0x71ED:
            y = task_create(g, u, PTEROS, 0x17, 0xFF);
            poke(g, F(y, 0x2D), rom(g, F(word(g, F(u, 7)), 2)));
            at = 0x6D30;
            break;
        case 0x7218: /* the team wave's */
            if (!peek(g, 0xA05C) || !peek(g, 0xA052)) {
                at = 0x7264;
                break;
            }
            set_word(g, F(u, 0x13), 0x7236);
            poke(g, 0xA0A3, 0);
            poke(g, 0xA0A4, 0);
            message(g, u, 0x6FEF);
            message(g, u, 0x6FF3);
            goto ret;
        case 0x7236:
            if (peek(g, 0xA0A3) | peek(g, 0xA0A4)) {
                x = 0x6FF7;
            } else {
                if (peek(g, 0xA052)) score_add(g, 0x88EE, 0x30, 0);
                if (peek(g, 0xA05C)) score_add(g, 0x891F, 0x30, 0);
                x = 0x6FFB;
            }
            /* fall through */
        case 0x725C:
            message(g, u, x);
            a = 0x14;
            at = 0x6E64;
            break;
        case 0x7264: /* the survival wave's */
            set_word(g, F(u, 0x13), 0x7274);
            poke(g, 0xA0A5, 0);
            poke(g, 0xA0A6, 0);
            message(g, u, 0x6FFF);
            goto ret;
        case 0x7274:
            x = 0x7003;
            if (peek(g, 0xA052)) {
                if (peek(g, 0xA0A5)) { at = 0x725C; break; }
                score_add(g, 0x88EE, 0x30, 0);
                x = 0x7007;
                at = 0x725C;
                break;
            }
            if (!peek(g, 0xA05C) || peek(g, 0xA0A6)) { at = 0x725C; break; }
            score_add(g, 0x891F, 0x30, 0);
            x = 0x700B;
            at = 0x725C;
            break;
        case 0x72A1: /* the gladiator wave's */
            if (!peek(g, 0xA052) || !peek(g, 0xA05C)) goto ret;
            set_word(g, F(u, 0x13), 0x72D2);
            poke(g, 0xA0A3, 0xFF);
            poke(g, 0xA0A4, 0xFF);
            message(g, u, 0x700F);
            a = 0x0F;
            if (push(&st, 0x72C0)) return STEP_UNPORTED;
            at = 0x6E64;
            break;
        case 0x72C0:
            message(g, u, 0x7013);
            message(g, u, 0x7017);
            a = 0x0F;
            at = 0x6D2A;
            break;
        case 0x72D2:
            if (!(peek(g, 0xA0A3) & 0x80)) x = 0x7023;
            else x = peek(g, 0xA0A4) & 0x80 ? 0x701B : 0x701F;
            message(g, u, x);
            poke(g, 0xA0A3, 0);
            poke(g, 0xA0A4, 0);
            a = 0x14;
            at = 0x6E64;
            break;
        case 0x72F1: /* the egg wave's */
            message(g, u, 0x7027);
            a = 0x1E;
            if (push(&st, 0x72FC)) return STEP_UNPORTED;
            at = 0x6E64;
            break;
        case 0x72FC: if (push(&st, 0x72FF)) return STEP_UNPORTED; at = 0x6E53; break;
        case 0x72FF: {
            messages_off(g, u);
            x = word(g, F(u, 7));
            y = 0x89E3;
            a = rom(g, x);
            if (!a) {
                a = (uint8_t)(rom(g, F(x, 1)) >> 4);
            } else {
                y = 0x89B2;
                if (!(a & 0x0F)) {
                    y = 0x8981;
                    a >>= 4;
                }
            }
            poke(g, 0xA0A2, a & 0x0F);
            set_word(g, F(u, 0xD), y);
            b = rom(g, 0xB1BA);
            if (peek(g, 0xA05C) && peek(g, 0xA052)) b >>= 1;
            poke(g, F(u, 0x1A), b);
            poke(g, 0xA08F, rom(g, F(x, 1)) & 0x0F);
            for (uint16_t c = 0xB142; c < 0xB18D; c++) poke(g, c, 0);
            poke(g, F(u, 0x1D), 2);
            poke(g, F(u, 0x19), 6);
        }
            /* fall through */
        case 0x7350: { /* six eggs by the rows' counts */
            y = 0x7484;
            a = (uint8_t)((game_random(g, NULL) * 0x0C) >> 8);
            while (peek(g, (uint16_t)(0xB187 + (int8_t)a))) {
                a--;
                if (a & 0x80) a = 5;
            }
            poke(g, (uint16_t)(0xB187 + (int8_t)a), (uint8_t)(peek(g, (uint16_t)(0xB187 + (int8_t)a)) + 1));
            y = (uint16_t)(y + (int8_t)a);
            int carry;
            a = game_random(g, &carry);
            a = (uint8_t)(a << 1 | carry);
            b = (uint8_t)(rom(g, y) - rom(g, (uint16_t)(y - 1)));
            a = (uint8_t)(((a * b) >> 8) + rom(g, (uint16_t)(y - 1)));
            poke(g, (uint16_t)(0xB142 + (int8_t)a), (uint8_t)(peek(g, (uint16_t)(0xB142 + (int8_t)a)) + 1));
            if (push(&st, 0x737F)) return STEP_UNPORTED;
            at = 0x7417;
            break;
        }
        case 0x737F:
            poke(g, F(u, 0x19), (uint8_t)(peek(g, F(u, 0x19)) - 1));
            if (peek(g, F(u, 0x19))) {
                at = 0x7350;
                break;
            }
            poke(g, F(u, 0x19), 6);
            /* fall through */
        case 0x7389: /* six more anywhere free */
            a = (uint8_t)((game_random(g, NULL) * 0x8A) >> 8);
            while (peek(g, (uint16_t)(0xB142 + (int8_t)a))) {
                a++;
                if (a >= 0x45) a = 0;
            }
            poke(g, (uint16_t)(0xB142 + (int8_t)a), (uint8_t)(peek(g, (uint16_t)(0xB142 + (int8_t)a)) + 1));
            if (push(&st, 0x73A3)) return STEP_UNPORTED;
            at = 0x7417;
            break;
        case 0x73A3:
            poke(g, F(u, 0x19), (uint8_t)(peek(g, F(u, 0x19)) - 1));
            at = peek(g, F(u, 0x19)) ? 0x7389 : 0x6D5F;
            break;
        case 0x7417:
            egg(g, u, a);
            if (!st.n) return STEP_UNPORTED;
            set_word(g, F(u, 0xF), st.s[--st.n]);
            return task_sleep(g, 1, EGGS_ON);
        case EGGS_ON: at = word(g, F(u, 0xF)); break;
        default:
            return STEP_UNPORTED;
        ret:
            if (!st.n) return STEP_UNPORTED;
            at = st.s[--st.n];
            break;
        }
    }
    return STEP_UNPORTED;
}

StepResult waves_step(Game *g, uint16_t u, uint16_t at)
{
    switch (at) {
    case PTEROS: return task_sleep(g, 0x41, PTEROS_ON);
    case PTEROS_ON: { /* $7205: one more every $41 frames; the last becomes one itself */
        uint8_t n = (uint8_t)(peek(g, F(u, 0x2D)) - 1);
        poke(g, F(u, 0x2D), n);
        if ((int8_t)(uint8_t)(n + 1) <= 1) return ptero_step(g, u, 0x68A5);
        task_create(g, u, 0x68A5, 0x17, 0xFF);
        return task_sleep(g, 0x41, PTEROS_ON);
    }
    }
    return run(g, u, at);
}
