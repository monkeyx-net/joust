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
 *     the carry must match
 *   - $83D8, a pair's box and data test: random task blocks; it returns
 *     (apart) or goes on to the handlers at $8418 (a collision)
 *   - the display list's nodes ($E2A1, $E2B8, $E2CF, $E2F9, $E322, $E358,
 *     sim/display.c): random chain tails, free pointer and objects; the
 *     node returned (X) and all of RAM must match
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/rom.h"
#include "emu/board.h"
#include "sim/collide.h"
#include "sim/display.h"
#include "sim/game.h"

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

/* Run the ROM from `pc` until it returns to RETURN or reaches `stop` (0: none). */
static uint16_t call(uint16_t pc, uint16_t stop)
{
    board.cpu.s = STACK_TOP;
    board.ram[--board.cpu.s] = RETURN & 0xFF;
    board.ram[--board.cpu.s] = RETURN >> 8;
    board.cpu.pc = pc;
    board.cpu.cc |= CC_I | CC_F;
    for (long n = 0; n < 200000; n++) {
        if (board.cpu.pc == RETURN || (stop && board.cpu.pc == stop)) return board.cpu.pc;
        if (getenv("TRACE")) fprintf(stderr, "  %04X D=%04X X=%04X Y=%04X CC=%02X\n", board.cpu.pc, board.cpu.a << 8 | board.cpu.b, board.cpu.x, board.cpu.y, board.cpu.cc);
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

static void sync_model(void) { game_load(&model, board.ram, board.romsel); }

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
        int rom_carry = board.cpu.cc & 1, c = collide_rows(&model, LIST_X, LIST_Y, dx);
        hits += rom_carry;
        if (rom_carry != c && failures++ < 8)
            fprintf(stderr, "test_routines: $DC21 case %d: ROM %d, C %d\n", i, rom_carry, c);
    }
    printf("test_routines: $DC21: %d cases, %ld overlapping\n", CASES, hits);
}

static void random_task(uint16_t t, uint16_t list)
{
    for (int i = 0; i < 0x38; i++) board.ram[t + i] = (uint8_t)rnd();
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
    long hits = 0;
    for (int i = 0; i < (getenv("TRACE") ? 2 : CASES); i++) {
        random_task(TASK_U, LIST_X);
        random_task(TASK_X, LIST_Y);
        if (rnd() % 2) { /* overlap the boxes more often */
            memcpy(board.ram + TASK_X + 7, board.ram + TASK_U + 7, 2);
            board.ram[TASK_X + 0x27] = board.ram[TASK_U + 0x27];
        }
        sync_model();
        board.cpu.u = TASK_U;
        board.cpu.x = TASK_X;
        uint16_t end = call(0x83D8, 0x8418);
        if (end == 0xFFFF) {
            if (failures++ < 8) fprintf(stderr, "test_routines: $83D8 case %d did not return\n", i);
            continue;
        }
        int c = collide_pair(&model, TASK_U, TASK_X);
        int rom = end == 0x8418 ? -1 : 0;
        hits += rom == -1;
        if (rom != c) {
            if (failures++ < 8) fprintf(stderr, "test_routines: $83D8 case %d: ROM %d, C %d\n", i, rom, c);
            continue;
        }
        long d = first_difference();
        if (d >= 0 && failures++ < 8)
            fprintf(stderr, "test_routines: $83D8 case %d: $%04lX = %02X, C %02X\n", i, d, board.ram[d], model.ram[d]);
    }
    printf("test_routines: $83D8: %d cases, %ld colliding\n", CASES, hits);
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
    pair_cases();
    display_cases();
    if (failures) {
        fprintf(stderr, "test_routines: %d differences\n", failures);
        return 1;
    }
    printf("test_routines: OK\n");
    return 0;
}
