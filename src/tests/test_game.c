/*
 * The C game's task steps against the emulated board (label rom):
 *   test_game <joust.zip|folder> [runner|two]
 * Plays a scripted game on the board (a coin, a start, then player 1
 * running and flapping; `runner`: 12000 frames of mostly running on the
 * ground, a flap now and then, then idle to the game's end, and in the
 * game the secret code ($6B25) at frame 5000; `two`: two
 * coins, a 2-player start, both players playing, 9000 frames) while a C
 * model of all of RAM (sim/game.h) follows it:
 *   - every write the board makes (the CPU's and the blitter's) is copied
 *     to the model, except during a ported task step;
 *   - when the ROM's dispatcher runs a task whose step is ported ($E154,
 *     `JMP [5,U]`), the C step runs on a copy of the model, which gives
 *     the bytes it changes; as the ROM's step writes a byte, the model
 *     takes the C value for it;
 *   - when the ROM's step yields (back in the dispatcher at $E15F, after a
 *     sleep or an end), the model must equal RAM, and the ROM select too.
 * Interrupts run inside steps: their writes are copied as they happen. A
 * step an interrupt broke into may then differ (the interrupt can change
 * what the step reads, e.g. the screen under the blitter): it must match
 * RAM when the C step reads the state from before the interrupt or from
 * after it, each byte both wrote keeping the later write in the ROM's
 * order; or when each interrupt's writes land in the C step after as many
 * of its writes as the ROM's step had made. When none matches and the
 * bytes that differ include some the interrupt wrote, the ROM read part
 * of the state before the interrupt and part after: a race the C game,
 * which never interrupts a routine, cannot have; counted, at most 1 in
 * 1000 steps. So is a step that read (in C) a byte the interrupt wrote
 * (e.g. the random state the interrupt stirs, read before the stir and
 * rotated after it). Between steps the model must equal RAM at every frame (a
 * check of the copying). The stack page ($BE00-$BEFF) is never compared.
 * The CMOS is not RAM: the board's is loaded into the model at each step,
 * and the C step's must equal it when the ROM's step yields.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/rom.h"
#include "emu/board.h"
#include "sim/game.h"
#include "sim/tasks.h"

#define PRIME_FRAMES 1800
#define FRAMES 6000
#define RUNNER_FRAMES 12000
#define DISPATCH 0xE154      /* JMP [5,U] */
#define DISPATCH_NEXT 0xE15F /* where a sleep or an end comes back to */
#define STACK_FROM 0xBE00
#define STACK_TO 0xBF00

static RomSet rom;
static Board board;
static uint8_t data[0x10000];
static Game model, scratch, before, other, reads, merged;
static int failures;

static int stack(unsigned a) { return a >= STACK_FROM && a < STACK_TO; }

/* ---- the interrupt's writes during a step ---------------------------------- */

static struct { uint16_t addr; uint8_t value; } irq_writes[16384];
static int irq_count;
static uint32_t step_seq[GAME_RAM], irq_seq[GAME_RAM], seq; /* when each byte was last written */
static struct { long after; int first, count; uint16_t pc; } irqs[64]; /* where each interrupt fell in the step */
static int irq_n;
static long step_writes;
static uint16_t irq_s[8]; /* S before each interrupt in progress pushed */
static int irq_depth;

/* ---- the step being followed ---------------------------------------------- */

static uint16_t step_task, step_at;
static uint8_t predicted[GAME_RAM], has_prediction[GAME_RAM], written[GAME_RAM], predicted_romsel, predicted_crb;
static uint8_t c_read[GAME_RAM]; /* what the C step read */

static void read_hook(void *user, uint16_t addr)
{
    (void)user;
    c_read[addr] = 1;
}
static int predicted_mux;
static uint8_t predicted_cmos[1024];
static int step_active, step_interrupted;
static long steps, steps_unported, steps_matched, steps_reordered, steps_raced, frame_checks;
static struct { uint16_t at; long n, ported, matched; } by_resume[128];

