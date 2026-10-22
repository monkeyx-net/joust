#include "sim/start.h"

#include "sim/attract.h"
#include "sim/score.h"
#include "sim/spawn.h"
#include "sim/tasks.h"
#include "sim/text.h"

#include <stddef.h>

static uint16_t F(uint16_t u, int off) { return (uint16_t)(u + off); }
static uint8_t peek(const Game *g, uint16_t a) { return game_peek(g, a); }
static void poke(Game *g, uint16_t a, uint8_t v) { game_poke(g, a, v); }
static uint16_t word(const Game *g, uint16_t a) { return game_word(g, a); }
static void set_word(Game *g, uint16_t a, uint16_t v) { game_set_word(g, a, v); }
static uint8_t rom(const Game *g, uint16_t a) { return game_read(g, a); }
static uint16_t rom_word(const Game *g, uint16_t a) { return game_read_word(g, a); }

int start_ported(uint16_t at)
{
    switch (at) {
    case 0x5ED0: case 0x5F11: case 0x5F69: case 0x5F75: case 0x5F91: case 0x5F9B: case 0x5FA5:
    case 0x61F0: case 0x61FB: case 0x623C: case 0x6267: case 0x627C: case 0x62AD: case 0x62DA: case 0x62DF:
    case 0x6301: case 0x6306: case 0x6327: case 0x6359: case 0x6364: case 0x63E5: case 0x649A: case 0x64B2:
    case 0x64E0: case 0x654D: case 0x6593: case 0x65AE: case 0x65D6: case 0x65E3:
        return 1;
    }
    return 0;
}

/* $40BC (through $3B2E): video RAM ($0000-$9FFF) cleared, from the top down */
void start_clear_screen(Game *g)
{
    for (uint16_t a = 0xA000; a-- > 0;) poke(g, a, 0);
}

/* $E4B2 (through $E015): the palette's copy <$0F-<$1E from x */
void start_palette(Game *g, uint16_t x)
{
    for (int i = 0; i < 16; i++) poke(g, (uint16_t)(0xA00F + i), rom(g, (uint16_t)(x + i)));
}

/* $8793: a blit of the picture at x (width, height, data) to d */
static void picture(Game *g, uint8_t ctrl, uint8_t solid, uint16_t x, uint16_t d)
{
    game_blit(g, ctrl, solid, (uint16_t)(x + 2), d, (uint8_t)(rom(g, x) ^ 4), (uint8_t)(rom(g, (uint16_t)(x + 1)) ^ 4));
}

void start_credits(Game *g, uint8_t ctrl, uint8_t solid)
{
    picture(g, ctrl, solid, 0x6357, 0x37E2);
    uint16_t x = text_message_small(g, 0x50, 0x22, 0x38E3);
    uint8_t a = peek(g, 0xA0F2);
    if (!(a & 0xF0)) a |= 0xF0;
    text_number_small(g, a, 0x22, x);
}

/* $E60A (through $E03A): the platforms' pictures from the bit stream at
   [$0036] into video RAM (with the ROM unselected for the reads) */
static uint8_t bits(Game *g, uint16_t *u, uint8_t *b, uint8_t n)
{
    uint8_t a = 1;
    while (n--) {
        int c = *b >> 7;
        *b = (uint8_t)(*b << 1);
        if (!*b) {
            uint8_t v = rom(g, (*u)++);
            c = v >> 7;
            *b = (uint8_t)((v << 1) + 1);
        }
        a = (uint8_t)(a << 1 | c);
    }
    return a;
}

