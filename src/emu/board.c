#include "emu/board.h"

#include <string.h>

static void update_irq(Board *b)
{
    b->cpu.irq = pia_irq_a(&b->pia[1]) || pia_irq_b(&b->pia[1]);
}

/* ---- the PIAs' connections -------------------------------------------- */

/* PIA 0 port A: bits 0-3 from the 74LS157 (player 2's controls with select 0,
   player 1's with select 1), bits 4-5 the start buttons. */
static uint8_t pia0_read_a(void *ctx)
{
    Board *b = ctx;
    uint8_t mux = (b->mux_select ? b->inp1 : b->inp2) & 0x0F;
    return (uint8_t)(mux | (b->in0 & 0x30));
}
static uint8_t pia0_read_b(void *ctx) { (void)ctx; return 0; }
static void pia0_cb2(void *ctx, int level) { ((Board *)ctx)->mux_select = level; }
static uint8_t pia1_read_a(void *ctx) { return ((Board *)ctx)->in2; }
/* The sound board's clock for a main-CPU cycle count. */
static int64_t sound_cycles(int64_t main_cycles) { return (int64_t)((double)main_cycles * SOUND_CPU_HZ / 1e6); }

/* A command reaches the sound board when the store's bus cycle ends (the
   instruction's last cycle): it is held until the main CPU's instruction is
   done, the sound CPU catches up to that moment, then the latch changes. */
static void pia1_write_b(void *ctx, uint8_t pins)
{
    Board *b = ctx;
    b->sound_pending = 1;
    b->sound_latch = pins | 0xC0; /* the top two bits are pulled up on the board */
}

static void send_sound_command(Board *b)
{
    b->sound_pending = 0;
    sound_run(&b->sound, sound_cycles(b->cpu.cycles));
    sound_command(&b->sound, b->sound_latch);
}

static void beam_catch_up(Board *b);

/* ---- memory map ------------------------------------------------------------ */

static uint8_t bus_read(void *ctx, uint16_t a)
{
    Board *b = ctx;
    if (a < 0x9000 && (b->romsel & 1)) return (uint8_t)rom_byte(b->rom, a);
    if (a < WILLIAMS_VRAM_SIZE) return b->ram[a];
    if (a >= 0xD000) return (uint8_t)rom_byte(b->rom, a);
    if (a >= 0xCC00) return b->cmos[a - 0xCC00];
    if (a >= 0xCB00) { /* video counter: the beam's line as the read lands, at the load's last cycle */
        int line = (int)((BOARD_FIRST_LINE + b->cpu.op_end / BOARD_CYCLES_PER_LINE) % BOARD_LINES);
        return (uint8_t)(line & 0xFC);
    }
    if ((a & 0xFF0C) == 0xC804) {
        uint8_t v = pia_read(&b->pia[0], a & 3);
        update_irq(b);
        return v;
    }
    if ((a & 0xFF0C) == 0xC80C) {
        uint8_t v = pia_read(&b->pia[1], a & 3);
        update_irq(b);
        return v;
    }
    return 0xFF; /* palette (write-only), unmapped */
}

/* The blitter's writes, for a write hook (tests follow RAM through it). */
static void blit_wrote(void *ctx, unsigned addr, uint8_t value)
{
    Board *b = ctx;
    b->write_hook(b, (uint16_t)addr, value);
}

static void blit_start(Board *b, uint8_t ctrl)
{
    WilliamsBus bus = { b->ram, b->rom, b->romsel & 1, b->write_hook ? blit_wrote : NULL, b };
    beam_catch_up(b);
    uint16_t src = (uint16_t)(b->blit[2] << 8 | b->blit[3]), dst = (uint16_t)(b->blit[4] << 8 | b->blit[5]);
    williams_blit(&bus, ctrl, b->blit[1], src, dst, b->blit[6], b->blit[7]);
    /* MAME's timing for the SC1 blitter: the CPU waits for it */
    int w = b->blit[6] ^ 4, h = b->blit[7] ^ 4;
    if (!w) w = 1;
    if (!h) h = 1;
    long accesses = 2L * w * h;
    long clocks4 = 4 + (ctrl & BLIT_SLOW ? 4 * (accesses + 2) : 2 * (accesses + 3));
    b->cpu.cycles += (clocks4 + 3) / 4;
}

