/*
 * The system program's routines against the ROM's (label rom):
 *   test_operator <joust.zip|folder>
 * Each case sets RAM, the CMOS and the switches on the emulated board,
 * runs the ROM routine on its CPU (interrupts off, as the test mode runs)
 * until it returns or reaches a given address, and the C routine
 * (sim/operator.c) on a model of the same state; all of RAM (but the
 * stack page), the CMOS, the palette and the ROM select must match.
 *   - $3D36, the CMOS's check (to the reboot at $E000, or to the message
 *     waiting for the advance at $3D69): adjustments, the operator's
 *     message, audits and table entries corrupted at random
 *   - $3FC1 (the tables checked), $3E09 (the tables reset)
 *   - $FE4A, the CMOS test, from random generator states
 *   - $FA27, the averages' BCD division, and the bookkeeping's page
 *     ($F962 to $FA21) on random BCD audits
 *   - the adjustments' page ($FB17 to $FB72) and $FCBA on random settings
 *   - the crosshatch ($F53A), the colour bars ($F4EA, $F715)
 *   - the sound test's step ($FDDC) and the switch test's look ($F781)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/rom.h"
#include "emu/board.h"
#include "sim/game.h"
#include "sim/operator.h"

#define RETURN 0x0000
#define STACK_TOP 0xBEF0

static RomSet rom;
static Board board;
static uint8_t data[0x10000];
static Game model;
static int failures;
static uint32_t seed = 4321;

static uint32_t rnd(void)
{
    seed = seed * 1103515245u + 12345u;
    return seed >> 8;
}

static uint8_t bcd(void) { return (uint8_t)((rnd() % 10) << 4 | rnd() % 10); }

static uint16_t call(uint16_t pc, uint16_t stop, uint16_t stop2)
{
    board.cpu.s = STACK_TOP;
    board.ram[--board.cpu.s] = RETURN & 0xFF;
    board.ram[--board.cpu.s] = RETURN >> 8;
    board.cpu.pc = pc;
    board.cpu.cc |= CC_I | CC_F;
    board.cpu.waiting = 0;
    board.cpu.dp = 0xA0;
    board.cpu.irq = board.cpu.firq = board.cpu.nmi = 0;
    for (long n = 0; n < 50000000; n++) {
        if (board.cpu.pc == RETURN || board.cpu.pc == stop || board.cpu.pc == stop2) return board.cpu.pc;
        m6809_step(&board.cpu);
    }
    return 0xFFFF;
}

/* the model from the board */
static void sync(void)
{
    board.romsel = 1;
    game_load(&model, board.ram, board.romsel);
    memcpy(model.cmos, board.cmos, sizeof model.cmos);
    memcpy(model.palette, board.palette, sizeof model.palette);
    model.in0 = board.in0;
    model.inp1 = board.inp1;
    model.inp2 = board.inp2;
    model.in2 = board.in2;
    model.pia0_crb = board.pia[0].b.cr;
    model.mux = board.mux_select;
}

static int differs(const char *what, int i)
{
    long d = -1;
    for (unsigned a = 0; a < 0xBE00 && d < 0; a++)
        if (model.ram[a] != board.ram[a]) d = a;
    for (unsigned a = 0xBF00; a < 0xC000 && d < 0; a++)
        if (model.ram[a] != board.ram[a]) d = a;
    const char *why = NULL;
    char buf[80];
    if (d >= 0) {
        snprintf(buf, sizeof buf, "$%04lX = %02X, C %02X", d, board.ram[d], model.ram[d]);
        why = buf;
    } else if (memcmp(model.cmos, board.cmos, sizeof model.cmos)) {
        for (int k = 0; k < 0x400; k++)
            if (model.cmos[k] != board.cmos[k]) {
                snprintf(buf, sizeof buf, "CMOS $%04X = %02X, C %02X", 0xCC00 + k, board.cmos[k], model.cmos[k]);
                break;
            }
        why = buf;
    } else if (memcmp(model.palette, board.palette, sizeof model.palette)) {
        why = "the palette";
    } else if ((model.romsel & 1) != (board.romsel & 1)) {
        why = "the ROM select";
    }
    if (!why) return 0;
    static const char *last;
    static int shown;
    if (last != what) {
        last = what;
        shown = 0;
    }
    if (shown++ < 3) fprintf(stderr, "test_operator: %s case %d: %s\n", what, i, why);
    failures++;
    return 1;
}

