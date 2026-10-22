/*
 * ROM routines on chosen inputs against the C ones (label rom):
 *   test_routines <joust.zip|folder>
 * Each case sets RAM and registers on the emulated board's CPU, calls the
 * ROM routine with a return address of its own and steps the CPU until it
 * comes back (no frames run: no interrupts); the C routine runs on a model
 * of the same RAM (sim/game.h), and the results, and all of RAM, must
 * match. Use it where scripted play rarely reaches a path; aim cases at
 * the edges. TRACE=1 runs two cases of each, printing the ROM's path.
 *   - $DC21, the collision data test (sim/collide.c): random row lists
 *     (well formed, and noise for the overflow paths), random offsets;
 *     the carry and where X and Y end must match
 *   - $83D8, a pair's box and data test and its outcome ($8418: who wins,
 *     bounces, an unhorsing with its egg and score, an egg collected):
 *     random task blocks with the riders' and the pterodactyl's real
 *     tables (its type $97), on a game in
 *     progress; it returns with the carry (the walk goes on or moves to
 *     the next pair) or reaches a path not ported (a player's death,
 *     $8279; the SWI at $8428); all of RAM and the CMOS
 *   - the display list's nodes ($E2A1, $E2B8, $E2CF, $E2F9, $E322, $E358,
 *     sim/display.c): random chain tails, free pointer and objects; the
 *     node returned (X) and all of RAM must match
 *   - $D9A3, the platforms around a rider (sim/platform.c), on RAM from a
 *     game in progress (the scripted game's frame 2000, for the wave's
 *     platform maps), the player's block at random x, line, velocity,
 *     speed and push: landing or flying (the Z flag) and all of RAM; the
 *     cases that start the troll's hand ($DA0E) are skipped
 *   - $E6A8 and $E6B1, scoring (sim/score.c): player 1's score block with
 *     random BCD scores, points, extra-life thresholds and intervals
 *     (sometimes none), lives; all of RAM (the icon's blit too) and CMOS
 *   - $80D6, the enemies' target (sim/brains.c): the two players' blocks
 *     at random lines and x (near each other often, for the ties), or
 *     missing, either out (<$A8, <$A9): found (Z clear) and X
 *   - the pterodactyl's brain states ($6640, $6695, $6795, $67B3,
 *     sim/ptero.c) on the game in progress: random position, velocity,
 *     targets, countdowns, push, the player's line, out or missing; all of
 *     RAM, or both off the screen ($6808)
 *   - a riderless mount's brain ($793C, $7A1F, $7A5E, sim/brains.c): near
 *     its knight or not, level or not, on the ground or flying; all of
 *     RAM, and picking it up (to $8D40) or not
 *   - $77BE, points placed (sim/points.c) among two displays near them
 *     on the task list: all of RAM after the step
 *   - the wave task (sim/waves.c) from a wave's end through the next
 *     wave's start, step by step, on every wave record and ending, the
 *     settings' countdowns and the CMOS difficulty at random
 *   - the pterodactyl's task step by step (to the sleep, $E15F) from a
 *     random flight or its death ($6828), through the body's pixels, to
 *     the points; all of RAM after each step
 */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "assets/rom.h"
#include "emu/board.h"
#include "sim/brains.h"
#include "sim/collide.h"
#include "sim/display.h"
#include "sim/game.h"
#include "sim/platform.h"
#include "sim/points.h"
#include "sim/ptero.h"
#include "sim/score.h"

#define CASES 20000
#define RETURN 0x0000      /* where the call returns: stepping stops there */
#define STACK_TOP 0xBEF0
#define LIST_X 0x9800      /* collision data in RAM above $9000 (RAM whatever the ROM select) */
#define LIST_Y 0x9A00
#define TASK_U 0xA300
#define TASK_X 0xA400

static RomSet rom;
static Board board;
static uint8_t data[0x10000];
static Game model;
static int failures;
static uint32_t seed = 12345;

static uint32_t rnd(void)
{
    seed = seed * 1103515245u + 12345u;
    return seed >> 8;
}

static void poke_word(uint16_t a, uint16_t v)
{
    board.ram[a] = (uint8_t)(v >> 8);
    board.ram[(uint16_t)(a + 1)] = (uint8_t)v;
}

/* where the outcome's unported paths begin */
static const uint16_t outcome_stops[] = { 0x8428 };
static uint8_t base[0xC000];      /* RAM of a game in progress (the scripted game's frame 2000) */
static uint16_t base_player;     /* the player's task block in it */
static int stop_at_outcomes;
static int tracing; /* ONLY=n: trace case n of the pair cases */
static int visited_8487;

/* Run the ROM from `pc` until it returns to RETURN or reaches `stop` (0: none). */
static uint16_t call(uint16_t pc, uint16_t stop)
{
    board.cpu.s = STACK_TOP;
    board.ram[--board.cpu.s] = RETURN & 0xFF;
    board.ram[--board.cpu.s] = RETURN >> 8;
    board.cpu.pc = pc;
    board.cpu.cc |= CC_I | CC_F;
    board.cpu.waiting = 0; /* (after frames the CPU may sit in a CWAI or SYNC, a line raised) */
    board.cpu.dp = 0xA0;   /* the game's direct page (a case before may have changed it) */
    board.cpu.irq = board.cpu.firq = board.cpu.nmi = 0;
    for (long n = 0; n < 200000; n++) {
        if (board.cpu.pc == RETURN || (stop && board.cpu.pc == stop)) return board.cpu.pc;
        if (stop_at_outcomes)
            for (size_t k = 0; k < sizeof outcome_stops / sizeof outcome_stops[0]; k++)
                if (board.cpu.pc == outcome_stops[k]) return board.cpu.pc;
        if (board.cpu.pc == 0x8487) visited_8487 = 1;
        if (getenv("TRACE") || tracing) fprintf(stderr, "  %04X D=%04X X=%04X Y=%04X CC=%02X\n", board.cpu.pc, board.cpu.a << 8 | board.cpu.b, board.cpu.x, board.cpu.y, board.cpu.cc);
        m6809_step(&board.cpu);
    }
    return 0xFFFF;
}

