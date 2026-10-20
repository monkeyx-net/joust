#include "sim/prng.h"

uint8_t prng_step(Prng *p, int *carry)
{
    uint8_t a = (uint8_t)((p->lo >> 3) ^ p->lo); /* LDA <$0E; LSRA x3; EORA <$0E */
    int c = a & 1;                                /* LSRA */
    a >>= 1;
    int hi_out = p->hi & 1;                       /* ROR <$0D */
    p->hi = (uint8_t)(c << 7 | p->hi >> 1);
    int lo_out = p->lo & 1;                       /* ROR <$0E */
    p->lo = (uint8_t)(hi_out << 7 | p->lo >> 1);
    if (carry) *carry = lo_out;
    return a;
}

void prng_stir(Prng *p)
{
    p->hi++; /* INC <$0D */
    p->lo--; /* DEC <$0E */
}
