/*
 * The C game on its own against the board, frame by frame (label rom):
 *   test_frame <joust.zip|folder> [two]
 * The board plays the scripted game (as test_game's); from a frame
 * boundary of the ROM's loop ($E0E2) the C game is loaded from its RAM
 * and then runs whole frames itself (sim/frame.c: both passes, every task
 * step in C, the interrupt in C, sim/irq.c). A hook on the board records
 * what timing decides there: where the interrupts came among the steps
 * (with S and the beam's lines they read), the controls at each step's
 * dispatch and where the second pass stopped; the C frame is given the
 * same. All of RAM (but the stack page) and the CMOS must then equal the
 * board's at its next $E0E2. An interrupt inside a ROM step is given to
 * the C game after that step: frames where that matters (the random
 * stir inside a random step) differ; they are counted, the C game is
 * loaded again and goes on. At least 97% of the frames must match.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/rom.h"
#include "emu/board.h"
#include "sim/frame.h"
#include "sim/game.h"
#include "sim/irq.h"
#include "sim/tasks.h"

#define LOOP 0xE0E2
#define DISPATCH 0xE154
#define IRQ 0xE38D
#define IRQ_TOP_BEAM 0xE3A7  /* after LDB $CB00 */
#define IRQ_BAND_BEAM 0xE466 /* after EORA $CB00 */
#define PASS2_STOPPED 0xE196 /* [<$68] was 0 */
#define PASS2_END 0xE197
#define PRIME_FRAMES 1800
#define STACK_FROM 0xBE00
#define STACK_TO 0xBF00

static RomSet rom;
static Board board;
static uint8_t data[0x10000];
static Game model;
static int two;

enum { EV_IRQ, EV_DISPATCH };
typedef struct {
    int type;
    uint16_t task, s;
    uint8_t beams[8], nbeams;
    uint8_t in0, inp1, inp2, in2;
} Event;

static Event events[512];
static int nevents, overflow;
static uint16_t pass2_stop; /* the block the second pass stopped after, or 0 at the list's end */
static int pass2_by_list;   /* it stopped because the list before was drawn */
static int recording;
static uint8_t last_beam_a;

static void step_hook(Board *b)
{
    if ((b->cpu.irq && !(b->cpu.cc & CC_I)) || (b->cpu.firq && !(b->cpu.cc & CC_F))) return;
    if (!recording) return;
    uint16_t pc = b->cpu.pc;
    Event *e = nevents ? &events[nevents - 1] : NULL;
    if (pc == IRQ) {
        if (nevents == 512) {
            overflow = 1;
            return;
        }
        e = &events[nevents++];
        memset(e, 0, sizeof *e);
        e->type = EV_IRQ;
        e->s = b->cpu.s;
        e->in0 = b->in0;
        e->inp1 = b->inp1;
        e->inp2 = b->inp2;
        e->in2 = b->in2;
    } else if (pc == IRQ_TOP_BEAM && e && e->type == EV_IRQ && e->nbeams < 8) {
        e->beams[e->nbeams++] = b->cpu.b;
    } else if (pc == 0xE463) {
        last_beam_a = b->cpu.a;
    } else if (pc == IRQ_BAND_BEAM && e && e->type == EV_IRQ && e->nbeams < 8) {
        e->beams[e->nbeams++] = (uint8_t)(b->cpu.a ^ last_beam_a);
    } else if (pc == DISPATCH) {
        if (nevents == 512) {
            overflow = 1;
            return;
        }
        e = &events[nevents++];
        memset(e, 0, sizeof *e);
        e->type = EV_DISPATCH;
        e->task = b->cpu.u;
        e->in0 = b->in0;
        e->inp1 = b->inp1;
        e->inp2 = b->inp2;
        e->in2 = b->in2;
    } else if (pc == PASS2_STOPPED) {
        pass2_by_list = 1;
    } else if (pc == PASS2_END) {
        pass2_stop = b->cpu.u;
    }
}

/* ---- the C frame, given the board's timing ---- */

static int next_event, mismatch_dispatch;
static const Event *irq_now;
static int beam_at;

static uint8_t beam(void *user)
{
    (void)user;
    if (irq_now && beam_at < irq_now->nbeams) return irq_now->beams[beam_at++];
    return irq_now && irq_now->nbeams ? irq_now->beams[irq_now->nbeams - 1] : 0;
}

static void run_irqs_until_dispatch(Game *g)
{
    while (next_event < nevents && events[next_event].type == EV_IRQ) {
        irq_now = &events[next_event++];
        beam_at = 0;
        g->irq_s = irq_now->s;
        g->in0 = irq_now->in0;
        g->inp1 = irq_now->inp1;
        g->inp2 = irq_now->inp2;
        g->in2 = irq_now->in2;
        irq_run(g);
        irq_now = NULL;
    }
}