/* A list of rows: left and right edges as words, ending in a negative word. */
static void random_list(uint16_t at)
{
    if (rnd() % 4 == 0) { /* noise */
        for (int i = 0; i < 0x80; i++) board.ram[at + i] = (uint8_t)rnd();
        return;
    }
    int rows = 1 + (int)(rnd() % 20), x = (int)(rnd() % 64) - 32;
    for (int r = 0; r < rows; r++) {
        int l = x + (int)(rnd() % 9) - 4, w = (int)(rnd() % 12);
        poke_word((uint16_t)(at + r * 4), (uint16_t)l);
        poke_word((uint16_t)(at + r * 4 + 2), (uint16_t)(l + w));
    }
    poke_word((uint16_t)(at + rows * 4), 0xFFFF);
    poke_word((uint16_t)(at + rows * 4 + 2), 0xFFFF);
}

static void sync_model(void)
{
    board.romsel = 1; /* as the game's tasks run (a case before may have changed it) */
    game_load(&model, board.ram, board.romsel);
    memcpy(model.cmos, board.cmos, sizeof model.cmos);
}

static long first_difference(void)
{
    for (unsigned a = 0; a < 0xBE00; a++)
        if (model.ram[a] != board.ram[a]) return a;
    return -1;
}

static void rows_cases(void)
{
    long hits = 0;
    for (int i = 0; i < (getenv("TRACE") ? 2 : CASES); i++) {
        random_list(LIST_X);
        random_list(LIST_Y);
        uint16_t dx = (uint16_t)(rnd() % 4 ? (int)(rnd() % 64) - 32 : (int)rnd());
        poke_word(0xA034, dx);
        sync_model();
        board.cpu.x = LIST_X;
        board.cpu.y = LIST_Y;
        if (call(0xDC21, 0) != RETURN) {
            if (failures++ < 8) fprintf(stderr, "test_routines: $DC21 case %d did not return\n", i);
            continue;
        }
        uint16_t x = LIST_X, y = LIST_Y;
        int rom_carry = board.cpu.cc & 1, c = collide_rows_at(&model, &x, &y, dx);
        hits += rom_carry;
        if ((rom_carry != c || x != board.cpu.x || y != board.cpu.y) && failures++ < 8)
            fprintf(stderr, "test_routines: $DC21 case %d: ROM %d X %04X Y %04X, C %d X %04X Y %04X\n", i, rom_carry,
                    board.cpu.x, board.cpu.y, c, x, y);
    }
    printf("test_routines: $DC21: %d cases, %ld overlapping\n", CASES, hits);
}

static void random_task(uint16_t t, uint16_t list)
{
    static const uint16_t tables[] = { 0x88EE, 0x8981, 0x89B2, 0x89E3, 0x8A14 };
    for (int i = 0; i < 0x38; i++) board.ram[t + i] = (uint8_t)rnd();
    uint16_t table = tables[rnd() % 5];
    poke_word((uint16_t)(t + 0xD), table);
    if (table == 0x8A14) board.ram[t + 2] = 0x97; /* a pterodactyl */
    int x = (int)(rnd() % 300), w = 4 + (int)(rnd() % 12);
    poke_word((uint16_t)(t + 7), (uint16_t)x);
    poke_word((uint16_t)(t + 0x29), (uint16_t)(x + w));
    int top = 20 + (int)(rnd() % 200);
    board.ram[t + 0x27] = (uint8_t)top;
    board.ram[t + 0x28] = (uint8_t)(top - (int)(rnd() % 24));
    poke_word((uint16_t)(t + 0x1C), (uint16_t)(list - 0x100)); /* [+$1C] is a pointer to the data */
    poke_word((uint16_t)(list - 0x100), list);
    random_list(list);
}