static void load_model(void)
{
    game_load(&model, board.ram, board.romsel);
    model.in0 = board.in0;
    model.inp1 = board.inp1;
    model.inp2 = board.inp2;
    model.pia0_crb = board.pia[0].b.cr;
    model.mux = board.mux_select;
}

/* The first address where `m` differs from RAM, or -1 (-2: the ROM select
   or PIA 0's control register B). */
static long first_difference_of(const Game *m)
{
    if (memcmp(m->ram, board.ram, STACK_FROM))
        for (unsigned a = 0; a < STACK_FROM; a++)
            if (m->ram[a] != board.ram[a]) return a;
    if (memcmp(m->ram + STACK_TO, board.ram + STACK_TO, GAME_RAM - STACK_TO))
        for (unsigned a = STACK_TO; a < GAME_RAM; a++)
            if (m->ram[a] != board.ram[a]) return a;
    if ((m->romsel & 1) != (board.romsel & 1) || m->pia0_crb != board.pia[0].b.cr || m->mux != board.mux_select)
        return -2;
    return -1;
}

static void report(long a, const char *what)
{
    if (failures++ >= 8) return;
    if (a == -2)
        fprintf(stderr, "test_game: frame %lld: %s (task $%04X): ROM select %d, C %d; $C807 %02X, C %02X\n",
                (long long)board.frame, what, step_task, board.romsel & 1, model.romsel & 1, board.pia[0].b.cr,
                model.pia0_crb);
    else
        fprintf(stderr, "test_game: frame %lld: %s (task $%04X at $%04X): $%04lX = %02X, C %02X\n",
                (long long)board.frame, what, step_task, step_at, a, board.ram[a], model.ram[a]);
    if (getenv("DBG"))
        for (unsigned d = 0; d < GAME_RAM; d++)
            if (!stack(d) && model.ram[d] != board.ram[d])
                fprintf(stderr, "  $%04X ram %02X model %02X\n", d, board.ram[d], model.ram[d]);
}

static void apply_irq_writes(Game *m)
{
    for (int i = 0; i < irq_count; i++) game_poke(m, irq_writes[i].addr, irq_writes[i].value);
}

/* The C step run on `reads`, merged with the interrupt's writes in the ROM's order. */
static int merged_matches(void)
{
    other = reads;
    if (game_step(&other, step_task) == STEP_UNPORTED) return 0;
    merged = before;
    apply_irq_writes(&merged);
    for (unsigned a = 0; a < GAME_RAM; a++) {
        if (stack(a) || other.ram[a] == reads.ram[a]) continue;
        if (step_seq[a] && step_seq[a] > irq_seq[a]) merged.ram[a] = other.ram[a];
    }
    merged.romsel = other.romsel;
    merged.pia0_crb = other.pia0_crb;
    merged.mux = other.mux;
    return first_difference_of(&merged) == -1;
}

/* The C step with each interrupt's writes applied where it fell among the step's writes. */
static long c_writes;
static int next_irq, replaying;

static void replay_hook(void *user, uint16_t addr)
{
    Game *g = user;
    (void)addr;
    if (replaying) return;
    while (next_irq < irq_n && irqs[next_irq].after == c_writes) {
        replaying = 1;
        for (int i = irqs[next_irq].first; i < irqs[next_irq].first + irqs[next_irq].count; i++)
            game_poke(g, irq_writes[i].addr, irq_writes[i].value);
        replaying = 0;
        next_irq++;
    }
    c_writes++;
}

static int matches_interleaved(void)
{
    if (irq_n == (int)(sizeof irqs / sizeof irqs[0])) return 0;
    other = before;
    other.on_write = replay_hook;
    other.user = &other;
    c_writes = 0;
    next_irq = 0;
    StepResult r = game_step(&other, step_task);
    other.on_write = NULL;
    while (next_irq < irq_n) { /* interrupts after the step's last write */
        for (int i = irqs[next_irq].first; i < irqs[next_irq].first + irqs[next_irq].count; i++)
            game_poke(&other, irq_writes[i].addr, irq_writes[i].value);
        next_irq++;
    }
    return r != STEP_UNPORTED && first_difference_of(&other) == -1;
}