static void platform_pictures(Game *g)
{
    uint16_t x0 = 0x0036, x = x0, y = 0x00D3, u = rom_word(g, 0x0036);
    uint8_t buf = 0;
    for (;;) {
        uint8_t b = buf, a = 0;
        int c;
        do { /* the run's length in unary */
            a++;
            c = b >> 7;
            b = (uint8_t)(b << 1);
            if (!b) {
                uint8_t v = rom(g, u++);
                c = v >> 7;
                b = (uint8_t)((v << 1) + 1);
            }
        } while (!c);
        uint8_t n = (uint8_t)(bits(g, &u, &b, a) - 2);
        a = bits(g, &u, &b, 3);
        buf = b;
        if (!n) {
            if (!(a & 7)) return;
            x = x0;
            y++;
            continue;
        }
        a &= 7;
        if (a) a = (uint8_t)(a + 7);
        game_select_rom(g, peek(g, 0xBFFF) & 0xFE);
        for (; n; n--) {
            uint16_t at = (uint16_t)(((x >> 1) & 0xFF) << 8 | (y & 0xFF));
            uint8_t v = x & 1 ? a : (uint8_t)(a << 4);
            poke(g, at, (uint8_t)(v | game_read(g, at)));
            x++;
        }
        game_select_rom(g, peek(g, 0xBFFF) | 0x01);
    }
}

/* $4B2B-$4B5B: the text block a ([$5D7B + 2a]: [screen address][colour]
   [message], bit 7 on the last), shown or erased (<$F6), in the small or
   large font */
static void block(Game *g, uint8_t a, int erase, int large)
{
    poke(g, 0xA0F6, erase ? (uint8_t)(peek(g, 0xA0F6) + 1) : 0);
    uint16_t y = rom_word(g, (uint16_t)(0x5D7B + 2 * (int8_t)a));
    for (;;) {
        uint16_t x = rom_word(g, y);
        uint8_t colour = rom(g, (uint16_t)(y + 2));
        y = (uint16_t)(y + 3);
        if (peek(g, 0xA0F6)) colour = 0;
        uint8_t msg = rom(g, y) & 0x7F;
        if (large) text_message(g, msg, colour, x);
        else text_message_small(g, msg, colour, x);
        if (rom(g, y++) & 0x80) return;
    }
}

/* $5FBC: a demo page's block a shown (0 none; bit 7 the large font) */
static void show(Game *g, uint8_t a)
{
    if (!a) return;
    if (a & 0x80) block(g, a & 0x7F, 0, 1);
    else block(g, a, 0, 0);
}

/* $60A3: a demo rider (table x) on its own task ($8EC8, type $8F) after
   the second player's death task */
static uint16_t demo_rider(Game *g, uint16_t x)
{
    uint16_t u = word(g, 0xA075);
    do u = word(g, u);
    while (peek(g, F(u, 2)) != 0x20);
    uint16_t y = task_create(g, u, 0x8EC8, 0x8F, 0xFF);
    for (int b = 7; b < 0x36; b++) poke(g, F(y, b), 0);
    set_word(g, F(y, 0xD), x);
    poke(g, 0xA0A1, (uint8_t)(peek(g, 0xA0A1) + 1));
    poke(g, 0xA0AA, (uint8_t)(peek(g, 0xA0AA) + 1));
    set_word(g, F(y, 0x18), rom_word(g, 0x001A));
    set_word(g, F(y, 0x1A), rom_word(g, rom_word(g, (uint16_t)(x + 0x0A))));
    poke(g, F(y, 0x33), 4);
    return y;
}

/* $6154: a demo enemy entering at the left, walking +$2B frames */
static void demo_walker(Game *g, uint16_t x, uint16_t walk)
{
    uint16_t y = demo_rider(g, x);
    set_word(g, F(y, 0x2B), walk);
    set_word(g, F(y, 7), 0x0008);
    set_word(g, F(y, 0x29), 0x0008);
    poke(g, F(y, 0xB), 0xD2);
    poke(g, F(y, 0x27), 0xD2);
    poke(g, F(y, 0x28), 0xD2);
    set_word(g, F(y, 0x24), 0x6170);
}