static void pair_cases(void)
{
    long bounces = 0, unported = 0, met = 0, unhorsed = 0, eggs = 0, ptero_kills = 0;
    stop_at_outcomes = 1;
    for (int i = 0; i < (getenv("TRACE") ? 2 : CASES); i++) {
        memcpy(board.ram, base, sizeof base);
        random_task(TASK_U, LIST_X);
        random_task(TASK_X, LIST_Y);
        if (rnd() % 2) { /* overlap the boxes more often */
            memcpy(board.ram + TASK_X + 7, board.ram + TASK_U + 7, 2);
            board.ram[TASK_X + 0x27] = board.ram[TASK_U + 0x27];
        }
        if (board.ram[TASK_U + 0xD] != 0x88 || board.ram[TASK_U + 0xE] != 0xEE) {
            /* only players collect eggs ($7535: types without bit 0 in common) */
            board.ram[TASK_U + 2] |= 1;
            board.ram[TASK_X + 2] |= 1;
            board.ram[TASK_U + 0x33] = board.ram[TASK_X + 0x33] = 0; /* nor take an egg as it is laid ($7528) */
        }
        if (rnd() % 3 == 0) { /* level lances: a tie */
            memcpy(board.ram + TASK_X + 0xA, board.ram + TASK_U + 0xA, 2);
            board.ram[TASK_X + 0x26] = board.ram[TASK_U + 0x26];
            board.ram[TASK_U + 2] |= 1;
            board.ram[TASK_X + 2] |= 1;
        }
        sync_model();
        board.cpu.u = TASK_U;
        board.cpu.x = TASK_X;
        tracing = getenv("ONLY") && atoi(getenv("ONLY")) == i;
        if (tracing)
            fprintf(stderr, "types %02X %02X, +26 %02X %02X, +A %04X %04X, +17 %02X, +F %02X %02X\n",
                    board.ram[TASK_U + 2], board.ram[TASK_X + 2], board.ram[TASK_U + 0x26], board.ram[TASK_X + 0x26],
                    board.ram[TASK_U + 0xA] << 8 | board.ram[TASK_U + 0xB], board.ram[TASK_X + 0xA] << 8 | board.ram[TASK_X + 0xB],
                    board.ram[TASK_X + 0x17], board.ram[TASK_U + 0xF], board.ram[TASK_X + 0xF]);
        visited_8487 = 0;
        uint16_t free_before = (uint16_t)(board.ram[0xA008] << 8 | board.ram[0xA009]);
        uint8_t aa_before = board.ram[0xA0AA];
        uint16_t end = call(0x83D8, 0);
        tracing = 0;
        ptero_kills += (board.ram[TASK_U + 0x24] == 0x68 && board.ram[TASK_U + 0x25] == 0x28) ||
                       (board.ram[TASK_X + 0x24] == 0x68 && board.ram[TASK_X + 0x25] == 0x28);
        unhorsed += (uint16_t)(board.ram[0xA008] << 8 | board.ram[0xA009]) != free_before;
        eggs += board.ram[0xA0AA] != aa_before;
        if (end == 0xFFFF) {
            if (failures++ < 8) fprintf(stderr, "test_routines: $83D8 case %d did not return\n", i);
            continue;
        }
        int c = collide_pair(&model, TASK_U, TASK_X);
        int rom = end != RETURN ? -1 : board.cpu.cc & 1;
        unported += rom < 0;
        bounces += rom == 1;
        met += rom == 0 && visited_8487;
        if (rom < 0 && c < 0) continue; /* both stop where the port does */
        if (rom != c) {
            if (failures++ < 8) fprintf(stderr, "test_routines: $83D8 case %d: ROM %d, C %d\n", i, rom, c);
            continue;
        }
        long d = first_difference();
        if (d >= 0 && failures++ < 8)
            fprintf(stderr, "test_routines: $83D8 case %d: $%04lX = %02X, C %02X\n", i, d, board.ram[d], model.ram[d]);
        if (memcmp(board.cmos, model.cmos, sizeof model.cmos) && failures++ < 8)
            fprintf(stderr, "test_routines: $83D8 case %d: the CMOS differs\n", i);
    }
    printf("test_routines: $83D8: %ld unhorsings (an egg laid), %ld eggs collected, %ld pterodactyls killed\n", unhorsed,
           eggs, ptero_kills);
    stop_at_outcomes = 0;
    printf("test_routines: $83D8: %d cases, %ld reaching a path not ported, %ld bounced to the next pair, %ld bounced"
           " by their boxes ($8487) and on\n",
           CASES, unported, bounces, met);
}

static void display_cases(void)
{
    static const struct { uint16_t pc; int chain, nodes; } entries[] = {
        { 0xE2A1, 0, 0 }, { 0xE2B8, 1, 0 }, { 0xE2CF, 0, 1 }, { 0xE2F9, 1, 1 }, { 0xE322, 0, 2 }, { 0xE358, 1, 2 },
    };
    for (int i = 0; i < CASES; i++) {
        int e = i % 6;
        for (int band = 0; band < 4; band++) /* each chain's tail: a next field in the lists' area */
            for (int c = 0; c < 2; c++) poke_word((uint16_t)(0xA004 + c * 2 + band * 0x40), (uint16_t)(0xA9C0 + rnd() % 0x500));
        poke_word(0xA02A, (uint16_t)(0xA9C0 + rnd() % 0x500));
        for (int k = 0; k < 0x38; k++) board.ram[TASK_U + k] = (uint8_t)rnd();
        uint8_t line = (uint8_t)rnd();
        sync_model();
        board.cpu.u = TASK_U;
        board.cpu.b = line;
        if (call(entries[e].pc, 0) != RETURN) {
            if (failures++ < 8) fprintf(stderr, "test_routines: $%04X case %d did not return\n", entries[e].pc, i);
            continue;
        }
        uint16_t node = entries[e].nodes ? display_object(&model, TASK_U, entries[e].chain, entries[e].nodes)
                                         : display_node(&model, entries[e].chain, line);
        long d = first_difference();
        if (node != board.cpu.x && failures++ < 8)
            fprintf(stderr, "test_routines: $%04X case %d: X %04X, C %04X\n", entries[e].pc, i, board.cpu.x, node);
        else if (d >= 0 && failures++ < 8)
            fprintf(stderr, "test_routines: $%04X case %d: $%04lX = %02X, C %02X\n", entries[e].pc, i, d,
                    board.ram[d], model.ram[d]);
    }
    printf("test_routines: display list nodes: %d cases over the 6 entries\n", CASES);
}

