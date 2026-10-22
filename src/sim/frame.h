/*
 * The frame loop ($E0E2-$E20C) in C: this frame's display list ($A9C0 or
 * $AC21 by <$27, its nodes from +9), the queued sound sent ($C80E: $FF,
 * then the command, when <$61 runs out; the script at <$63 gives the
 * next command and its frames), the layers' chains emptied, then the
 * dispatcher's two passes over the task list: the first runs each task
 * without the pass flag (+3) whose sleep (+4) runs out; the second, from
 * where it stopped (<$66), the flagged ones while the interrupt still
 * draws the list before ([<$68] set). Last the layers are chained into
 * the list ($E197: band b's chain 0, then band b - 1's chain 1; band 3
 * also its own chain 1) and the list marked ready.
 *
 * On the board the interrupt comes four times a frame among the steps
 * (sim/irq.c), and how far the second pass gets depends on the beam: the
 * hooks give both (a test, the board's; the C game alone, its schedule).
 */
#ifndef JOUST_FRAME_H
#define JOUST_FRAME_H

#include "sim/game.h"

#define FRAME_START 1 /* (not a task block) */
#define FRAME_END 0

typedef struct {
    void *ctx;
    /* at the frame's start (FRAME_START), before each task step (its
       block) and after the passes, before the list is chained and marked
       ready (FRAME_END): run the interrupts that come there (irq_run) */
    void (*interrupts)(void *ctx, Game *g, uint16_t task);
    /* the second pass, about to look at the task after `prev`: go on? */
    int (*pass2_on)(void *ctx, Game *g, uint16_t prev);
} FrameHooks;

typedef enum {
    FRAME_DONE = 0,
    FRAME_UNPORTED = 1, /* a task step not ported: the state is unspecified */
    FRAME_HALT = 2,     /* a step left the kernel (Game.halt) */
} FrameResult;

/* One frame of the loop. *unported (if not NULL) gets the task whose step
   was not ported. */
FrameResult frame_run(Game *g, const FrameHooks *h, uint16_t *unported);

#endif