static int matches_reordered(void)
{
    if (irq_count == (int)(sizeof irq_writes / sizeof irq_writes[0])) return 0;
    reads = before; /* the step read before the interrupt */
    if (merged_matches()) return 1;
    apply_irq_writes(&reads); /* or after it */
    if (merged_matches()) return 1;
    return matches_interleaved(); /* or where it fell */
}

/* Did the interrupt during the step write a byte that differs, or one the C step read? */
static int irq_touched_difference(void)
{
    for (unsigned a = 0; a < GAME_RAM; a++)
        if (!stack(a) && irq_seq[a] && (model.ram[a] != board.ram[a] || c_read[a])) return 1;
    return 0;
}

static void count_resume(uint16_t at, int ported)
{
    for (int i = 0; i < 128; i++)
        if (by_resume[i].at == at || !by_resume[i].n) {
            by_resume[i].at = at;
            by_resume[i].n++;
            by_resume[i].ported += ported;
            return;
        }
}

static void start_step(uint16_t task)
{
    uint16_t at = game_word(&model, (uint16_t)(task + TASK_RESUME));
    int ported = game_step_ported(&model, task);
    count_resume(at, ported);
    if (!ported) return;
    step_task = task;
    step_at = at;
    steps++;
    memcpy(model.cmos, board.cmos, sizeof model.cmos); /* not RAM: the board's, at each step */
    model.in0 = board.in0; /* the controls change between frames: as the PIA shows them now */
    model.inp1 = board.inp1;
    model.inp2 = board.inp2;
    model.y = board.cpu.y;
    before = model;
    irq_count = irq_n = 0;
    step_writes = 0;
    memset(step_seq, 0, sizeof step_seq);
    memset(irq_seq, 0, sizeof irq_seq);
    seq = 0;
    scratch = model;
    memset(c_read, 0, sizeof c_read);
    scratch.on_read = read_hook;
    StepResult r = game_step(&scratch, task);
    scratch.on_read = NULL;
    if (r == STEP_UNPORTED) { /* a path not ported: the ROM's writes are copied */
        steps_unported++;
        return;
    }
    memset(has_prediction, 0, sizeof has_prediction);
    memset(written, 0, sizeof written);
    for (unsigned a = 0; a < GAME_RAM; a++)
        if (model.ram[a] != scratch.ram[a]) {
            has_prediction[a] = 1;
            predicted[a] = scratch.ram[a];
        }
    predicted_romsel = scratch.romsel;
    memcpy(predicted_cmos, scratch.cmos, sizeof predicted_cmos);
    predicted_crb = scratch.pia0_crb;
    predicted_mux = scratch.mux;
    step_active = 1;
    step_interrupted = 0;
}

static void end_step(void)
{
    step_active = 0;
    for (int i = 0; i < 1024; i++)
        if (predicted_cmos[i] != board.cmos[i]) {
            if (failures++ < 8)
                fprintf(stderr, "test_game: frame %lld: CMOS $%04X = %02X, C %02X (task $%04X at $%04X)\n",
                        (long long)board.frame, 0xCC00 + i, board.cmos[i], predicted_cmos[i], step_task, step_at);
            break;
        }
    model.romsel = predicted_romsel;
    model.pia0_crb = predicted_crb;
    model.mux = predicted_mux;
    long diff = -1;
    for (unsigned a = 0; a < GAME_RAM && diff == -1; a++)
        if (!stack(a) && has_prediction[a] && !written[a] && predicted[a] != board.ram[a]) diff = a;
    if (diff == -1) diff = first_difference_of(&model);
    if (diff == -1) {
        steps_matched++;
        for (int i = 0; i < 128 && by_resume[i].n; i++)
            if (by_resume[i].at == step_at) by_resume[i].matched++;
    } else if (step_interrupted && matches_reordered()) {
        steps_reordered++;
    } else if (step_interrupted && diff >= 0 && irq_touched_difference()) {
        if (steps_raced++ < 3) {
            fprintf(stderr, "test_game: frame %lld: a race (an interrupt inside the ROM's step $%04X at $%04X, $%04lX differs;"
                            " interrupted at", (long long)board.frame, step_task, step_at, diff);
            for (int k = 0; k < irq_n; k++) fprintf(stderr, " $%04X", irqs[k].pc);
            fprintf(stderr, ")\n");
        }
    } else {
        report(diff, step_interrupted ? "after an interrupted step" : "after a step");
    }
    load_model();
}