static void target_cases(void)
{
    long found = 0, second = 0;
    const uint16_t p1 = 0xA400, p2 = 0xA500;
    for (int i = 0; i < CASES; i++) {
        for (int k = 0; k < 0x38; k++) board.ram[p1 + k] = board.ram[p2 + k] = board.ram[TASK_U + k] = (uint8_t)rnd();
        int near = rnd() % 2;
        for (int k = 0; k < 3; k++) {
            uint16_t t = k == 0 ? TASK_U : k == 1 ? p1 : p2;
            int x = near ? 0x80 + (int)(rnd() % 16) : (int)(rnd() % 0x130);
            poke_word((uint16_t)(t + 7), (uint16_t)x);
            board.ram[t + 0xB] = (uint8_t)(near ? 0x80 + rnd() % 16 : rnd());
        }
        poke_word(0xA073, rnd() % 8 ? p1 : 0);
        poke_word(0xA075, rnd() % 4 ? p2 : 0);
        board.ram[0xA0A8] = (uint8_t)(rnd() % 5 ? 0 : rnd());
        board.ram[0xA0A9] = (uint8_t)(rnd() % 5 ? 0 : rnd());
        sync_model();
        board.cpu.u = TASK_U;
        if (call(0x80D6, 0) != RETURN) {
            if (failures++ < 8) fprintf(stderr, "test_routines: $80D6 case %d did not return\n", i);
            continue;
        }
        uint16_t x;
        int rom = !(board.cpu.cc & 0x04), c = brain_target(&model, TASK_U, &x);
        found += rom;
        second += rom && board.cpu.x == p2;
        if ((rom != c || x != board.cpu.x) && failures++ < 8)
            fprintf(stderr, "test_routines: $80D6 case %d: ROM %d X %04X, C %d X %04X\n", i, rom, board.cpu.x, c, x);
        long d = first_difference();
        if (d >= 0 && failures++ < 8)
            fprintf(stderr, "test_routines: $80D6 case %d: $%04lX = %02X, C %02X\n", i, d, board.ram[d], model.ram[d]);
    }
    printf("test_routines: $80D6: %d cases, %ld found (%ld player 2)\n", CASES, found, second);
}

/* RAM of a game in progress, and its player's block */
static void make_base(void)
{
    static uint8_t cmos[BOARD_CMOS_SIZE];
    board_init(&board, &rom, NULL);
    for (int i = 0; i < 1800; i++) board_run_frame(&board, NULL);
    memcpy(cmos, board.cmos, sizeof cmos);
    board_init(&board, &rom, cmos);
    for (long f = 1; f <= 2000; f++) {
        board.in2 = f >= 1200 && f < 1215 ? IN2_COIN1 : 0;
        board.in0 = f >= 1300 && f < 1315 ? IN0_START1 : 0;
        board.inp1 = f > 1400 ? (uint8_t)(f % 90 < 40 ? INP_RIGHT : 0) : 0;
        board_run_frame(&board, NULL);
    }
    memcpy(base, board.ram, sizeof base);
    for (uint16_t t = (uint16_t)(base[0xA00A] << 8 | base[0xA00B]); t; t = (uint16_t)(base[t] << 8 | base[t + 1]))
        if (base[t + 2] == 0x89) base_player = t;
}

static uint8_t bcd(void) { return (uint8_t)((rnd() % 10) << 4 | rnd() % 10); }

static void score_cases(void)
{
    long lives = 0;
    for (int i = 0; i < CASES; i++) {
        memcpy(board.ram, base, sizeof base);
        const uint16_t y = 0xA04C;
        uint8_t was = 0;
        for (int k = 0; k < 4; k++) board.ram[y + k] = bcd();
        board.ram[y + 5] = board.ram[y + 6] = (uint8_t)(rnd() % 7);
        if (rnd() % 8 == 0) board.ram[y + 6] = 0xFF;
        /* the interval as the operator sets it (none, or 10000-50000 points); the
           next life ($E6E0 compares +1 + +7 (BCD) and +2 with +8-9) at the score
           or just past it: a far one would loop for thousands of lives */
        board.ram[0xA097] = rnd() % 4 ? (uint8_t)(1 + rnd() % 5) : 0;
        board.ram[0xA098] = 0;
        board.ram[y + 7] = (uint8_t)(rnd() % 2 ? 0 : 0x80);
        {
            int c = 0, h = 0;
            board.ram[y + 8] = game_daa(game_add(board.ram[y + 1], board.ram[y + 7], 0, &c, &h), &c, h);
            board.ram[y + 9] = (uint8_t)(board.ram[y + 2] + (rnd() % 2 ? rnd() % 3 : 0));
        }
        was = board.ram[y + 5];
        int nibbles = rnd() % 2;
        uint8_t a = nibbles ? (uint8_t)((bcd() & 0xF0) | rnd() % 10) : bcd();
        sync_model();
        board.cpu.x = 0x88EE;
        board.cpu.a = a;
        if (call(nibbles ? 0xE6B1 : 0xE6A8, 0) != RETURN) {
            if (failures++ < 8) fprintf(stderr, "test_routines: scoring case %d did not return\n", i);
            continue;
        }
        score_add(&model, 0x88EE, a, nibbles);
        lives += board.ram[y + 5] != was;
        long d = first_difference();
        if (d >= 0 && failures++ < 8)
            fprintf(stderr, "test_routines: scoring case %d: $%04lX = %02X, C %02X\n", i, d, board.ram[d], model.ram[d]);
        if (memcmp(board.cmos, model.cmos, sizeof model.cmos) && failures++ < 8)
            fprintf(stderr, "test_routines: scoring case %d: the CMOS differs\n", i);
    }
    printf("test_routines: scoring: %d cases, %ld with extra lives\n", CASES, lives);
}

