/*
 * The C game on its own (`joust --c-game`): sim/frame.c's frame with the
 * interrupt on a fixed schedule (the board's comes four times a frame
 * among the steps; here at the frame's start with the beam at line 0,
 * drawing the last band of the list before and doing the top-of-frame
 * work, and after the passes at lines $40, $80 and $C0, drawing the
 * list's first three bands), so the second pass always runs to the
 * list's end. The picture is taken when the frame starts, after its
 * interrupt: the whole list before drawn.
 */
#ifndef JOUST_CGAME_H
#define JOUST_CGAME_H

#include "sim/frame.h"
#include "sim/game.h"

typedef struct {
    Game *g;
    /* called at each frame's start once the list before is drawn (the
       picture to show) */
    void (*picture)(void *user, const Game *g);
    void *user;
    int advance; /* the advance switch was pressed (the test mode: not ported) */
} CGame;

/* $E03D-$E0E0: the state at the frame loop's first start after a reset
   (RAM cleared to $BF7F, the ROM selected, PIA 0's multiplexer, the
   switches' debounce, the task blocks linked, the display lists, the
   palette from $E5FA, the minute's count, the credits from the CMOS's
   copy, the first task $5ED0). The CMOS (g->cmos) as it is. */
void cgame_power_on(Game *g);
/* One frame; FRAME_UNPORTED if a step was not ported (*task the task). */
FrameResult cgame_frame(CGame *c, uint16_t *task);

#endif