static void step_hook(Board *b)
{
    /* the board calls the hook also before an instruction the CPU then
       puts off for an interrupt; it comes again when the instruction runs */
    if ((b->cpu.irq && !(b->cpu.cc & CC_I)) || (b->cpu.firq && !(b->cpu.cc & CC_F))) return;
    if (b->cpu.last_interrupt && irq_depth < 8) {
        /* the stacked CC's E bit: the whole state (12 bytes) or PC and CC (3) */
        irq_s[irq_depth++] = (uint16_t)(b->cpu.s + (b->ram[b->cpu.s] & 0x80 ? 12 : 3));
        if (step_active) {
            step_interrupted = 1;
            if (irq_n < (int)(sizeof irqs / sizeof irqs[0])) {
                irqs[irq_n].after = step_writes;
                irqs[irq_n].first = irq_count;
                irqs[irq_n].count = 0;
                /* the PC it interrupted, stacked under CC, A, B, DP, X, Y, U */
                irqs[irq_n].pc = (uint16_t)(b->ram[(uint16_t)(b->cpu.s + 10)] << 8 | b->ram[(uint16_t)(b->cpu.s + 11)]);
                irq_n++;
            }
        }
        return;
    }
    if (irq_depth && b->cpu.s == irq_s[irq_depth - 1]) irq_depth--; /* returned (RTI) */
    if (irq_depth) return;
    uint16_t pc = b->cpu.pc;
    if (pc == DISPATCH && !step_active) start_step(b->cpu.u);
    else if (pc == DISPATCH_NEXT && step_active) end_step();
}

static void write_hook(Board *b, uint16_t a, uint8_t v)
{
    (void)b;
    if (a == 0xC900 || a == 0xC807) { /* the I/O the model keeps: copied, as RAM, outside ported steps */
        if (!step_active || irq_depth) {
            if (a == 0xC900) model.romsel = v;
            else game_poke(&model, a, v);
        }
        return;
    }
    if (a >= GAME_RAM) return;
    if (!step_active || irq_depth || stack(a)) {
        model.ram[a] = v;
        if (step_active && !stack(a)) {
            irq_seq[a] = ++seq;
            if (irq_count < (int)(sizeof irq_writes / sizeof irq_writes[0])) {
                irq_writes[irq_count].addr = a;
                irq_writes[irq_count].value = v;
                irq_count++;
                if (irq_n) irqs[irq_n - 1].count++;
            }
        }
        return;
    }
    written[a] = 1;
    step_seq[a] = ++seq;
    step_writes++;
    if (has_prediction[a]) model.ram[a] = predicted[a];
    /* else C did not write it: the model keeps its byte, and differs if the ROM's does */
}

static int runner, two;