static void platform_cases(void)
{
    uint16_t player = base_player;
    if (!player) {
        if (failures++ < 8) fprintf(stderr, "test_routines: no player in the game at frame 2000\n");
        return;
    }
    long lands = 0, bounces = 0, skipped = 0;
    for (int i = 0; i < CASES; i++) {
        memcpy(board.ram, base, sizeof base);
        poke_word((uint16_t)(player + 7), (uint16_t)((int)(rnd() % 0x130) - 10));
        board.ram[player + 0xA] = 0;
        board.ram[player + 0xB] = (uint8_t)(0x20 + rnd() % 0xC8);
        poke_word((uint16_t)(player + 0x11), (uint16_t)((int)(rnd() % 0x400) - 0x200));
        board.ram[player + 0x14] = (uint8_t)(((int)(rnd() % 9) - 4) * 2);
        board.ram[player + 0x22] = (uint8_t)((int)(rnd() % 13) - 6);
        board.ram[player + 0x23] = 0;
        board.ram[0xA04B] = 0;
        sync_model();
        board.cpu.u = player;
        uint16_t end = call(0xD9A3, 0xDA0E);
        if (end == 0xDA0E) {
            skipped++;
            continue;
        }
        if (end != RETURN) {
            if (failures++ < 8) fprintf(stderr, "test_routines: $D9A3 case %d did not return\n", i);
            continue;
        }
        int rom = board.cpu.cc & 0x04 ? 0 : 1, c = platform_below(&model, player);
        lands += !rom;
        bounces += board.ram[player + 0x23] != 0;
        if (rom != c) {
            if (failures++ < 8) fprintf(stderr, "test_routines: $D9A3 case %d: ROM %d, C %d\n", i, rom, c);
            continue;
        }
        long d = first_difference();
        if (d >= 0 && failures++ < 8)
            fprintf(stderr, "test_routines: $D9A3 case %d: $%04lX = %02X, C %02X\n", i, d, board.ram[d], model.ram[d]);
    }
    printf("test_routines: $D9A3: %d cases, %ld landing, %ld bounced, %ld at the troll's hand skipped\n", CASES, lands,
           bounces, skipped);
}

/* the pterodactyl's brain states on the game in progress (the players' blocks there) */
static void ptero_cases(void)
{
    static const uint16_t states[] = { 0x6640, 0x6695, 0x6795, 0x67B3 };
    long swoops = 0, ended = 0, targets = 0;
    for (int i = 0; i < CASES; i++) {
        memcpy(board.ram, base, sizeof base);
        for (int k = 7; k < 0x38; k++) board.ram[TASK_U + k] = (uint8_t)rnd();
        poke_word(TASK_U + 0x24, states[rnd() % 4]);
        poke_word(TASK_U + 7, (uint16_t)((int)(rnd() % 0x140) - 0x10));
        board.ram[TASK_U + 0xB] = (uint8_t)(0x20 + rnd() % 0xC0);
        poke_word(TASK_U + 0x11, (uint16_t)(((int)(rnd() % 0x21) - 0x10) * 0x10 + (int)(rnd() % 4 ? 0u : rnd() % 0x10)));
        poke_word(TASK_U + 0x2B, (uint16_t)((int)(rnd() % 0x140) - 0x10));
        board.ram[TASK_U + 0x2E] = (uint8_t)(rnd() % 3 ? board.ram[TASK_U + 0xB] + (int)(rnd() % 0x20) - 0x10 : (int)rnd());
        board.ram[TASK_U + 0x2F] = (uint8_t)(rnd() % 2 ? 1 : rnd());
        board.ram[TASK_U + 0x33] = (uint8_t)(rnd() % 2 ? 1 + rnd() % 3 : rnd());
        board.ram[TASK_U + 0x34] = (uint8_t)(rnd() % 2 ? 0 : 0xFF);
        board.ram[TASK_U + 0x22] = (uint8_t)(rnd() % 3 ? 0 : (int)(rnd() % 13) - 6);
        board.ram[TASK_U + 0x2D] = (uint8_t)(rnd() % 4);
        board.ram[0xA0A8] = (uint8_t)(rnd() % 4 ? 0 : 0x46); /* player 1 just spawned (out), or not */
        board.ram[base_player + 0xB] = (uint8_t)(rnd() % 4 ? 0x20 + rnd() % 0xC0 : 0xCC + rnd() % 0x10);
        if (rnd() % 8 == 0) poke_word(0xA073, 0);
        sync_model();
        board.cpu.u = TASK_U;
        uint16_t end = call(board.ram[TASK_U + 0x24] << 8 | board.ram[TASK_U + 0x25], 0x6808);
        uint8_t aim[2] = { model.ram[TASK_U + 0x2B], model.ram[TASK_U + 0x2C] };
        int c = ptero_brain(&model, TASK_U);
        if (end == 0x6808) {
            ended++;
            if (c != -1 && failures++ < 8) fprintf(stderr, "test_routines: pterodactyl case %d: ROM off the screen, C not\n", i);
            continue;
        }
        if (end != RETURN) {
            if (failures++ < 8) fprintf(stderr, "test_routines: pterodactyl case %d did not return\n", i);
            continue;
        }
        targets += aim[0] != board.ram[TASK_U + 0x2B] || aim[1] != board.ram[TASK_U + 0x2C];
        swoops += board.ram[TASK_U + 0x24] == 0x66 && board.ram[TASK_U + 0x25] == 0x95;
        long d = first_difference();
        if ((c || d >= 0) && failures++ < 8)
            fprintf(stderr, "test_routines: pterodactyl case %d: C %d, $%04lX = %02X, C %02X\n", i, c, d,
                    d >= 0 ? board.ram[d] : 0, d >= 0 ? model.ram[d] : 0);
    }
    printf("test_routines: the pterodactyl's brain: %d cases, %ld aiming at a player, %ld swooping after, %ld off the"
           " screen\n", CASES, targets, swoops, ended);
}

