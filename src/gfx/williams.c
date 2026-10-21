#include "gfx/williams.h"

#include <math.h>

#include "gfx/blit.h"

/* MAME compute_resistor_weights(0, 255, -1.0, ...) with no pull-ups or
   pull-downs: each bit's weight is its conductance, scaled so that every
   channel with all its bits set is 255. */
static const double R_RG[3] = { 1200, 560, 330 }, R_B[2] = { 560, 330 };

static long level(int bits, const double *ohms, int n)
{
    double total = 0, v = 0;
    for (int i = 0; i < n; i++) {
        total += 1 / ohms[i];
        if (bits >> i & 1) v += 1 / ohms[i];
    }
    return lround(255 * v / total);
}

uint32_t williams_rgb(uint8_t entry)
{
    return (uint32_t)level(entry & 7, R_RG, 3) << 16 | (uint32_t)level(entry >> 3 & 7, R_RG, 3) << 8 |
           (uint32_t)level(entry >> 6 & 3, R_B, 2);
}

void williams_draw_vram(Framebuffer *fb, const uint8_t *vram, const uint8_t palette[16])
{
    uint32_t rgb[16];
    for (int i = 0; i < 16; i++) rgb[i] = williams_rgb(palette[i]);
    for (int y = 0; y < FB_HEIGHT; y++)
        for (int x = 0; x < FB_WIDTH; x++) {
            int vx = x + WILLIAMS_VISIBLE_X, vy = y + WILLIAMS_VISIBLE_Y;
            uint8_t b = vram[(vx >> 1) * 256 + vy];
            fill_rectf(fb, x, y, 1, 1, rgb[vx & 1 ? b & 0x0F : b >> 4]);
        }
}

static uint8_t bus_read(const WilliamsBus *bus, unsigned addr)
{
    addr &= 0xFFFF;
    if (addr < 0x9000 && bus->romsel) return (uint8_t)rom_byte(bus->rom, addr);
    if (addr < WILLIAMS_VRAM_SIZE) return bus->ram[addr];
    if (addr >= 0xD000) return (uint8_t)rom_byte(bus->rom, addr);
    return 0; /* I/O and the rest: not image sources */
}

/* MAME blit_pixel: keep the destination nibbles the flags protect, or that a
   transparent source pixel leaves alone, and write the rest. */
static void blit_pixel(WilliamsBus *bus, unsigned dst, uint8_t src, uint8_t ctrl, uint8_t solid)
{
    if (dst >= WILLIAMS_VRAM_SIZE) return; /* only RAM below $C000 here */
    uint8_t cur = bus->ram[dst], keep = 0xFF;
    if ((ctrl & BLIT_FOREGROUND) && !(src & 0xF0)) {
        if (ctrl & BLIT_NO_EVEN) keep &= 0x0F;
    } else if (!(ctrl & BLIT_NO_EVEN)) {
        keep &= 0x0F;
    }
    if ((ctrl & BLIT_FOREGROUND) && !(src & 0x0F)) {
        if (ctrl & BLIT_NO_ODD) keep &= 0xF0;
    } else if (!(ctrl & BLIT_NO_ODD)) {
        keep &= 0xF0;
    }
    cur &= keep;
    cur |= (ctrl & BLIT_SOLID ? solid : src) & (uint8_t)~keep;
    if (bus->on_write) bus->on_write(bus->ctx, dst, cur);
    bus->ram[dst] = cur;
}

void williams_blit(WilliamsBus *bus, uint8_t ctrl, uint8_t solid, uint16_t src, uint16_t dst, uint8_t w_reg,
                   uint8_t h_reg)
{
    int w = w_reg ^ 4, h = h_reg ^ 4;
    if (w == 0) w = 1;
    if (h == 0) h = 1;
    const unsigned sxadv = ctrl & BLIT_SRC_STRIDE_256 ? 0x100 : 1, syadv = ctrl & BLIT_SRC_STRIDE_256 ? 1 : (unsigned)w;
    const unsigned dxadv = ctrl & BLIT_DST_STRIDE_256 ? 0x100 : 1, dyadv = ctrl & BLIT_DST_STRIDE_256 ? 1 : (unsigned)w;
    unsigned sstart = src, dstart = dst;
    unsigned pixdata = 0; /* shift mode: carried across rows, as in MAME */
    for (int y = 0; y < h; y++) {
        unsigned s = sstart & 0xFFFF, d = dstart & 0xFFFF;
        for (int x = 0; x < w; x++) {
            uint8_t b = bus_read(bus, s);
            if (ctrl & BLIT_SHIFT) {
                pixdata = pixdata << 8 | b;
                b = (uint8_t)(pixdata >> 4);
            }
            blit_pixel(bus, d, b, ctrl, solid);
            s = (s + sxadv) & 0xFFFF;
            d = (d + dxadv) & 0xFFFF;
        }
        /* a screen-layout pointer steps rows within its column (no carry into x) */
        if (ctrl & BLIT_DST_STRIDE_256) dstart = (dstart & 0xFF00) | ((dstart + dyadv) & 0xFF);
        else dstart += dyadv;
        if (ctrl & BLIT_SRC_STRIDE_256) sstart = (sstart & 0xFF00) | ((sstart + syadv) & 0xFF);
        else sstart += syadv;
    }
}