/* A coin, a 1-player start, then player 1 runs right and left and flaps
   (the runner: runs, flaps one frame in 200, idle after 8000). */
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
    if (runner) {
        /* the secret code ($6B25), in the game */
        if (f >= 5000 && f < 5040) {
            board.in0 = IN0_START1;
            board.inp1 = INP_LEFT | INP_FLAP;
            board.inp2 = INP_RIGHT;
        } else if (f >= 5040 && f < 5080) {
            board.in0 = IN0_START2;
            board.inp1 = INP_RIGHT;
            board.inp2 = INP_FLAP;
        } else if (f >= 5080 && f < 5120) {
            board.inp1 = INP_LEFT | INP_FLAP;
            board.inp2 = INP_RIGHT | INP_FLAP;
        }
        if (f < 8000 && (f < 5000 || f >= 5120))
            board.inp1 = (uint8_t)((f % 300 < 140 ? INP_RIGHT : f % 300 < 150 ? 0 : f % 300 < 290 ? INP_LEFT : 0) |
                                   (f % 200 == 0 ? INP_FLAP : 0));
        return;
    }
    board.inp1 = (uint8_t)((f % 90 < 40 ? INP_RIGHT : f % 90 < 70 ? INP_LEFT : 0) | (f % 12 < 3 ? INP_FLAP : 0));
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "runner") && strcmp(argv[2], "two"))) {
        fprintf(stderr, "usage: test_game <joust.zip|folder> [runner|two]\n");
        return 2;
    }
    runner = argc == 3 && !strcmp(argv[2], "runner");
    two = argc == 3 && !strcmp(argv[2], "two");
    char err[512];
    if (rom_load(&rom, argv[1], err, sizeof err)) {
        fprintf(stderr, "test_game: %s\n", err);
        return 1;
    }
    for (unsigned a = 0; a < 0x10000; a++) {
        int v = rom_byte(&rom, a);
        data[a] = v < 0 ? 0 : (uint8_t)v;
    }
    model.rom = scratch.rom = data;
    static uint8_t cmos[BOARD_CMOS_SIZE];
    board_init(&board, &rom, NULL);
    for (int i = 0; i < PRIME_FRAMES; i++) board_run_frame(&board, NULL);
    memcpy(cmos, board.cmos, sizeof cmos);
    board_init(&board, &rom, cmos);
    load_model();
    board.step_hook = step_hook;
    board.write_hook = write_hook;
    for (long f = 1; f <= (runner ? RUNNER_FRAMES : two ? 9000 : FRAMES); f++) {
        controls(f);
        board_run_frame(&board, NULL);
        if (!step_active) {
            frame_checks++;
            long d = first_difference_of(&model);
            if (d != -1) {
                report(d, "between steps");
                load_model();
            }
        }
    }
    printf("test_game: %ld steps of ported tasks run: %ld matched, %ld matched with an interrupt's writes\n"
           "           reordered, %ld took paths not yet ported; %ld frames compared\n",
           steps, steps_matched, steps_reordered, steps_unported, frame_checks);
    printf("test_game: races (an interrupt inside a step, state read partly before it): %ld\n", steps_raced);
    if (steps_raced * 1000 > steps) {
        fprintf(stderr, "test_game: too many races\n");
        failures++;
    }
    long all = 0, ported = 0;
    printf("test_game: not ported, by where they resume:");
    for (int i = 0; i < 128 && by_resume[i].n; i++) {
        all += by_resume[i].n;
        ported += by_resume[i].ported;
        if (!by_resume[i].ported) printf(" $%04X %ld", by_resume[i].at, by_resume[i].n);
    }
    printf("\ntest_game: ported, by where they resume (matched/run):");
    for (int i = 0; i < 128 && by_resume[i].n; i++)
        if (by_resume[i].ported) printf(" $%04X %ld/%ld", by_resume[i].at, by_resume[i].matched, by_resume[i].n);
    printf("\ntest_game: %ld of %ld task steps ported\n", ported, all);
    if (steps_matched < 100) {
        fprintf(stderr, "test_game: too few steps matched\n");
        return 1;
    }
    if (failures) {
        fprintf(stderr, "test_game: %d differences\n", failures);
        return 1;
    }
    printf("test_game: OK\n");
    return 0;
}