/* the pterodactyl's task, step by step (the ROM's to its sleep at $E15F)
   from a random flight or its death, until it ends or reaches a step not ported */
static void ptero_step_cases(void)
{
    static const uint16_t states[] = { 0x6640, 0x6695, 0x6795, 0x67B3, 0x6828, 0x6828 };
    long steps = 0, bodies = 0;
    for (int i = 0; i < CASES / 20; i++) {
        memcpy(board.ram, base, sizeof base);
        for (int k = 7; k < 0x38; k++) board.ram[TASK_U + k] = (uint8_t)rnd();
        board.ram[TASK_U + 2] = 0x97;
        poke_word(TASK_U + 5, 0x6925);
        poke_word(TASK_U + 0xD, 0x8A14);
        poke_word(TASK_U + 0x24, states[rnd() % 6]);
        poke_word(TASK_U + 7, (uint16_t)((int)(rnd() % 0x130) - 8));
        poke_word(TASK_U + 0xA, (uint16_t)(0x28 + rnd() % 0xB0));
        poke_word(TASK_U + 0x11, (uint16_t)((int)(rnd() % 0x200) - 0x100));
        board.ram[TASK_U + 0xF] = (uint8_t)(rnd() % 2 ? 0 : 0xFF);
        board.ram[TASK_U + 0x10] = (uint8_t)(rnd() % 4 * 2);
        board.ram[TASK_U + 0x14] = (uint8_t)(rnd() % 2 ? 4 : 0xFC);
        board.ram[TASK_U + 0x15] = (uint8_t)(1 + rnd() % 8);
        board.ram[TASK_U + 0x17] = (uint8_t)(rnd() % 3 * 0x0C);
        poke_word(TASK_U + 0x1C, 0);
        poke_word(TASK_U + 0x1E, 0);
        board.ram[TASK_U + 0x22] = 0;
        board.ram[TASK_U + 0x23] = 0;
        board.ram[TASK_U + 0x2D] = (uint8_t)(rnd() % 8);
        board.ram[TASK_U + 0x34] = (uint8_t)(rnd() % 2 ? 0 : 0xFF);
        board.ram[0xA0AA] = (uint8_t)(rnd() % 4 ? 3 : 0);
        board.ram[0xA0AC] = (uint8_t)(rnd() % 2 ? 0 : 0x25);
        board.ram[0xA0A8] = (uint8_t)(rnd() % 4 ? 0 : 0x46);
        poke_word(0xA02E, TASK_U);
        for (int n = 0; n < 200; n++) {
            uint16_t at = (uint16_t)(board.ram[TASK_U + 5] << 8 | board.ram[TASK_U + 6]);
            if (!ptero_ported(at) || at == 0x6816) break; /* the points, or its end */
            poke_word(0xA02A, 0xAA00); /* the display list's free nodes start over each frame */
            sync_model();
            board.cpu.u = TASK_U;
            uint16_t end = call(at, 0xE15F);
            StepResult r = ptero_step(&model, TASK_U, at);
            if (end != 0xE15F) {
                if (failures++ < 8) fprintf(stderr, "test_routines: pterodactyl step case %d at $%04X did not sleep\n", i, at);
                break;
            }
            if (r != STEP_DONE) {
                if (failures++ < 8) fprintf(stderr, "test_routines: pterodactyl step case %d at $%04X: C did not port it\n", i, at);
                break;
            }
            steps++;
            bodies += at == 0x686B;
            long d = first_difference();
            if (d >= 0) {
                if (failures++ < 8)
                    fprintf(stderr, "test_routines: pterodactyl step case %d at $%04X: $%04lX = %02X, C %02X\n", i, at, d,
                            board.ram[d], model.ram[d]);
                break;
            }
        }
    }
    printf("test_routines: the pterodactyl's steps: %d cases, %ld steps, %ld drawing its body\n", CASES / 20, steps, bodies);
}