static int watching;

static void interrupts(void *ctx, Game *g, uint16_t task)
{
    (void)ctx;
    if (watching) fprintf(stderr, "  hook %04X at event %d, [A9C0] %02X\n", task, next_event, g->ram[0xA9C0]);
    run_irqs_until_dispatch(g);
    if (task == FRAME_START || task == FRAME_END) return;
    if (next_event >= nevents || events[next_event].task != task) {
        mismatch_dispatch = 1;
        return;
    }
    const Event *e = &events[next_event++];
    g->in0 = e->in0;
    g->inp1 = e->inp1;
    g->inp2 = e->inp2;
    g->in2 = e->in2;
}

static int pass2_on(void *ctx, Game *g, uint16_t prev)
{
    (void)ctx;
    (void)g;
    if (watching) fprintf(stderr, "  pass2 at %04X: stop %04X by list %d\n", prev, pass2_stop, pass2_by_list);
    return !(pass2_by_list && prev == pass2_stop);
}

static void load_model(void)
{
    game_load(&model, board.ram, board.romsel);
    memcpy(model.cmos, board.cmos, sizeof model.cmos);
    model.in0 = board.in0;
    model.inp1 = board.inp1;
    model.inp2 = board.inp2;
    model.in2 = board.in2;
    model.pia0_crb = board.pia[0].b.cr;
    model.mux = board.mux_select;
}

static long first_difference(void)
{
    for (unsigned a = 0; a < GAME_RAM; a++)
        if ((a < STACK_FROM || a >= STACK_TO) && model.ram[a] != board.ram[a]) return a;
    if (memcmp(model.cmos, board.cmos, sizeof model.cmos)) return -2;
    return -1;
}

static void controls(long f)
{
    board.inp1 = board.inp2 = 0;
    if (two) {
        board.in2 = (f >= 1200 && f < 1215) || (f >= 1350 && f < 1365) ? IN2_COIN1 : 0;
        board.in0 = f >= 1500 && f < 1515 ? IN0_START2 : 0;
        if (f <= 1600) return;
        board.inp1 = (uint8_t)((f % 90 < 40 ? INP_RIGHT : f % 90 < 70 ? INP_LEFT : 0) | (f % 12 < 3 ? INP_FLAP : 0));
        board.inp2 = (uint8_t)((f % 130 < 60 ? INP_LEFT : f % 130 < 100 ? INP_RIGHT : 0) | (f % 17 < 3 ? INP_FLAP : 0));
        return;
    }
    board.in2 = f >= 1200 && f < 1215 ? IN2_COIN1 : 0;
    board.in0 = f >= 1300 && f < 1315 ? IN0_START1 : 0;
    if (f <= 1400) return;
    board.inp1 = (uint8_t)((f % 90 < 40 ? INP_RIGHT : f % 90 < 70 ? INP_LEFT : 0) | (f % 12 < 3 ? INP_FLAP : 0));
}

static long frames, matched, run, longest, mismatched, unported_frames, dispatch_mismatches;
static int failures, start_frame, loaded;
static long board_frame;

