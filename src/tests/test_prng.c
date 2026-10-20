/*
 * The random number generator against MAME (label mame):
 *   test_prng <prng_probe output>
 * Replays every stir and step the probe logged from power-on through
 * sim/prng.c and checks the state after each.
 */
#include <stdio.h>
#include <stdlib.h>

#include "sim/prng.h"

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: test_prng <prng_probe output>\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    char kind;
    unsigned v;
    Prng p = { 0, 0 };
    int have = 0, stirs = 0, steps = 0, bad = 0, line = 0;
    while (fscanf(f, " %c %4x", &kind, &v) == 2) {
        line++;
        Prng want = { (uint8_t)(v >> 8), (uint8_t)v };
        if (kind == 'I' || kind == 'W') { /* start, or the boot RAM test and clear */
            p = want;
            have = 1;
            continue;
        }
        if (!have || (kind != 'S' && kind != 'R')) {
            fprintf(stderr, "line %d: unexpected '%c'\n", line, kind);
            bad++;
            break;
        }
        if (kind == 'S') {
            prng_stir(&p);
            stirs++;
        } else {
            prng_step(&p, NULL);
            steps++;
        }
        if (p.hi != want.hi || p.lo != want.lo) {
            fprintf(stderr, "line %d (%c): %02X%02X, MAME %04X\n", line, kind, p.hi, p.lo, v);
            if (++bad > 5) break;
            p = want; /* resync to report further differences */
        }
    }
    fclose(f);
    printf("prng: %d stirs, %d steps replayed, %d mismatches\n", stirs, steps, bad);
    return bad || !steps ? 1 : 0;
}
