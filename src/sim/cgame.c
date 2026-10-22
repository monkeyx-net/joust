#include "sim/cgame.h"

#include "sim/irq.h"
#include "sim/tasks.h"

#include <stddef.h>

#define IRQ_S 0xBEF2 /* S as the interrupt saves it (the board's is about this) */

static void irq(CGame *c, uint8_t line)
{
    c->g->vcount = line;
    c->g->irq_s = IRQ_S;
    if (irq_run(c->g)) c->advance = 1;
}

static void interrupts(void *ctx, Game *g, uint16_t task)
{
    CGame *c = ctx;
    (void)g;
    if (task == FRAME_START) {
        irq(c, 0x00);
        if (c->picture) c->picture(c->user, c->g);
    } else if (task == FRAME_END) {
        irq(c, 0x40);
        irq(c, 0x80);
        irq(c, 0xC0);
    }
}

void cgame_start(CGame *c)
{
    operator_reset(&c->op);
    c->operating = 1;
}

FrameResult cgame_frame(CGame *c, uint16_t *task)
{
    if (c->operating) {
        operator_frame(&c->op, c->g);
        if (c->picture) c->picture(c->user, c->g);
        if (c->op.done) { /* the reboot ($E000) */
            c->operating = 0;
            cgame_power_on(c->g);
        }
        return FRAME_DONE;
    }
    FrameHooks h = { c, interrupts, NULL };
    c->g->beam = NULL;
    FrameResult r = frame_run(c->g, &h, task);
    if (c->advance) { /* $F342 */
        c->advance = 0;
        operator_test_mode(&c->op);
        c->operating = 1;
    }
    return r;
}

void cgame_power_on(Game *g)
{
    for (uint16_t a = 0; a < 0xBF80; a++) game_poke(g, a, 0);
    game_select_rom(g, 1);
    game_poke(g, 0xC807, 0x00); /* PIA 0: the controls, the multiplexer (CB2) high */
    game_poke(g, 0xC807, 0x34);
    game_poke(g, 0xA08B, 0x3C);
    game_poke(g, 0xA0B4, 0x3C);
    game_set_word(g, TASK_FREE, TASK_FIRST); /* the blocks linked, the last to $A9C0 */
    for (uint16_t u = TASK_FIRST; u < 0xA9C0; u = (uint16_t)(u + TASK_SIZE)) game_set_word(g, u, (uint16_t)(u + TASK_SIZE));
    game_set_word(g, 0xA025, 0xA9C0);
    game_set_word(g, 0xA068, 0xAC21);
    for (int i = 0; i < 16; i++) game_poke(g, (uint16_t)(0xA00F + i), game_read(g, (uint16_t)(0xE5FA + i)));
    game_set_word(g, 0xA099, 0x0E10);
    uint8_t b = game_cmos_byte(g, 0xCD00); /* the credits, if the CMOS's copy is sane */
    if (b > 0x20 || (b & 0x0F) > 9) {
        b = 0;
        game_poke(g, 0xCD01, b);
        game_poke(g, 0xCD00, (uint8_t)(b >> 4));
    }
    game_poke(g, 0xA0F2, b);
    task_create(g, TASK_HEAD, 0x5ED0, 0x00, 0x00);
}