/* a valid CMOS: the board's factory settings, then a game's worth of play */
static uint8_t base_cmos[BOARD_CMOS_SIZE];
static uint8_t base_ram[0xC000];

static void reset_state(void)
{
    memcpy(board.cmos, base_cmos, sizeof board.cmos);
    memcpy(board.ram, base_ram, sizeof board.ram);
    for (int i = 0; i < 16; i++) board.palette[i] = (uint8_t)rnd();
    board.in0 = board.inp1 = board.inp2 = board.in2 = 0;
}

static void corrupt_cmos(void)
{
    int kind = (int)(rnd() % 6);
    if (kind == 0) board.cmos[rnd() % 0x24] = (uint8_t)(0xF0 | rnd()); /* an adjustment */
    if (kind == 1) board.cmos[0x24 + rnd() % 0x68] = (uint8_t)(0xF0 | rnd()); /* the message */
    if (kind == 2)
        for (int k = 0; k < 6; k++) board.cmos[0x102 + rnd() % 0x3C] = (uint8_t)(0xF0 | (0x0A + rnd() % 6)); /* audits */
    if (kind == 3)
        for (int k = 0, n = (int)(rnd() % 12); k < n; k++) board.cmos[0x174 + rnd() % 0x230] = (uint8_t)(0xF0 | rnd());
    if (kind == 4) board.cmos[0x13E + rnd() % 0x36] = (uint8_t)(0xF0 | rnd()); /* the best one */
}

static void cmos_check_cases(void)
{
    long waits = 0;
    for (int i = 0; i < 300; i++) {
        reset_state();
        corrupt_cmos();
        if (rnd() % 3 == 0) corrupt_cmos();
        sync();
        uint16_t end = call(0x3D36, 0xE000, 0x3D69);
        int c = operator_cmos_check_now(&model);
        if (end == 0xFFFF || (end == 0x3D69) != c) {
            if (failures++ < 10) fprintf(stderr, "test_operator: $3D36 case %d: ROM at $%04X, C %d\n", i, end, c);
            continue;
        }
        waits += c;
        differs("$3D36", i);
    }
    printf("test_operator: $3D36: 300 cases, %ld with a message\n", waits);
}

static void tables_cases(void)
{
    for (int i = 0; i < 300; i++) {
        reset_state();
        int kind = i % 3;
        if (kind < 2)
            for (int k = 0, n = (int)(rnd() % 20); k < n; k++)
                board.cmos[(kind ? 0x13E : 0x174) + rnd() % (kind ? 0x36 : 0x230)] = (uint8_t)(0xF0 | rnd());
        sync();
        if (call(kind == 2 ? 0x3E09 : 0x3FC1, 0, 0) != RETURN) {
            if (failures++ < 10) fprintf(stderr, "test_operator: tables case %d did not return\n", i);
            continue;
        }
        if (kind == 2) operator_tables_reset(&model);
        else operator_tables_validate(&model);
        differs("tables", i);
    }
    printf("test_operator: $3FC1 / $3E09: 300 cases\n");
}

static void cmos_test_cases(void)
{
    for (int i = 0; i < 40; i++) {
        reset_state();
        board.ram[0xA0D9] = (uint8_t)rnd();
        board.ram[0xA0DA] = (uint8_t)rnd();
        board.ram[0xA0DB] = (uint8_t)rnd();
        sync();
        if (call(0xFE4A, 0, 0) != RETURN) {
            if (failures++ < 10) fprintf(stderr, "test_operator: $FE4A case %d did not return\n", i);
            continue;
        }
        int rom_ok = !(board.cpu.cc & 1), c = operator_cmos_test(&model);
        if (rom_ok != c && failures++ < 10) fprintf(stderr, "test_operator: $FE4A case %d: ROM %d, C %d\n", i, rom_ok, c);
        differs("$FE4A", i);
    }
    printf("test_operator: $FE4A: 40 cases\n");
}

