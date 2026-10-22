#include "sim/cgame.h"

#include "sim/irq.h"

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

FrameResult cgame_frame(CGame *c, uint16_t *task)
{
    FrameHooks h = { c, interrupts, NULL };
    c->g->beam = NULL;
    return frame_run(c->g, &h, task);
}