/* a riderless mount's brain ($793C, $7A1F, $7A5E) going for its knight */
static void mount_cases(void)
{
    static const uint16_t states[] = { 0x793C, 0x793C, 0x7A1F, 0x7A5E };
    long picked = 0, hops = 0;
    for (int i = 0; i < CASES; i++) {
        memcpy(board.ram, base, sizeof base);
        for (int k = 7; k < 0x38; k++) board.ram[TASK_U + k] = board.ram[TASK_X + k] = (uint8_t)rnd();
        board.ram[TASK_X + 2] = 0x82;
        poke_word(TASK_U + 0x2B, TASK_X);
        poke_word(TASK_U + 0xD, (uint16_t)(0x8981 + 0x31 * (rnd() % 3)));
        poke_word(TASK_U + 0x24, states[rnd() % 4]);
        int ux = (int)(rnd() % 0x130) - 8, near = rnd() % 2;
        poke_word(TASK_U + 7, (uint16_t)ux);
        poke_word(TASK_X + 7, (uint16_t)(near ? ux + (int)(rnd() % 0x30) - 0x18 : (int)(rnd() % 0x130) - 8));
        poke_word(TASK_U + 0xA, (uint16_t)(0x30 + rnd() % 0xA8));
        poke_word(TASK_X + 0xA, (uint16_t)(rnd() % 3 ? board.ram[TASK_U + 0xB] : 0x30 + rnd() % 0xA8));
        poke_word(TASK_U + 0x11, (uint16_t)((int)(rnd() % 0x600) - 0x300));
        board.ram[TASK_U + 0x14] = (uint8_t)(((int)(rnd() % 9) - 4) * 2);
        board.ram[TASK_U + 0xF] = (uint8_t)(rnd() % 2 ? 0 : 0xFF);
        if (rnd() % 2) poke_word(TASK_U + 0x20, 0);
        board.ram[TASK_U + 0x2E] = (uint8_t)(rnd() % 2);
        board.ram[TASK_U + 0x2D] = (uint8_t)(1 + rnd() % 7);
        board.ram[0xA090] = (uint8_t)(rnd() % 2);
        sync_model();
        board.cpu.u = TASK_U;
        uint16_t end = call(board.ram[TASK_U + 0x24] << 8 | board.ram[TASK_U + 0x25], 0x8D40);
        int c = brain_enemy(&model, TASK_U, (uint16_t)(model.ram[TASK_U + 0x24] << 8 | model.ram[TASK_U + 0x25]));
        if (end != RETURN && end != 0x8D40) {
            if (failures++ < 8) fprintf(stderr, "test_routines: mount case %d did not return\n", i);
            continue;
        }
        picked += end == 0x8D40;
        hops += board.ram[TASK_U + 0x24] == 0x7A && board.ram[TASK_U + 0x25] == 0x5E;
        long d = first_difference();
        if ((c != (end == 0x8D40) || d >= 0 || (c == 0 && board.cpu.a != model.ram[0xA032])) && failures++ < 8)
            fprintf(stderr, "test_routines: mount case %d: ROM %d, C %d, $%04lX = %02X, C %02X\n", i, end == 0x8D40, c, d,
                    d >= 0 ? board.ram[d] : 0, d >= 0 ? model.ram[d] : 0);
    }
    printf("test_routines: the riderless mount's brain: %d cases, %ld picking up the knight, %ld hopping\n", CASES, picked,
           hops);
}

/* $77BE: points placed, waiting for the displays near them (two on the list, type 3 or 4) */
static void points_cases(void)
{
    long waits = 0;
    for (int i = 0; i < CASES; i++) {
        memcpy(board.ram, base, sizeof base);
        uint16_t t1 = (uint16_t)(board.ram[0xA00A] << 8 | board.ram[0xA00B]);
        uint16_t t2 = (uint16_t)(board.ram[t1] << 8 | board.ram[t1 + 1]);
        if (t2 == TASK_X || t2 == 0xA500 || t2 == TASK_U || t1 == TASK_X || t1 == 0xA500 || t1 == TASK_U) continue;
        poke_word(t2, TASK_X);
        poke_word(TASK_X, 0xA500);
        poke_word(0xA500, 0);
        for (int k = 2; k < 0x38; k++) board.ram[TASK_U + k] = board.ram[TASK_X + k] = board.ram[0xA500 + k] = (uint8_t)rnd();
        int x = (int)(rnd() % 0x130) - 8, line = 0x20 + (int)(rnd() % 0xC0);
        poke_word(TASK_U + 7, (uint16_t)x);
        poke_word(TASK_U + 0xA, (uint16_t)line);
        for (uint16_t t = TASK_X; t <= 0xA500; t += 0x100) {
            board.ram[t + 2] = (uint8_t)(rnd() % 4 ? 3 : 4);
            poke_word((uint16_t)(t + 7), (uint16_t)(x + (int)(rnd() % 0x28) - 0x14));
            poke_word((uint16_t)(t + 0xA), (uint16_t)(line + (int)(rnd() % 0x10) - 8));
            board.ram[t + 0x2D] = (uint8_t)(rnd() % 0x20);
            board.ram[t + 0x32] = (uint8_t)(rnd() % 8);
        }
        poke_word(0xA02E, TASK_U);
        sync_model();
        board.cpu.u = TASK_U;
        uint16_t end = call(0x77BE, 0xE15F);
        StepResult r = points_step(&model, TASK_U, 0x77BE);
        if (end != 0xE15F || r != STEP_DONE) {
            if (failures++ < 8) fprintf(stderr, "test_routines: points case %d did not sleep\n", i);
            continue;
        }
        waits += board.ram[TASK_U + 0x32] > 1;
        long d = first_difference();
        if (d >= 0 && failures++ < 8)
            fprintf(stderr, "test_routines: points case %d: $%04lX = %02X, C %02X\n", i, d, board.ram[d], model.ram[d]);
    }
    printf("test_routines: points placed: %d cases, %ld waiting for others\n", CASES, waits);
}