/* the demo pages' routines ($5FCA's records) */
static int demo_routine(Game *g, uint16_t r)
{
    switch (r) {
    case 0x5FC9: return 0;
    case 0x6010: {
        uint16_t x = word(g, 0xA075);
        set_word(g, F(x, 0x24), 0x6021);
        poke(g, F(x, 0x2D), 0xA0);
        poke(g, F(x, 0x33), 0xA0);
        return 0;
    }
    case 0x6074: {
        uint16_t y = demo_rider(g, 0x8981);
        set_word(g, F(y, 0x24), 0x7A83);
        set_word(g, F(y, 7), 0x0123);
        set_word(g, F(y, 0x29), 0x0123);
        set_word(g, F(y, 0xA), 0x0081);
        poke(g, F(y, 0x27), 0x81);
        poke(g, F(y, 0x28), 0x81);
        poke(g, F(y, 0xF), 0x81);
        uint16_t x = word(g, 0xA073);
        set_word(g, F(x, 0x24), 0x61B9);
        poke(g, F(x, 0x2D), 0x60);
        return 0;
    }
    case 0x60DD: set_word(g, F(word(g, 0xA075), 0x24), 0x60E6); return 0;
    case 0x6123:
        text_message(g, 0xF9, 0x44, 0x74DC);
        text_message(g, 0xFA, 0x44, 0x76E3);
        return 0;
    case 0x6135: demo_walker(g, 0x8981, 0x0178); return 0;
    case 0x6140: demo_walker(g, 0x89B2, 0x00F8); return 0;
    case 0x614B: demo_walker(g, 0x89E3, 0x0080); return 0;
    case 0x6192: /* the enemies fly off */
        for (uint16_t u = game_word(g, TASK_CURRENT); (u = word(g, u)) != 0;)
            if (peek(g, F(u, 2)) == 0x8F) set_word(g, F(u, 0x24), 0xEEC2);
        return 0;
    case 0x61A7: { /* a pterodactyl after the last task */
        uint16_t u = game_word(g, TASK_CURRENT);
        do u = word(g, u);
        while (word(g, u));
        task_create(g, u, 0x68A5, 0x17, 0xFF);
        return 0;
    }
    }
    return -1;
}

/* $6227: message $00 at $3090 (game over), in colour $11 */
static void game_over_message(Game *g) { text_message_small(g, 0x00, 0x11, 0x3090); }

