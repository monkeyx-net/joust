/*
 * Controls, sampled once per 60 Hz frame. Joust is two players at once, each
 * with a 2-way joystick and a flap button. Pads go through SDL_GameController
 * (SDL_GAMECONTROLLERCONFIG / gamecontrollerdb.txt): the first pad is player
 * 1, the second player 2.
 *
 *            pad                          keyboard (P1 / P2)
 *   move     left stick, D-pad            Left Right / A D
 *   flap     A, B, X or Y                 Space or Ctrl / W
 *   coin     Select                       5
 *   start    Start (1P), L1+Start (2P)    1 or Enter / 2
 *   menu     Select+R1                    F2
 *   graphics Select+L1                    F3 (F4 glow, F5 scanlines)
 *   quit     hold Select+Start 1 s        Esc
 */
#ifndef JOUST_INPUT_H
#define JOUST_INPUT_H

#include <stdbool.h>

/* Frames Select+Start must be held to quit (1 s). */
#define QUIT_HOLD_FRAMES 60
#define PLAYERS 2

typedef struct {
    int move_x;          /* -1, 0, 1 */
    bool flap;
} PlayerInput;

typedef struct {
    PlayerInput p[PLAYERS];
    bool coin;
    bool start;          /* 1-player start */
    bool start2;         /* 2-player start */
    bool operator_menu;  /* Select+R1 or F2, edge-triggered */
    bool menu_held;      /* the same, while held */
    bool quit;           /* Select+Start held QUIT_HOLD_FRAMES, Esc, or window close */
    bool gfx_next;       /* Select+L1 or F3, edge-triggered: next graphics preset */
    bool gfx_glow;       /* F4, edge-triggered */
    bool gfx_scanlines;  /* F5, edge-triggered */
    int pads;            /* game controllers connected */
} InputFrame;

void input_init(void);
/* Pump SDL events and build this frame's input. */
void input_poll(InputFrame *out);
void input_shutdown(void);

#endif