/* the wave task from a wave's end through the next wave's start (every
   record of $7173-$719B, the endings, lava bridges and flames, one or two
   players), step by step; the tasks after it are taken off the list
   before each step, as if gone */
static void waves_cases(void)
{
    static const uint16_t endings[] = { 0x71C8, 0x7236, 0x7274, 0x72D2 };
    uint16_t w = 0;
    for (uint16_t t = (uint16_t)(base[0xA00A] << 8 | base[0xA00B]); t; t = (uint16_t)(base[t] << 8 | base[t + 1]))
        if (base[t + 5] == 0x6D && base[t + 6] == 0x91) w = t;
    if (!w) {
        if (failures++ < 8) fprintf(stderr, "test_routines: no wave task in the game at frame 2000\n");
        return;
    }
    long steps = 0, platforms = 0, eggs = 0, done = 0;
    for (int i = 0; i < CASES / 50; i++) {
        memcpy(board.ram, base, sizeof base);
        poke_word((uint16_t)(w + 7), (uint16_t)(0x7173 + 4 * (rnd() % 10)));
        poke_word((uint16_t)(w + 0x13), endings[rnd() % 4]);
        board.ram[0xA0AA] = 0;
        board.ram[0xA09F] = (uint8_t)(rnd() % 3 ? 0 : 1 + rnd() % 2);
        board.ram[0xA0A0] = (uint8_t)(rnd() % 3 ? 0 : 1 + rnd() % 2);
        board.ram[0xA09E] = (uint8_t)(rnd() % 0x99);
        board.ram[0xA052] = (uint8_t)(rnd() % 2);
        board.ram[0xA05C] = (uint8_t)(rnd() % 2);
        for (uint16_t a = 0xA0A3; a <= 0xA0A6; a++) board.ram[a] = (uint8_t)(rnd() % 2 ? 0 : 0xFF);
        if (rnd() % 8 == 0) board.ram[0xA090] = 0;
        for (uint16_t a = 0xB1AF; a < 0xB1AD + 3 * 28; a += 3) board.ram[a] = (uint8_t)(rnd() % 3);
        board.ram[0xA09B] = (uint8_t)(0xD8 + rnd() % 0x28);
        board.cmos[0x14] = 0xF0;
        board.cmos[0x15] = (uint8_t)(0xF0 | rnd() % 10); /* the difficulty */
        board.ram[w + 1] = board.ram[w] = 0;
        poke_word((uint16_t)(w + 5), 0x6D91);
        for (int n = 0; n < 800; n++) {
            uint16_t at = (uint16_t)(board.ram[w + 5] << 8 | board.ram[w + 6]);
            if (n && at == 0x6D91) {
                done++;
                break;
            }
            board.ram[w] = board.ram[w + 1] = 0;
            poke_word(0xA02E, w);
            poke_word(0xA02A, 0xAA00);
            sync_model();
            board.cpu.u = w;
            uint16_t end = call(at, 0xE15F);
            StepResult r = game_step(&model, w);
            if (end != 0xE15F || r != STEP_DONE) {
                if (failures++ < 8)
                    fprintf(stderr, "test_routines: wave case %d at $%04X: ROM %s, C %s\n", i, at,
                            end == 0xE15F ? "slept" : "did not sleep", r == STEP_DONE ? "slept" : "not ported");
                break;
            }
            steps++;
            platforms += at == 0x6F9B;
            eggs += at == 0x7480;
            long d = first_difference();
            if (d >= 0) {
                if (failures++ < 8)
                    fprintf(stderr, "test_routines: wave case %d at $%04X: $%04lX = %02X, C %02X\n", i, at, d, board.ram[d],
                            model.ram[d]);
                break;
            }
        }
    }
    printf("test_routines: the waves: %d cases, %ld steps, %ld back at the manager, %ld after a platform, %ld eggs\n",
           CASES / 50, steps, done, platforms, eggs);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: test_routines <joust.zip|folder>\n");
        return 2;
    }
    char err[512];
    if (rom_load(&rom, argv[1], err, sizeof err)) {
        fprintf(stderr, "test_routines: %s\n", err);
        return 1;
    }
    for (unsigned a = 0; a < 0x10000; a++) {
        int v = rom_byte(&rom, a);
        data[a] = v < 0 ? 0 : (uint8_t)v;
    }
    model.rom = data;
    board_init(&board, &rom, NULL);
    board.romsel = 1;    /* as the game runs */
    board.cpu.dp = 0xA0; /* direct page: the game's globals */
    rows_cases();
    make_base();
    board.cpu.dp = 0xA0;
    board.romsel = 1; /* the game had it off at that moment */
    pair_cases();
    display_cases();
    target_cases();
    score_cases();
    platform_cases();
    ptero_cases();
    ptero_step_cases();
    mount_cases();
    points_cases();
    waves_cases();
    if (failures) {
        fprintf(stderr, "test_routines: %d differences\n", failures);
        return 1;
    }
    printf("test_routines: OK\n");
    return 0;
}
