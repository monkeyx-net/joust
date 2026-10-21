#include "sim/game.h"

#include <string.h>

#include "gfx/williams.h"

#include "sim/checksum.h"
#include "sim/collide.h"
#include "sim/flow.h"
#include "sim/lava.h"
#include "sim/prng.h"
#include "sim/rider.h"
#include "sim/scores.h"
#include "sim/tasks.h"

#define RNG 0xA00D          /* <$0D-<$0E */
#define SOUND_TIMER 0xA061  /* <$61: frames until the queued sound goes out */
#define SOUND_PRIORITY 0xA062
#define SOUND_SCRIPT 0xA063
#define SOUND_GATE 0xA090   /* <$90: bit 7 clear, only sounds of priority bit 7 play */
#define ROMSEL_COPY 0xBFFF

void game_load(Game *g, const uint8_t *ram, uint8_t romsel)
{
    memcpy(g->ram, ram, GAME_RAM);
    g->romsel = romsel;
}

uint8_t game_read(const Game *g, uint16_t addr)
{
    if (addr < 0x9000 && (g->romsel & 1)) return g->rom[addr];
    if (addr < GAME_RAM) return game_peek(g, addr);
    if (addr >= 0xD000) return g->rom[addr];
    if (addr == 0xC804) return (uint8_t)(((g->mux ? g->inp1 : g->inp2) & 0x0F) | (g->in0 & 0x30));
    if (addr == 0xC807) return g->pia0_crb;
    return 0;
}

uint16_t game_read_word(const Game *g, uint16_t addr)
{
    return (uint16_t)(game_read(g, addr) << 8 | game_read(g, (uint16_t)(addr + 1)));
}

uint8_t game_peek(const Game *g, uint16_t addr)
{
    if (addr >= GAME_RAM) return 0;
    if (g->on_read) g->on_read(g->user, addr);
    return g->ram[addr];
}

void game_poke(Game *g, uint16_t addr, uint8_t value)
{
    if (addr == 0xC807) { /* as pia6821.c: the IRQ flags stay; CB2 follows bit 3 in its manual output mode */
        g->pia0_crb = (uint8_t)((g->pia0_crb & 0xC0) | (value & 0x3F));
        if ((value & 0x30) == 0x30) g->mux = (value >> 3) & 1;
        return;
    }
    if (addr >= GAME_RAM) return;
    if (g->on_write) g->on_write(g->user, addr);
    g->ram[addr] = value;
}

uint16_t game_word(const Game *g, uint16_t addr)
{
    return (uint16_t)(game_peek(g, addr) << 8 | game_peek(g, (uint16_t)(addr + 1)));
}

void game_set_word(Game *g, uint16_t addr, uint16_t value)
{
    game_poke(g, addr, (uint8_t)(value >> 8));
    game_poke(g, (uint16_t)(addr + 1), (uint8_t)value);
}

void game_select_rom(Game *g, uint8_t value)
{
    game_poke(g, ROMSEL_COPY, value);
    g->romsel = value;
}

static void blit_wrote(void *ctx, unsigned addr, uint8_t value)
{
    Game *g = ctx;
    (void)value;
    if (g->on_write) g->on_write(g->user, (uint16_t)addr);
}

void game_blit(Game *g, uint8_t ctrl, uint8_t solid, uint16_t src, uint16_t dst, uint8_t w_reg, uint8_t h_reg)
{
    WilliamsBus bus = { g->ram, NULL, g->romsel & 1, blit_wrote, g, g->rom };
    williams_blit(&bus, ctrl, solid, src, dst, w_reg, h_reg);
}

uint16_t game_controls(Game *g)
{
    game_poke(g, 0xC807, game_read(g, 0xC807) | 0x08);
    uint8_t a = game_read(g, 0xC804) & 0x37;
    game_poke(g, 0xC807, game_read(g, 0xC807) & 0xF7);
    uint8_t b = game_read(g, 0xC804) & 0x07;
    return (uint16_t)(a << 8 | b);
}

uint8_t game_random(Game *g, int *carry)
{
    Prng p = { game_peek(g, RNG), game_peek(g, RNG + 1) };
    uint8_t a = prng_step(&p, carry);
    game_poke(g, RNG, p.hi); /* ROR <$0D, then ROR <$0E */
    game_poke(g, RNG + 1, p.lo);
    return a;
}

void game_sound(Game *g, uint16_t script)
{
    uint8_t priority = game_read(g, script);
    if (!(priority & 0x80) && !(game_peek(g, SOUND_GATE) & 0x80)) return;
    if (game_peek(g, SOUND_TIMER) && priority < game_peek(g, SOUND_PRIORITY)) return;
    game_poke(g, SOUND_PRIORITY, priority);
    game_poke(g, SOUND_TIMER, 1);
    game_set_word(g, SOUND_SCRIPT, (uint16_t)(script + 1));
}

int game_step_ported(const Game *g, uint16_t task)
{
    uint16_t at = game_word(g, (uint16_t)(task + TASK_RESUME));
    return checksum_ported(at) || lava_ported(at) || collide_ported(at) || scores_ported(at) || flow_ported(at) || rider_ported(at);
}

StepResult game_step(Game *g, uint16_t task)
{
    uint16_t at = game_word(g, (uint16_t)(task + TASK_RESUME));
    if (checksum_ported(at)) return checksum_step(g, task, at);
    if (lava_ported(at)) return lava_step(g, task, at);
    if (collide_ported(at)) return collide_step(g, task, at);
    if (scores_ported(at)) return scores_step(g, task, at);
    if (flow_ported(at)) return flow_step(g, task, at);
    if (rider_ported(at)) return rider_step(g, task, at);
    return STEP_UNPORTED;
}
