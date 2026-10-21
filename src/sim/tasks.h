/*
 * Joust's task kernel (the Williams cooperative scheduler), on the C game's
 * RAM (sim/game.h). Tasks are 40 blocks of $38 bytes at $A100-$A9BF, on a
 * list from <$0A ($A00A is the list's head, as if a block); free blocks are
 * on a list from <$08. A block is +0 the next block, +2 its type, +3 a flag
 * that runs it in the frame's second pass, +4 frames left to sleep, +5-6
 * where it resumes, then the task's own bytes. <$2E is the task running,
 * <$2C the block before it on the list, <$66 where the second pass resumes
 * next frame.
 *
 * The frame loop ($E0E2) runs two passes: the first ($E163) counts down
 * every task without the flag and runs those at zero; the second ($E180)
 * the flagged ones, from <$66 on, while the interrupt still has blits to
 * draw (the byte at [<$68] is not zero). A task runs from its resume
 * address until it sleeps or ends; the stores below are in the ROM's order.
 */
#ifndef JOUST_TASKS_H
#define JOUST_TASKS_H

#include <stdint.h>

#include "sim/game.h"

#define TASK_HEAD 0xA00A
#define TASK_FREE 0xA008
#define TASK_CURRENT 0xA02E
#define TASK_PREVIOUS 0xA02C
#define TASK_PASS2 0xA066
#define TASK_FIRST 0xA100
#define TASK_SIZE 0x38
#define TASKS 40

#define TASK_TYPE 2
#define TASK_PASS 3
#define TASK_SLEEP 4
#define TASK_RESUME 5

/* $E157 (through $E037, `LDA #ticks; JSR $E037`): the current task sleeps
   `ticks` frames and resumes at `resume` (the address after the call). */
StepResult task_sleep(Game *g, uint8_t ticks, uint16_t resume);
/* $E20F (through $E00F): the current task ends; its block goes to the free list. */
StepResult task_end(Game *g);
/* $E284 (through $E030): a new task after block `after`, starting at `start`
   in a frame (its sleep is 1). Returns its block. $E282 (through $E009) is
   the same after the current task. */
uint16_t task_create(Game *g, uint16_t after, uint16_t start, uint8_t type, uint8_t pass);
/* $E24D (through $E00C): ends every task but the current one whose type & mask == type. */
void task_kill_type(Game *g, uint8_t type, uint8_t mask);

#endif