/* the C frame for the loop frame just recorded, then the comparison */
static void c_frame(void)
{
    FrameHooks h = { NULL, interrupts, pass2_on };
    next_event = 0;
    mismatch_dispatch = 0;
    uint16_t bad = 0;
    watching = getenv("WATCH") && labs(board_frame - atol(getenv("WATCH"))) < 2;
    FrameResult r = overflow ? FRAME_UNPORTED : frame_run(&model, &h, &bad);
    watching = 0;
    frames++;
    if (getenv("WATCH") && labs(board_frame - atol(getenv("WATCH"))) < 4) {
        fprintf(stderr, "frame %ld: board A9C0 %02X AC21 %02X A023-26 %02X %02X%02X %02X%02X 27 %02X; C A9C0 %02X AC21 %02X A023-26 %02X %02X%02X %02X%02X 27 %02X; events:",
                board_frame, board.ram[0xA9C0], board.ram[0xAC21], board.ram[0xA023], board.ram[0xA024], board.ram[0xA025],
                board.ram[0xA026], 0, board.ram[0xA027], model.ram[0xA9C0], model.ram[0xAC21], model.ram[0xA023], model.ram[0xA024],
                model.ram[0xA025], model.ram[0xA026], 0, model.ram[0xA027]);
        for (int k = 0; k < nevents; k++)
            if (events[k].type == EV_IRQ) {
                fprintf(stderr, " IRQ(");
                for (int j = 0; j < events[k].nbeams; j++) fprintf(stderr, "%s%02X", j ? "," : "", events[k].beams[j]);
                fprintf(stderr, ")");
            } else fprintf(stderr, " %04X", events[k].task);
        fprintf(stderr, "\n");
    }
    long d = r == FRAME_DONE && !mismatch_dispatch ? first_difference() : -3;
    if (d == -1) {
        matched++;
        if (++run > longest) longest = run;
    } else {
        if (r != FRAME_DONE) unported_frames++;
        else if (mismatch_dispatch) dispatch_mismatches++;
        else mismatched++;
        if (getenv("DBG") && mismatched + dispatch_mismatches + unported_frames <= 12) {
            if (d >= 0) {
                fprintf(stderr, "test_frame: board frame %ld: $%04lX = %02X, C %02X (after %ld matching); events:", board_frame,
                        d, board.ram[d], model.ram[d], run);
                for (int k = 0; k < nevents; k++)
                    if (events[k].type == EV_IRQ) {
                        fprintf(stderr, " IRQ(");
                        for (int j = 0; j < events[k].nbeams; j++) fprintf(stderr, "%s%02X", j ? "," : "", events[k].beams[j]);
                        fprintf(stderr, ")");
                    }
                    else fprintf(stderr, " %04X", events[k].task);
                fprintf(stderr, "\n  F2-F4 board %02X %02X %02X, C %02X %02X %02X; B3 %02X/%02X", board.ram[0xA0F2], board.ram[0xA0F3],
                        board.ram[0xA0F4], model.ram[0xA0F2], model.ram[0xA0F3], model.ram[0xA0F4], board.ram[0xA0B3], model.ram[0xA0B3]);
                fprintf(stderr, "\n  A021-A026 board");
                for (int k = 0x21; k <= 0x26; k++) fprintf(stderr, " %02X", board.ram[0xA000 + k]);
                fprintf(stderr, ", C");
                for (int k = 0x21; k <= 0x26; k++) fprintf(stderr, " %02X", model.ram[0xA000 + k]);
                fprintf(stderr, "\n");
            }
            else
                fprintf(stderr, "test_frame: board frame %ld: %s (task $%04X)\n", board_frame,
                        d == -2 ? "the CMOS differs" : r != FRAME_DONE ? "a step not ported" : "another task dispatched",
                        bad);
        }
        run = 0;
        load_model();
    }
}

static void loop_hook(Board *b)
{
    if ((b->cpu.irq && !(b->cpu.cc & CC_I)) || (b->cpu.firq && !(b->cpu.cc & CC_F))) return;
    if (b->cpu.pc == LOOP && board_frame >= start_frame) {
        if (loaded) c_frame();
        else {
            load_model();
            loaded = 1;
        }
        nevents = 0;
        overflow = 0;
        pass2_stop = 0;
        pass2_by_list = 0;
        recording = 1;
    }
    step_hook(b);
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "two"))) {
        fprintf(stderr, "usage: test_frame <joust.zip|folder> [two]\n");
        return 2;
    }
    two = argc == 3;
    char err[512];
    if (rom_load(&rom, argv[1], err, sizeof err)) {
        fprintf(stderr, "test_frame: %s\n", err);
        return 1;
    }
    for (unsigned a = 0; a < 0x10000; a++) {
        int v = rom_byte(&rom, a);
        data[a] = v < 0 ? 0 : (uint8_t)v;
    }
    model.rom = data;
    model.beam = beam;
    static uint8_t cmos[BOARD_CMOS_SIZE];
    board_init(&board, &rom, NULL);
    for (int i = 0; i < PRIME_FRAMES; i++) board_run_frame(&board, NULL);
    memcpy(cmos, board.cmos, sizeof cmos);
    board_init(&board, &rom, cmos);
    board.step_hook = loop_hook;
    start_frame = 100;
    for (board_frame = 1; board_frame <= 9000; board_frame++) {
        controls(board_frame);
        board_run_frame(&board, NULL);
    }
    printf("test_frame: %ld frames of the C game alone: %ld matched (longest run %ld); %ld differed, %ld dispatched"
           " another task, %ld reached a step not ported\n",
           frames, matched, longest, mismatched, dispatch_mismatches, unported_frames);
    if (frames < 1000 || matched * 100 < frames * 97) {
        fprintf(stderr, "test_frame: too few frames matched\n");
        failures++;
    }
    if (failures) return 1;
    printf("test_frame: OK\n");
    return 0;
}