static void bus_write(void *ctx, uint16_t a, uint8_t v)
{
    Board *b = ctx;
    if (b->write_hook) b->write_hook(b, a, v);
    if (a < WILLIAMS_VRAM_SIZE) {
        beam_catch_up(b);
        b->ram[a] = v; /* writes always reach RAM, ROM selected or not */
        return;
    }
    if (a >= 0xD000) return;
    if (a >= 0xCC00) {
        uint8_t nv = v | 0xF0; /* 4-bit CMOS */
        if (b->cmos[a - 0xCC00] != nv) b->cmos_dirty = 1;
        b->cmos[a - 0xCC00] = nv;
        return;
    }
    if (a >= 0xCB00) return; /* $CBFF watchdog */
    if (a >= 0xCA00) {
        b->blit[a & 7] = v;
        if (!(a & 7)) blit_start(b, v);
        return;
    }
    if (a >= 0xC900) {
        b->romsel = v;
        return;
    }
    if ((a & 0xFF0C) == 0xC804) {
        pia_write(&b->pia[0], a & 3, v);
        update_irq(b);
        return;
    }
    if ((a & 0xFF0C) == 0xC80C) {
        pia_write(&b->pia[1], a & 3, v);
        update_irq(b);
        return;
    }
    if (a < 0xC400) { /* $C000-$C00F, mirrored to $C3FF */
        beam_catch_up(b);
        b->palette[a & 0x0F] = v;
    }
}

/* ---- frame ------------------------------------------------------------------ */

void board_init(Board *b, const RomSet *rom, const uint8_t *cmos)
{
    memset(b, 0, sizeof *b);
    b->rom = rom;
    if (cmos) memcpy(b->cmos, cmos, sizeof b->cmos);
    for (int i = 0; i < 2; i++) {
        pia_reset(&b->pia[i]);
        b->pia[i].ctx = b;
    }
    b->pia[0].read_a = pia0_read_a;
    b->pia[0].read_b = pia0_read_b;
    b->pia[0].c2b_changed = pia0_cb2;
    b->pia[1].read_a = pia1_read_a;
    b->pia[1].write_b = pia1_write_b;
    /* one frame of emulated sound fills one tick of the port's audio */
    sound_init(&b->sound, rom, SOUND_CPU_HZ / BOARD_FRAME_HZ / (BOARD_AUDIO_RATE / BOARD_AUDIO_FPS));
    b->cpu.ctx = b;
    b->cpu.read = bus_read;
    b->cpu.write = bus_write;
    m6809_reset(&b->cpu);
    b->cpu.cycles = 4; /* the reset sequence: MAME's first instruction starts at cycle 4 */
}

static void draw_line(const Board *b, Framebuffer *fb, int line)
{
    int y = line - WILLIAMS_VISIBLE_Y;
    if (y < 0 || y >= FB_HEIGHT) return;
    uint32_t rgb[16];
    for (int i = 0; i < 16; i++) rgb[i] = 0xFF000000u | williams_rgb(b->palette[i]);
    uint32_t *row = fb_row(fb, y);
    for (int x = 0; x < FB_WIDTH; x++) {
        int vx = x + WILLIAMS_VISIBLE_X;
        uint8_t px = b->ram[(vx >> 1) * 256 + line];
        row[x] = rgb[vx & 1 ? px & 0x0F : px >> 4];
    }
}

/* Each line is drawn whole, from RAM and the palette as they are BEAM_LEAD
   cycles before the line starts (measured against MAME's frames in the
   Defender port: 2 or 3 match every captured frame, 0-1 and 4 or more do
   not). A store or blit lands on its instruction's last cycle: when that is
   too late for the next line, the line is drawn first, from what RAM held
   before. */
#define BEAM_LEAD 2

static void beam_catch_up(Board *b)
{
    if (b->fb && !b->next_drawn && b->cpu.op_end >= b->next_line_start - BEAM_LEAD) {
        draw_line(b, b->fb, b->next_line);
        b->next_drawn = 1;
    }
}

/* A frame runs from the start of vertical blank (line 247, where MAME's
   screen starts at power-on and where its frames end) to the last visible
   line, 246: the frame's picture is complete when it returns. */
void board_run_frame(Board *b, Framebuffer *fb)
{
    b->sound.out_len = 0;
    b->fb = fb && fb->scale == 1 ? fb : NULL;
    b->next_drawn = 1;
    for (int i = 0; i < BOARD_LINES; i++) {
        int line = (BOARD_FIRST_LINE + i) % BOARD_LINES;
        b->scanline = line;
        if (b->line_hook) b->line_hook(b, line);
        if (line < 256 && !(line & 0x1F)) pia_cb1(&b->pia[1], (line & 0x20) != 0); /* VA11 */
        if (line == 240) pia_ca1(&b->pia[1], 1);                                    /* COUNT240 */
        if (line == 0) pia_ca1(&b->pia[1], 0);
        update_irq(b);
        if (b->fb && !b->next_drawn) draw_line(b, b->fb, line);
        int64_t end = (b->frame * BOARD_LINES + i + 1) * (int64_t)BOARD_CYCLES_PER_LINE;
        /* the next line, which a store late in this one may have to draw early */
        b->next_line = (line + 1) % BOARD_LINES;
        b->next_line_start = end;
        b->next_drawn = i + 1 == BOARD_LINES; /* the next frame's first line belongs to it */
        while (b->cpu.cycles < end) {
            if (b->step_hook) b->step_hook(b);
            m6809_step(&b->cpu);
            if (b->sound_pending) send_sound_command(b);
        }
        sound_run(&b->sound, sound_cycles(end));
    }
    b->frame++;
    b->fb = NULL;
}
