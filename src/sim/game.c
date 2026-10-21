#include "sim/game.h"

#include <string.h>

#include "sim/checksum.h"
#include "sim/lava.h"
#include "sim/prng.h"
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
    if (addr < GAME_RAM) return g->ram[addr];
    return g->rom[addr];
}

uint16_t game_read_word(const Game *g, uint16_t addr)
{
    return (uint16_t)(game_read(g, addr) << 8 | game_read(g, (uint16_t)(addr + 1)));
}

uint8_t game_peek(const Game *g, uint16_t addr) { return addr < GAME_RAM ? g->ram[addr] : 0; }

void game_poke(Game *g, uint16_t addr, uint8_t value)
{
    if (g->on_write) g->on_write(g->user, addr);
    if (addr < GAME_RAM) g->ram[addr] = value;
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

uint8_t game_random(Game *g, int *carry)
{
    Prng p = { g->ram[RNG], g->ram[RNG + 1] };
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
    return checksum_ported(at) || lava_ported(at);
}

StepResult game_step(Game *g, uint16_t task)
{
    uint16_t at = game_word(g, (uint16_t)(task + TASK_RESUME));
    if (checksum_ported(at)) return checksum_step(g, task, at);
    if (lava_ported(at)) return lava_step(g, task, at);
    return STEP_UNPORTED;
}
