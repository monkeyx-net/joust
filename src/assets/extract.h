/*
 * The extractor (`joust --extract`, plan decision 1, phase X1): reads the
 * player's ROM set once and writes what the C game needs under <out>/;
 * after that `--c-game` runs from <out>/ alone, never the ROM set. Nothing
 * here is compiled in: the data comes from the player's set.
 *
 *   data/program.bin  the main CPU's program as the C game reads it: 64 KB
 *                     by address, $0000-$8FFF and $D000-$FFFF from the
 *                     ROMs, zeros between (RAM and I/O)
 *   data/cmos.bin     the factory settings: the CMOS as the arcade leaves
 *                     it after restoring them from blank (1 KB of nibbles)
 *   sounds/NN.wav     each sound command NN (hex, $00-$3E, the value the
 *                     game writes), recorded from the emulated sound board,
 *                     48 kHz mono 16-bit
 *   sounds/sounds.txt per command: once, loop (still playing after
 *                     SOUND_MAX_SECONDS; the WAV holds its first seconds) or
 *                     none (no output)
 *   manifest.txt      written last: the folder is complete
 */
#ifndef JOUST_EXTRACT_H
#define JOUST_EXTRACT_H

#include "assets/rom.h"

#define EXTRACT_MANIFEST "manifest.txt"
#define EXTRACT_VERSION 1
#define EXTRACT_PROGRAM "data/program.bin"
#define EXTRACT_CMOS "data/cmos.bin"
#define EXTRACT_SOUND_COMMANDS 0x3F /* $3F (with the top bits, $FF) is "no command" */

/* Write everything to out_dir (created if needed). Returns 0, or -1 with a
   message in err; without manifest.txt the folder counts as missing. */
int extract_all(const RomSet *rom, const char *out_dir, char *err, int errlen);

#endif