static void average_cases(void)
{
    for (int i = 0; i < 2000; i++) {
        reset_state();
        for (uint16_t a = 0xA0DC; a <= 0xA0E9; a++) board.ram[a] = (uint8_t)rnd();
        for (uint16_t a = 0xA0DF; a <= 0xA0E4; a++) board.ram[a] = rnd() % 4 ? bcd() : 0;
        if (rnd() % 4 == 0) board.ram[0xA0E2] = board.ram[0xA0E3] = board.ram[0xA0E4] = 0;
        sync();
        if (call(0xFA27, 0, 0) != RETURN) {
            if (failures++ < 10) fprintf(stderr, "test_operator: $FA27 case %d did not return\n", i);
            continue;
        }
        uint8_t a = operator_average(&model);
        if (a != board.cpu.a && failures++ < 10)
            fprintf(stderr, "test_operator: $FA27 case %d: ROM A %02X, C %02X\n", i, board.cpu.a, a);
        differs("$FA27", i);
    }
    printf("test_operator: $FA27: 2000 cases\n");
}

static void put_byte(uint16_t at, uint8_t v)
{
    board.cmos[at - 0xCC00 + 1] = (uint8_t)(0xF0 | (v & 0x0F));
    board.cmos[at - 0xCC00] = (uint8_t)(0xF0 | v >> 4);
}

static void bookkeeping_cases(void)
{
    for (int i = 0; i < 200; i++) {
        reset_state();
        for (uint16_t at = 0xCD02; at < 0xCD3E; at = (uint16_t)(at + 2)) put_byte(at, rnd() % 3 ? bcd() : 0);
        sync();
        if (call(0xF962, 0xFA21, 0) != 0xFA21) {
            if (failures++ < 10) fprintf(stderr, "test_operator: bookkeeping case %d did not get there\n", i);
            continue;
        }
        operator_bookkeeping(&model);
        differs("bookkeeping", i);
    }
    printf("test_operator: the bookkeeping: 200 cases\n");
}

static void adjust_cases(void)
{
    for (int i = 0; i < 200; i++) {
        reset_state();
        for (uint16_t at = 0xCC00; at < 0xCC24; at = (uint16_t)(at + 2)) {
            uint8_t lo = data[0xFC96 + (at & 0xFF)], hi = data[0xFC97 + (at & 0xFF)];
            uint8_t v = lo;
            for (int k = (int)(rnd() % 12); k > 0 && v != hi; k--) {
                v = (uint8_t)(v + 1);
                if ((v & 0x0F) > 9) v = (uint8_t)(v + 6);
            }
            put_byte(at, v);
        }
        sync();
        if (call(0xFB17, 0xFB72, 0) != 0xFB72) {
            if (failures++ < 10) fprintf(stderr, "test_operator: adjustments case %d did not get there\n", i);
            continue;
        }
        operator_adjust_page(&model);
        differs("adjustments", i);
    }
    printf("test_operator: the adjustments' page: 200 cases\n");
}

static void adjust_edit_cases(void)
{
    long moved = 0, changed = 0;
    for (int i = 0; i < 2000; i++) {
        reset_state();
        int k = (int)(rnd() % 18);
        uint16_t u = (uint16_t)(0xCC00 + 2 * k), y = (uint16_t)(0x1620 + 0x0A * k);
        uint8_t lo = data[0xFC96 + (u & 0xFF)], hi = data[0xFC97 + (u & 0xFF)];
        put_byte(u, rnd() % 3 ? lo : rnd() % 2 ? hi : bcd());
        board.ram[0xA0D9] = (uint8_t)(rnd() % 3 ? 0 : rnd() % 10);
        board.ram[0xA0DC] = 0;
        board.ram[0xA0E9] = 0x22;
        board.ram[0xA0F7] = 0x30;
        board.ram[0xA0F8] = 0x33;
        uint8_t a = (uint8_t)(rnd() % 4 | (rnd() % 4 ? 0 : rnd() & 0xFC));
        sync();
        int edit = i % 2;
        board.cpu.a = a;
        board.cpu.u = u;
        board.cpu.y = y;
        if (call(edit ? 0xFC51 : 0xFBCD, edit ? 0xFC8B + 0x08 : 0xFC14, 0) == 0xFFFF) {
            if (failures++ < 10) fprintf(stderr, "test_operator: edit case %d did not return\n", i);
            continue;
        }
        if (edit) {
            operator_value_change(&model, a, u, y);
            changed += model.cmos[u - 0xCC00 + 1] != board.cmos[u - 0xCC00 + 1] || 1;
        } else {
            uint16_t yy = y, uu = u;
            operator_cursor_move(&model, a, &yy, &uu);
            if ((yy != board.cpu.y || uu != board.cpu.u) && failures++ < 10)
                fprintf(stderr, "test_operator: $FBCD case %d: ROM Y %04X U %04X, C %04X %04X\n", i, board.cpu.y,
                        board.cpu.u, yy, uu);
            moved += yy != y;
        }
        differs(edit ? "$FC51" : "$FBCD", i);
    }
    printf("test_operator: the adjustments' cursor and values: 2000 cases, %ld moves\n", moved);
    (void)changed;
}

