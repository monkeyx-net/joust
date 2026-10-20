/*
 * Joust's random number generator: a 16-bit shift register at $A00D (hi)
 * and $A00E (lo), in direct page $A0.
 *   step ($E4C0, called through $E018): A = (lo >> 3) ^ lo, shifted right
 *     once; the bit shifted out goes into bit 7 of hi, and hi's bit 0 into
 *     bit 7 of lo (ROR $0D; ROR $0E). Returns A; the carry left over (lo's
 *     old bit 0) is used by callers too.
 *   stir (the IRQ handler at $E395, 4 times a frame): INC $0D, DEC $0E.
 * Checked step by step against MAME by mame_prng (src/tests/test_prng.c).
 */
#ifndef JOUST_PRNG_H
#define JOUST_PRNG_H

#include <stdint.h>

typedef struct {
    uint8_t hi, lo; /* $A00D, $A00E */
} Prng;

/* One call of $E4C0: returns A, *carry (if not NULL) gets the carry flag. */
uint8_t prng_step(Prng *p, int *carry);
/* What each interrupt does to the state. */
void prng_stir(Prng *p);

#endif
