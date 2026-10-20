/*
 * The user's Joust ROM set (MAME `joust`, white/green label), read from a
 * folder or a zip. Files are matched by CRC-32, so any naming works (MAME's
 * current names, older sets, FBNeo).
 *
 * CPU map of the program ROMs (Williams first generation, as Robotron):
 * $0000-$8FFF = 12 x 4 KB banks 1-9 under video RAM (read when $C900 bit 0
 * selects ROM), $D000-$FFFF = banks 10-12. The sound board's 6808 runs the
 * sound ROM at $F000.
 */
#ifndef JOUST_ROM_H
#define JOUST_ROM_H

#include <stddef.h>
#include <stdint.h>

#define ROM_PROG_BANKS 12
#define ROM_SIZE 4096

typedef struct {
    uint8_t prog[ROM_PROG_BANKS][ROM_SIZE];
    uint8_t sound[ROM_SIZE];
    int loaded;
} RomSet;

/* Load the 12 program ROMs and the sound ROM from a folder or a zip. Returns 0,
   or -1 with a message in err. */
int rom_load(RomSet *rom, const char *src, char *err, int errlen);
/* Byte at a main-CPU address in program ROM ($0000-$8FFF, $D000-$FFFF); -1 elsewhere. */
int rom_byte(const RomSet *rom, unsigned addr);

/* Where the ROM set is: $JOUST_ROM, else joust.zip or a joust/ folder next to
   the program or in the current folder (development builds: also in the
   source tree's roms/). Returns 0 if found. */
int rom_locate(const char *argv0, char *out, size_t outlen);

#endif