static void screens_cases(void)
{
    static const uint16_t at[] = { 0xF53A, 0xF4EA, 0xF715 };
    for (int i = 0; i < 3; i++) {
        reset_state();
        for (unsigned a = 0; a < 0x9800; a++) board.ram[a] = (uint8_t)rnd();
        sync();
        if (call(at[i], 0, 0) != RETURN) {
            if (failures++ < 10) fprintf(stderr, "test_operator: $%04X did not return\n", at[i]);
            continue;
        }
        if (i == 0) operator_crosshatch(&model);
        else if (i == 1) operator_bars(&model);
        else operator_colour_bars(&model);
        differs("screens", i);
    }
    printf("test_operator: the crosshatch and colour bars\n");
}

static void sound_switch_cases(void)
{
    for (int i = 0; i < 100; i++) {
        reset_state();
        board.ram[0xA0F8] = (uint8_t)(0xC0 | rnd());
        board.ram[0xA0F9] = (uint8_t)(1 + rnd() % 6);
        board.ram[0xA0D7] = (uint8_t)(rnd() % 2);
        board.in2 = (uint8_t)(rnd() % 2);
        sync();
        if (call(0xFDDC, 0, 0) != RETURN) {
            if (failures++ < 10) fprintf(stderr, "test_operator: $FDDC case %d did not return\n", i);
            continue;
        }
        operator_sound_step(&model);
        differs("$FDDC", i);
    }
    for (int i = 0; i < 300; i++) {
        reset_state();
        for (uint16_t a = 0xA0F7; a <= 0xA100; a++) board.ram[a] = (uint8_t)rnd();
        board.in2 = (uint8_t)rnd();
        board.inp1 = (uint8_t)(rnd() & 0x0F);
        board.inp2 = (uint8_t)(rnd() & 0x0F);
        sync();
        if (call(0xF781, 0xF799, 0) != 0xF799) {
            if (failures++ < 10) fprintf(stderr, "test_operator: switches case %d did not get there\n", i);
            continue;
        }
        operator_switches(&model);
        differs("switches", i);
    }
    printf("test_operator: the sound test's page, 300 switch looks\n");
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: test_operator <joust.zip|folder>\n");
        return 2;
    }
    char err[512];
    if (rom_load(&rom, argv[1], err, sizeof err)) {
        fprintf(stderr, "test_operator: %s\n", err);
        return 1;
    }
    for (unsigned a = 0; a < 0x10000; a++) {
        int v = rom_byte(&rom, a);
        data[a] = v < 0 ? 0 : (uint8_t)v;
    }
    model.rom = data;
    board_init(&board, &rom, NULL);
    for (int i = 0; i < 1800; i++) board_run_frame(&board, NULL);
    memcpy(base_cmos, board.cmos, sizeof base_cmos);
    board_init(&board, &rom, base_cmos);
    for (int i = 0; i < 2000; i++) board_run_frame(&board, NULL);
    memcpy(base_cmos, board.cmos, sizeof base_cmos);
    memcpy(base_ram, board.ram, sizeof base_ram);
    cmos_check_cases();
    tables_cases();
    cmos_test_cases();
    average_cases();
    bookkeeping_cases();
    adjust_cases();
    adjust_edit_cases();
    screens_cases();
    sound_switch_cases();
    if (failures) {
        fprintf(stderr, "test_operator: %d differences\n", failures);
        return 1;
    }
    printf("test_operator: OK\n");
    return 0;
}