/* The shell's task from `at` until it sleeps: the ROM's blocks by address. */
static StepResult run(Game *g, uint16_t u, uint16_t at)
{
    uint8_t a = 0, b = 0;
    uint16_t x = 0, y = 0;
    for (int guard = 0; guard < 256; guard++) {
        switch (at) {
        /* ---- the demo ($5EF8) ------------------------------------------------ */
        case START_DEMO: /* $5EEB, after the title page */
            poke(g, 0xA090, 0x7F);
            poke(g, 0xA077, 3);
            poke(g, 0xA060, 3);
            return task_sleep(g, 4, 0x63E5);
        case 0x5EF8:
            poke(g, 0xA08E, 0);
            poke(g, 0xA08F, 0);
            for (x = 0xAE82; x < 0xAFE2; x += 2) {
                uint16_t d = word(g, x);
                set_word(g, x, d & 0x8080);
            }
            return task_sleep(g, 1, 0x5F11);
        case 0x5F11:
            for (x = 0xAFE2; x < 0xB142; x++) poke(g, x, peek(g, x) & 0x20);
            set_word(g, 0xA097, 0);
            set_word(g, F(u, 0x18), 0);
            set_word(g, F(u, 0x1A), 0);
            poke(g, 0xA06E, 0xFF);
            poke(g, 0xA06F, 0xFF);
            poke(g, 0xA070, 0xFF);
            start_credits(g, 0x12, 0x00);
            set_word(g, F(u, 0x2B), 0x5FC5);
            /* fall through */
        case 0x5F3B: /* the next page */
            x = (uint16_t)(word(g, F(u, 0x2B)) + 5);
            set_word(g, F(u, 0x2B), x);
            if (x >= 0x6010) {
                at = 0x626B;
                break;
            }
            a = rom(g, F(x, 4));
            if (a) { /* its block off the screen */
                uint16_t s = F(u, 0x18);
                while (peek(g, s++) != a) continue;
                poke(g, --s, 0);
                if (a & 0x80) block(g, a & 0x7F, 1, 1);
                else block(g, a, 1, 0);
            }
            return task_sleep(g, 8, 0x5F69);
        case 0x5F69:
            x = word(g, F(u, 0x2B));
            a = rom(g, F(x, 2));
            if (a) { /* $5FB1: into the first free slot, shown */
                uint16_t s = F(u, 0x18);
                while (peek(g, s++)) continue;
                poke(g, --s, a);
                show(g, a);
            }
            return task_sleep(g, 8, 0x5F75);
        case 0x5F75:
            x = word(g, F(u, 0x2B));
            if (demo_routine(g, rom_word(g, x))) return STEP_UNPORTED;
            poke(g, F(u, 0x2D), rom(g, F(word(g, F(u, 0x2B)), 3)));
            /* fall through */
        case 0x5F82:
            poke(g, F(u, 0x33), 0x0A);
            /* fall through */
        case 0x5F87:
            show(g, peek(g, F(u, 0x18)));
            return task_sleep(g, 2, 0x5F91);
        case 0x5F91:
            show(g, peek(g, F(u, 0x19)));
            return task_sleep(g, 2, 0x5F9B);
        case 0x5F9B:
            show(g, peek(g, F(u, 0x1A)));
            return task_sleep(g, 2, 0x5FA5);
        case 0x5FA5:
            poke(g, F(u, 0x33), (uint8_t)(peek(g, F(u, 0x33)) - 1));
            if (peek(g, F(u, 0x33))) { at = 0x5F87; break; }
            poke(g, F(u, 0x2D), (uint8_t)(peek(g, F(u, 0x2D)) - 1));
            at = peek(g, F(u, 0x2D)) ? 0x5F82 : 0x5F3B;
            break;

        /* ---- game over ($61F0) ----------------------------------------------- */
        case 0x61F0:
            poke(g, 0xA0B3, 0x80);
            poke(g, 0xA090, 0);
            return task_sleep(g, 8, 0x61FB);
        case 0x61FB:
            task_create(g, u, 0x6359, 0x48, 0x00);
            start_credits(g, 0x12, 0x00);
            poke(g, 0xA08F, 0);
            for (x = word(g, word(g, TASK_HEAD)); (x = word(g, x)) != 0;) { /* the enemies to their first brains */
                a = peek(g, F(x, 2));
                if (!(a & 0x80) || a == 0x97) continue;
                set_word(g, F(x, 0x24), rom_word(g, word(g, F(x, 0xD))));
            }
            poke(g, F(u, 0xD), 0x0B);
            /* fall through */
        case 0x6234:
            game_over_message(g);
            return task_sleep(g, 8, 0x623C);
        case 0x623C:
            poke(g, F(u, 0xD), (uint8_t)(peek(g, F(u, 0xD)) - 1));
            if (peek(g, F(u, 0xD))) { at = 0x6234; break; }
            for (x = 0xA0EA; x < 0xA0F2; x++) poke(g, x, 0);
            return STEP_UNPORTED; /* $3B61: the high score entry (the system ROM) */

        /* ---- the attract mode's start ---------------------------------------- */
        case 0x5ED0: at = 0x627F; break;
        case 0x6267:
            poke(g, F(u, 0xD), (uint8_t)(peek(g, F(u, 0xD)) - 1));
            if (peek(g, F(u, 0xD))) {
                game_over_message(g);
                return task_sleep(g, 8, 0x6267);
            }
            /* fall through */
        case 0x626B:
            task_kill_type(g, 0, 0);
            /* fall through */
        case 0x6271:
            start_palette(g, 0x62F1);
            return task_sleep(g, 4, 0x627C);
        case 0x627C:
            start_clear_screen(g);
            /* fall through */
        case 0x627F:
            poke(g, 0xA090, 0);
            poke(g, F(u, 2), 0x31);
            task_create(g, u, 0x6359, 0x48, 0x00);
            if (!(rom(g, 0xCC05) & 1)) {
                at = 0x62BA;
                break;
            }
            set_word(g, F(u, 0xD), 0x0384);
            poke(g, F(u, 0x10), 0);
            start_palette(g, 0x62E9);
            return STEP_UNPORTED; /* $3B43: the high score table (the system ROM) */
        case 0x62AD:
            poke(g, 0xA011, (uint8_t)(peek(g, 0xA011) + 1));
            if (word(g, F(u, 0xD))) {
                set_word(g, F(u, 0xD), (uint16_t)(word(g, F(u, 0xD)) - 1));
                return task_sleep(g, 1, 0x62AD);
            }
            /* fall through */
        case 0x62BA:
            if (game_controls(g) == 0x0201) { /* player 1 right, player 2 left */
                poke(g, F(u, 0x10), 0x3C);
                return task_sleep(g, 1, 0x62AD);
            }
            b = peek(g, F(u, 0x10));
            poke(g, F(u, 0x10), (uint8_t)(b - 1));
            if ((int8_t)b > 1) return task_sleep(g, 1, 0x62AD);
            task_create(g, u, 0x62DA, 0x49, 0x00);
            return attract_step(g, u, ATTRACT_PAGES);
        case 0x62DA: return task_sleep(g, 5, 0x62DF);
        case 0x62DF:
            if (game_controls(g) != 0x0201) return task_sleep(g, 5, 0x62DF);
            at = 0x626B;
            break;

        /* ---- a coin: its screen ($6301) -------------------------------------- */
        case 0x6301: return task_sleep(g, 4, 0x6306);
        case 0x6306:
            poke(g, F(u, 0xD), 0x1E);
            start_clear_screen(g);
            start_palette(g, rom_word(g, 0xE033));
            start_credits(g, 0x12, 0x00);
            block(g, (uint8_t)(peek(g, 0xA0F2) == 1 ? 0x19 : 0x1A), 0, 0);
            return task_sleep(g, 0x3C, 0x6327);
        case 0x6327:
            poke(g, F(u, 0xD), (uint8_t)(peek(g, F(u, 0xD)) - 1));
            if (peek(g, F(u, 0xD))) return task_sleep(g, 0x3C, 0x6327);
            at = 0x6271;
            break;

        /* ---- the start buttons ($6359) ---------------------------------------- */
        case 0x6359:
            task_kill_type(g, 0x48, 0xFF);
            return task_sleep(g, 1, 0x6364);
        case 0x6364:
            if ((int8_t)peek(g, 0xA0B3) > 0) { /* a coin: its screen */
                poke(g, 0xA0B3, 0);
                task_kill_type(g, 0, 0);
                task_create(g, u, 0x6301, 0x30, 0x00);
            }
            b = rom(g, 0xC804) & 0x30;
            if (!b) return task_sleep(g, 1, 0x6364);
            a = game_cmos_byte(g, 0xCC06);
            if (a != 9) { /* not free play: the credits */
                a = peek(g, 0xA0F2);
                if (!a || (a < 2 && (b & 0x10))) return task_sleep(g, 1, 0x6364);
                int c, h;
                if (b & 0x10) {
                    a = game_add(a, 0x99, 0, &c, &h);
                    a = game_daa(a, &c, h);
                }
                a = game_add(a, 0x99, 0, &c, &h);
                a = game_daa(a, &c, h);
                poke(g, 0xA0F2, a);
                poke(g, 0xCD01, a); /* $40FA: the CMOS's copy */
                poke(g, 0xCD00, (uint8_t)(a >> 4));
            }
            a = 8;
            poke(g, 0xA060, 0);
            if (b & 0x10) {
                a = 9;
                poke(g, 0xA060, (uint8_t)(peek(g, 0xA060) + 1));
                game_audit(g, 0x0A);
            }
            game_audit(g, a);
            game_audit(g, 0x0A);
            {
                b = game_cmos_byte(g, 0xCC02); /* lives, BCD ($437B) */
                uint8_t n = 0;
                while (b >= 0x10) {
                    n = (uint8_t)(n + 0x0A);
                    b = (uint8_t)(b - 0x10);
                }
                poke(g, 0xA077, (uint8_t)(n + b));
            }
            poke(g, 0xA090, 0);
            task_kill_type(g, 0, 0);
            poke(g, F(u, 2), 0x30);
            game_sound(g, 0xEF1E);
            return task_sleep(g, 4, 0x63E5);

        /* ---- a game's set-up ($63E5) ----------------------------------------- */
        case 0x63E5: {
            start_clear_screen(g);
            start_palette(g, rom_word(g, 0xE033));
            uint16_t s = 0x719B;
            x = rom_word(g, s);
            uint16_t d = rom_word(g, (uint16_t)(s + 2));
            s = (uint16_t)(s + 4);
            a = (uint8_t)(d >> 8);
            b = (uint8_t)d;
            do { /* a line in the ROM's own code */
                a = (uint8_t)((uint8_t)(a - 0x30) ^ 0x5A);
                x = text_char_small(g, a, b, x);
                a = rom(g, s++);
            } while (a);
            for (x = 0xA04C; x < 0xA060; x++) poke(g, x, 0);
            poke(g, 0xA036, 0);
            poke(g, 0xA037, 0);
            poke(g, 0xA0AA, 0);
            d = (uint16_t)(game_cmos_byte(g, 0xCC00) << 4); /* the extra life interval */
            set_word(g, 0xA097, d);
            set_word(g, 0xA054, d);
            set_word(g, 0xA05E, d);
            a = game_cmos_byte(g, 0xCC14);
            uint8_t level = (uint8_t)((a > 3) + (a > 6));
            x = 0xDE11;
            y = 0xB1AD;
            do { /* the settings by the difficulty */
                set_word(g, y, rom_word(g, (uint16_t)(x + (int8_t)(uint8_t)(level << 1))));
                poke(g, F(y, 2), 2);
                x = (uint16_t)(x + 0x0E);
                y = (uint16_t)(y + 3);
            } while (x < 0xDF99);
            poke(g, 0xA048, 4);
            poke(g, 0xA09F, 3);
            poke(g, 0xA0A0, 1);
            poke(g, 0xA0B0, 1);
            poke(g, 0xA0A7, 0x5A);
            poke(g, 0xA096, 0);
            poke(g, 0xA09B, 0xEA);
            poke(g, 0xA03C, 0x0D);
            set_word(g, 0xA06E, 0);
            set_word(g, 0xA070, 0);
            set_word(g, 0xA073, 0);
            set_word(g, 0xA075, 0);
            set_word(g, 0xA038, 0);
            set_word(g, 0xA03A, 0);
            poke(g, 0xA0AF, 0);
            poke(g, 0xA08E, 0);
            poke(g, 0xA0A1, 0);
            for (x = 0xAE82; x < 0xAFE2; x += 2) set_word(g, x, rom_word(g, (uint16_t)(x + 0x3AB5)));
            return task_sleep(g, 1, 0x649A);
        }
        case 0x649A:
            for (x = 0xAFE2; x < 0xB142; x++) poke(g, x, rom(g, (uint16_t)(x + 0x3BB6)) | 0x20);
            platform_pictures(g);
            return task_sleep(g, 1, 0x64B2);
        case 0x64B2:
            for (y = 0x6608; y <= 0x6610; y += 8) {
                uint16_t node = spawn_pad_draw(g, y);
                set_word(g, node, rom_word(g, y));
            }
            if ((int8_t)peek(g, 0xA090) > 0) {
                at = 0x64EB;
                break;
            }
            for (uint16_t p = 0; p < 0x0E; p += 2) spawn_pad_draw(g, rom_word(g, p));
            return task_sleep(g, 2, 0x64E0);
        case 0x64E0:
            poke(g, 0xA090, (uint8_t)(peek(g, 0xA090) - 1));
            task_create(g, u, 0x6BC3, 0x11, 0xFF);
            /* fall through */
        case 0x64EB:
            task_create(g, u, 0x837A, 0x20, 0xFF);
            y = task_create(g, u, 0x8D23, 0x09, 0x00);
            set_word(g, F(y, 0x18), rom_word(g, 0x0018));
            poke(g, F(y, 0xF), 0);
            set_word(g, F(y, 7), 0x0064);
            x = (int8_t)peek(g, 0xA090) > 0 ? 0x888C : 0x88EE;
            set_word(g, F(y, 0xD), x);
            a = peek(g, 0xA077);
            do score_life(g, x); /* $870E */
            while (--a);
            if (!peek(g, 0xA060)) {
                at = 0x654D;
                break;
            }
            y = task_create(g, u, 0x8D23, 0x09, 0x00);
            set_word(g, F(y, 0x18), rom_word(g, 0x001C));
            poke(g, F(y, 0xF), 0xFF);
            set_word(g, F(y, 7), 0x00C8);
            x = (int8_t)peek(g, 0xA090) > 0 ? 0x88BD : 0x891F;
            set_word(g, F(y, 0xD), x);
            a = peek(g, 0xA077);
            do score_life(g, x);
            while (--a);
            return task_sleep(g, 8, 0x654D);
        case 0x654D:
            task_create(g, u, 0x839F, 0x20, 0x00);
            task_create(g, u, rom_word(g, 0xE035), 0x68, 0xFF);
            task_create(g, u, 0xECF8, 0x23, 0xFF);
            y = task_create(g, u, 0xE785, 0x21, 0xFF);
            set_word(g, F(y, 0xD), 0xE71D);
            if (peek(g, 0xA060)) {
                y = task_create(g, u, 0xE785, 0x22, 0xFF);
                set_word(g, F(y, 0xD), 0xE751);
            }
            if ((int8_t)peek(g, 0xA090) > 0) {
                at = 0x5EF8;
                break;
            }
            return task_sleep(g, 2, 0x6593);
        case 0x6593: { /* $6AC7: the program ROM's sum */
            a = 0;
            for (x = rom_word(g, 0x6BC1); x != 0xD000;) a = (uint8_t)(a + rom(g, --x));
            if (a != 0xA1) poke(g, 0xA0B2, (uint8_t)(peek(g, 0xA0B2) + 1));
            task_create(g, u, 0x6B20, 0x6F, 0x00);
            poke(g, F(u, 2), 0xFF);
            poke(g, F(u, 0xD), 0);
            poke(g, F(u, 0xF), 0);
            poke(g, 0xA04B, 0);
            return task_sleep(g, 2, 0x65AE);
        }

        /* ---- the platforms met, drawn again ($65AE) --------------------------- */
        case 0x65AE: {
            uint8_t met = peek(g, 0xA04B);
            if (!met) return task_sleep(g, 2, 0x65AE);
            a = peek(g, F(u, 0xE));
            b = peek(g, F(u, 0xF));
            do { /* the next bit met, round */
                a = (uint8_t)(a + 2);
                b = (uint8_t)(b << 1);
                if (!b) {
                    b = 1;
                    a = 0;
                }
            } while (!(b & met));
            poke(g, F(u, 0xE), a);
            poke(g, F(u, 0xF), b);
            poke(g, 0xA04B, (uint8_t)(met ^ b));
            spawn_pad_draw(g, (uint16_t)(rom_word(g, word(g, F(u, 0xD))) + 6));
            if (!(peek(g, F(u, 0xF)) & 0x80)) return task_sleep(g, 2, 0x65AE);
            return task_sleep(g, 1, 0x65D6);
        }
        case 0x65D6:
            spawn_pad_draw(g, (uint16_t)(rom_word(g, 0x000E) + 0x0C));
            return task_sleep(g, 1, 0x65E3);
        case 0x65E3:
            spawn_pad_draw(g, (uint16_t)(rom_word(g, 0x000E) + 0x12));
            return task_sleep(g, 2, 0x65AE);
        default:
            return STEP_UNPORTED;
        }
    }
    return STEP_UNPORTED;
}

StepResult start_step(Game *g, uint16_t u, uint16_t at) { return run(g, u, at); }
