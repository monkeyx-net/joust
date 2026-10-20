/*
 * Joust native build: SDL setup and the fixed 60 Hz loop. See JOUST_PORT_PLAN.md.
 * The game is the ROM's own code on the emulated board (emu/board.c), one
 * board frame per tick; the port supplies video (with the enhanced presets),
 * input and saving the arcade's CMOS. --test-card shows the test card instead
 * (ROM status, a two-player input tester, the graphics presets, and pages of
 * the images decoded from the ROM: Select+R1 / F2 steps the pages).
 *
 *   joust [--windowed] [--gfx preset] [--rom path] [--test-card] [--page N]
 *         [--input script.txt] [--frames N] [--screenshot out.png]
 *   joust --check-rom <dir|zip>
 *   joust --dump-rom <dir|zip> <first> <last>
 *   joust --list-images
 *
 * --frames N stops after N updates; with --screenshot the last frame is
 * written as a PNG (use SDL_VIDEODRIVER=dummy to run headless).
 * --input replays scripted controls: lines of "<frame> [coin] [start] [start2]
 * [left] [right] [flap] [left2] [right2] [flap2] [menu]", each holding from
 * that frame until the next line (live controls still quit).
 * --dump-rom prints "ADDR BYTE" lines (hex) for main-CPU addresses first..last,
 * the format the MAME probe writes (src/tests/mame/rom_map_probe.lua).
 * --list-images prints the image catalogue as "ADDR:W:H name" (hex address,
 * width in bytes, height), the input of src/tests/mame/image_probe.lua.
 */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assets/images.h"
#include "assets/png.h"
#include "assets/rom.h"
#include "assets/savefile.h"
#include "emu/board.h"
#include "gfx/blit.h"
#include "gfx/debugfont.h"
#include "gfx/enhance.h"
#include "gfx/fb.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "platform/video.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define FRAME_HZ 60
/* Cap catch-up after a stall (suspend/resume, SD card hiccup). */
#define MAX_STEPS_PER_PRESENT 4
#define GFX_NOTICE_FRAMES 120
#define CMOS_FILE "joust.cmos"
#define CMOS_SAVE_FRAMES 300  /* write changed CMOS (scores, settings) at most every 5 s */
#define PRIME_FRAMES 1800     /* blank CMOS: let the game restore its factory settings */

#define WHITE 0xFFFFFFu
#define GREY 0x606060u
#define YELLOW 0xFFFF44u
#define CYAN 0x44FFFFu
#define GREEN 0x44FF44u
#define RED 0xFF4444u
#define ORANGE 0xFF8800u

static Framebuffer fb;     /* arcade resolution */
static Framebuffer fb_big; /* enhanced modes: the frame at the render scale */
static Framebuffer *shown; /* the frame last drawn */

typedef struct {
    unsigned long clock;
    InputFrame in;
    GfxSettings gfx;
    int gfx_notice;
    int coins;
    char rom_path[1024];
    char rom_error[256];
    RomSet rom;
    ImageSheet sheet;
    int have_sheet;
    int page;            /* 0 the card, then image pages */
    int image_pages;
    int first_image[MAX_ROM_IMAGES + 1]; /* first catalogue entry on each image page */
    int test_card;       /* --test-card, or no ROM: the card instead of the game */
    Board board;
    int cmos_wait;       /* frames until changed CMOS is written */
} Game;

static Game g;

/* ---- test card ------------------------------------------------------------ */

static void centred(Framebuffer *t, int y, const char *text, uint32_t rgb)
{
    debug_text(t, (FB_WIDTH - debug_text_width(text)) / 2.0, y, text, rgb);
}

static void lamp(Framebuffer *t, int x, int y, const char *label, int on)
{
    fill_rectf(t, x, y, 5, 5, on ? GREEN : GREY);
    debug_text(t, x + 8, y, label, on ? WHITE : GREY);
}

static void draw_scene(Framebuffer *t)
{
    char line[160];
    fb_clear(t, 0xFF000000u);
    fill_rectf(t, 6, 15, 280, 2, ORANGE);
    fill_rectf(t, 6, 229, 280, 2, ORANGE);
    fill_rectf(t, 6, 15, 2, 216, ORANGE);
    fill_rectf(t, 284, 15, 2, 216, ORANGE);
    centred(t, 4, "JOUST  -  NATIVE PORT  -  TEST CARD", YELLOW);

    if (g.rom.loaded) {
        snprintf(line, sizeof line, "ROM: %.60s", g.rom_path);
        centred(t, 26, line, GREEN);
        snprintf(line, sizeof line, "MAME JOUST SET OK   RESET VECTOR $%02X%02X", rom_byte(&g.rom, 0xFFFE),
                 rom_byte(&g.rom, 0xFFFF));
        centred(t, 34, line, GREEN);
    } else {
        centred(t, 26, g.rom_error[0] ? g.rom_error : "NO ROM SET FOUND", RED);
        centred(t, 34, "PUT JOUST.ZIP NEXT TO THE PROGRAM, OR USE --ROM", RED);
    }

    for (int p = 0; p < PLAYERS; p++) {
        const PlayerInput *pi = &g.in.p[p];
        int x = p ? 162 : 30, y = 56;
        snprintf(line, sizeof line, "PLAYER %d%s", p + 1, p < g.in.pads ? "  (PAD)" : "");
        debug_text(t, x, y, line, CYAN);
        lamp(t, x, y + 12, "LEFT", pi->move_x < 0);
        lamp(t, x + 44, y + 12, "RIGHT", pi->move_x > 0);
        lamp(t, x, y + 22, "FLAP", pi->flap);
    }
    lamp(t, 30, 100, "COIN", g.in.coin);
    lamp(t, 74, 100, "START", g.in.start);
    lamp(t, 122, 100, "2P START", g.in.start2);
    lamp(t, 182, 100, "MENU", g.in.menu_held);
    if (g.have_sheet) {
        snprintf(line, sizeof line, "%d IMAGES DECODED FROM THE ROM: SELECT+R1 / F2 TO VIEW", ROM_IMAGE_COUNT);
        debug_text(t, 30, 118, line, GREEN);
    }
    snprintf(line, sizeof line, "COINS %d", g.coins);
    debug_text(t, 30, 110, line, WHITE);

    /* something moving, to judge pacing and the enhanced modes */
    double phase = (double)(g.clock % 240) / 240.0;
    double bx = 30 + 220 * (phase < 0.5 ? phase * 2 : 2 - phase * 2);
    fill_rectf(t, 30, 140, 232, 1, GREY);
    fill_rectf(t, bx, 128, 12, 12, YELLOW);
    fill_rectf(t, bx + 3, 131, 6, 6, RED);
    for (int i = 0; i < 8; i++) {
        static const uint32_t bars[8] = { 0xFFFFFF, 0xFFFF00, 0x00FFFF, 0x00FF00, 0xFF00FF, 0xFF0000, 0x0000FF, 0x000000 };
        fill_rectf(t, 30 + i * 29, 150, 29, 20, bars[i]);
    }

    snprintf(line, sizeof line, "GRAPHICS: %s   (SELECT+L1 / F3, F4 GLOW, F5 SCANLINES)", gfx_describe(&g.gfx));
    centred(t, 184, line, g.gfx_notice > 0 ? YELLOW : WHITE);
    snprintf(line, sizeof line, "FRAME %lu   RENDER %dX", g.clock, t->scale);
    centred(t, 194, line, GREY);
    centred(t, 210, "EXIT: HOLD SELECT+START OR ESC", GREY);
}

/* ---- image pages ---------------------------------------------------------- */

#define PAGE_X0 10
#define PAGE_X1 282
#define PAGE_Y0 26
#define PAGE_Y1 226
#define PAGE_GAP 4

/* Lay the catalogue out in shelves; call fn for each image on `page` (or count pages). */
static int layout_images(int page, Framebuffer *t)
{
    int pg = 0, x = PAGE_X0, y = PAGE_Y0, shelf = 0;
    for (int i = 0; i < ROM_IMAGE_COUNT; i++) {
        const ImageFrame *f = &g.sheet.frames[i];
        if (x + f->w > PAGE_X1) {
            x = PAGE_X0;
            y += shelf + PAGE_GAP;
            shelf = 0;
        }
        if (y + f->h > PAGE_Y1) {
            pg++;
            x = PAGE_X0;
            y = PAGE_Y0;
            shelf = 0;
        }
        if (t && pg == page)
            blit(t, &g.sheet.image, f->x, f->y, f->w, f->h, x, y, 1, 0xFFFFFF);
        x += f->w + PAGE_GAP;
        if (f->h > shelf) shelf = f->h;
    }
    return pg + 1;
}

static void draw_images(Framebuffer *t)
{
    char line[96];
    fb_clear(t, 0xFF000000u);
    snprintf(line, sizeof line, "ROM IMAGES  %d  -  PAGE %d OF %d   (SELECT+R1 / F2: NEXT)", ROM_IMAGE_COUNT, g.page,
             g.image_pages);
    centred(t, 8, line, YELLOW);
    layout_images(g.page - 1, t);
}

/* ---- the game --------------------------------------------------------------- */

/* With blank CMOS the arcade restores its factory settings and then waits for
   a reset; run that headless once, as the MAME tests prime their CMOS. */
static void board_start(void)
{
    static uint8_t cmos[BOARD_CMOS_SIZE];
    if (!save_load(CMOS_FILE, cmos, sizeof cmos)) {
        board_init(&g.board, &g.rom, cmos);
        return;
    }
    board_init(&g.board, &g.rom, NULL);
    for (int i = 0; i < PRIME_FRAMES; i++) board_run_frame(&g.board, NULL);
    memcpy(cmos, g.board.cmos, sizeof cmos);
    board_init(&g.board, &g.rom, cmos);
    g.board.cmos_dirty = 1;
}

static void save_cmos(void)
{
    if (!g.board.cmos_dirty) return;
    if (save_store(CMOS_FILE, g.board.cmos, sizeof g.board.cmos))
        fprintf(stderr, "joust: cannot save %s\n", save_path(CMOS_FILE));
    g.board.cmos_dirty = 0;
}

/* The port's controls as the cabinet's switches (emu/board.h). */
static void board_inputs(const InputFrame *in)
{
    Board *b = &g.board;
    uint8_t *inp[PLAYERS] = { &b->inp1, &b->inp2 };
    for (int p = 0; p < PLAYERS; p++)
        *inp[p] = (uint8_t)((in->p[p].move_x < 0 ? INP_LEFT : 0) | (in->p[p].move_x > 0 ? INP_RIGHT : 0) |
                            (in->p[p].flap ? INP_FLAP : 0));
    b->in0 = (uint8_t)((in->start ? IN0_START1 : 0) | (in->start2 ? IN0_START2 : 0));
    b->in2 = (uint8_t)((in->coin ? IN2_COIN1 : 0) | (in->menu_held ? IN2_ADVANCE : 0));
}

/* Classic: the arcade frame. HD: drawn at the render scale. FILTER: the arcade
   frame upscaled whole. Glow goes over whichever is shown. The game's frame is
   only pixels, so its HD presets upscale it like FILTER. */
static void draw(int display_scale)
{
    GfxSettings gfx = g.gfx;
    if (!g.test_card && gfx.mode == GFX_HD) gfx.mode = GFX_FILTER;
    const GfxSettings *gs = &gfx;
    const Scaler scaler = gs->smooth ? SCALER_SMOOTH : SCALER_CRISP;
    const int s = gfx_render_scale(gs, display_scale);
    shown = &fb;
    if (!g.test_card) { /* the board drew the frame into fb */
        if (g.gfx_notice > 0) {
            char text[80];
            snprintf(text, sizeof text, "GRAPHICS: %s", gfx_describe(&g.gfx));
            fill_rectf(&fb, 0, FB_HEIGHT - 9, FB_WIDTH, 9, 0x000000);
            centred(&fb, FB_HEIGHT - 7, text, YELLOW);
        }
        if (gs->mode == GFX_FILTER && !fb_init(&fb_big, s) && !enhance_filter_frame(&fb, &fb_big, scaler))
            shown = &fb_big;
        if (gs->glow) enhance_glow(shown);
        return;
    }
    void (*scene)(Framebuffer *) = g.page ? draw_images : draw_scene;
    if (gs->mode == GFX_HD && !fb_init(&fb_big, s) && !enhance_prepare(s, scaler)) {
        scene(&fb_big);
        shown = &fb_big;
    } else {
        scene(&fb);
        if (gs->mode == GFX_FILTER && !fb_init(&fb_big, s) && !enhance_filter_frame(&fb, &fb_big, scaler))
            shown = &fb_big;
    }
    if (gs->glow) enhance_glow(shown);
}

static void step(const InputFrame *in)
{
    static bool coin_was;
    g.clock++;
    g.in = *in;
    if (!g.test_card) {
        board_inputs(in);
        board_run_frame(&g.board, &fb);
        audio_stream(g.board.sound.out, g.board.sound.out_len, BOARD_AUDIO_RATE);
        if (g.board.cmos_dirty && --g.cmos_wait <= 0) {
            save_cmos();
            g.cmos_wait = CMOS_SAVE_FRAMES;
        }
    }
    if (in->coin && !coin_was) g.coins++;
    coin_was = in->coin;
    if (g.test_card && in->operator_menu) g.page = g.have_sheet ? (g.page + 1) % (g.image_pages + 1) : 0;
    if (in->gfx_next || in->gfx_glow || in->gfx_scanlines) {
        if (in->gfx_next) gfx_next_preset(&g.gfx);
        if (in->gfx_glow) g.gfx.glow = !g.gfx.glow;
        if (in->gfx_scanlines) g.gfx.scanlines = !g.gfx.scanlines;
        g.gfx_notice = GFX_NOTICE_FRAMES;
    }
    if (g.gfx_notice > 0) g.gfx_notice--;
}

/* ---- tools ---------------------------------------------------------------- */

static int check_rom(const char *path)
{
    static RomSet rom;
    char err[512];
    if (rom_load(&rom, path, err, sizeof err)) {
        fprintf(stderr, "joust: %s\n", err);
        return 1;
    }
    printf("joust: %s: MAME joust set OK\n", path);
    return 0;
}

static int dump_rom(const char *path, const char *first, const char *last)
{
    static RomSet rom;
    char err[512];
    if (rom_load(&rom, path, err, sizeof err)) {
        fprintf(stderr, "joust: %s\n", err);
        return 1;
    }
    unsigned a = (unsigned)strtoul(first, NULL, 16), b = (unsigned)strtoul(last, NULL, 16);
    for (unsigned addr = a; addr <= b && addr <= 0xFFFF; addr++) {
        int v = rom_byte(&rom, addr);
        if (v >= 0) printf("%04X %02X\n", addr, v);
    }
    return 0;
}

static int save_screenshot(const char *path)
{
    static uint8_t rgba[FB_HEIGHT * FB_MAX_SCALE * FB_WIDTH * FB_MAX_SCALE * 4];
    const Framebuffer *f = shown ? shown : &fb;
    for (int y = 0; y < f->h; y++)
        for (int x = 0; x < f->w; x++) {
            uint32_t p = fb_row(f, y)[x];
            uint8_t *o = rgba + ((long)y * f->w + x) * 4;
            o[0] = (uint8_t)(p >> 16);
            o[1] = (uint8_t)(p >> 8);
            o[2] = (uint8_t)p;
            o[3] = 0xFF;
        }
    if (png_write(path, rgba, f->w, f->h)) {
        fprintf(stderr, "joust: cannot write %s\n", path);
        return -1;
    }
    return 0;
}

/* ---- scripted input ------------------------------------------------------- */

typedef struct {
    long frame;
    InputFrame in;
} ScriptLine;

static ScriptLine script[4096];
static int script_len, script_pos;

static int load_script(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "joust: cannot open %s\n", path);
        return -1;
    }
    char line[256];
    while (script_len < (int)(sizeof script / sizeof script[0]) && fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        ScriptLine *l = &script[script_len];
        memset(l, 0, sizeof *l);
        char *tok = strtok(line, " \t\n");
        if (!tok) continue;
        l->frame = strtol(tok, NULL, 10);
        while ((tok = strtok(NULL, " \t\n"))) {
            if (!strcmp(tok, "coin")) l->in.coin = true;
            else if (!strcmp(tok, "start")) l->in.start = true;
            else if (!strcmp(tok, "start2")) l->in.start2 = true;
            else if (!strcmp(tok, "menu")) l->in.menu_held = true;
            else if (!strcmp(tok, "left")) l->in.p[0].move_x = -1;
            else if (!strcmp(tok, "right")) l->in.p[0].move_x = 1;
            else if (!strcmp(tok, "flap")) l->in.p[0].flap = true;
            else if (!strcmp(tok, "left2")) l->in.p[1].move_x = -1;
            else if (!strcmp(tok, "right2")) l->in.p[1].move_x = 1;
            else if (!strcmp(tok, "flap2")) l->in.p[1].flap = true;
            else fprintf(stderr, "joust: %s: unknown token '%s'\n", path, tok);
        }
        script_len++;
    }
    fclose(f);
    return 0;
}

/* Replace live controls with the script line in force at `frame` (quit stays live). */
static void apply_script(InputFrame *in, long frame)
{
    while (script_pos + 1 < script_len && script[script_pos + 1].frame <= frame) script_pos++;
    bool quit = in->quit;
    if (script[script_pos].frame <= frame) *in = script[script_pos].in;
    else memset(in, 0, sizeof *in);
    in->quit = quit;
}

/* ---- loop ----------------------------------------------------------------- */

/* Loop state, static so the browser can run one pass per animation frame. */
static long max_frames = -1;
static const char *screenshot;
static Uint64 step_ticks, last, acc;
static long frames;
static int rc;

/* Step for the time elapsed, then draw and present; false = stop. */
static bool run_once(void)
{
    Uint64 now = SDL_GetPerformanceCounter();
    acc += now - last;
    last = now;
    if (acc > step_ticks * MAX_STEPS_PER_PRESENT)
        acc = step_ticks * MAX_STEPS_PER_PRESENT;
    /* Headless runs step once per loop so the frame count is exact. */
    if (max_frames >= 0 && acc < step_ticks) acc = step_ticks;

    while (acc >= step_ticks) {
        InputFrame in;
        input_poll(&in);
#ifndef __EMSCRIPTEN__
        if (in.quit) return false; /* a browser tab has nothing to quit to */
#endif
        if (script_len) apply_script(&in, frames + 1);
        step(&in);
        frames++;
        acc -= step_ticks;
        if (max_frames >= 0 && frames >= max_frames) break;
    }
    if (max_frames < 0 || frames >= max_frames) draw(video_display_scale());
    if (max_frames < 0) video_present(shown, g.gfx.scanlines); /* blocks on vsync */
    if (max_frames >= 0 && frames >= max_frames) {
        if (screenshot && save_screenshot(screenshot)) rc = 1;
        return false;
    }
    return true;
}

#ifdef __EMSCRIPTEN__
static void web_frame(void) { run_once(); }
#endif

int main(int argc, char **argv)
{
    bool windowed = false;
    const char *rom_arg = NULL;
    if (argc == 3 && !strcmp(argv[1], "--check-rom")) return check_rom(argv[2]);
    if (argc == 5 && !strcmp(argv[1], "--dump-rom")) return dump_rom(argv[2], argv[3], argv[4]);
    if (argc == 2 && !strcmp(argv[1], "--list-images")) {
        for (int i = 0; i < ROM_IMAGE_COUNT; i++)
            printf("%04X:%d:%d %s\n", ROM_IMAGES[i].addr, ROM_IMAGES[i].w, ROM_IMAGES[i].h, ROM_IMAGES[i].name);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--windowed")) windowed = true;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) max_frames = strtol(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) screenshot = argv[++i];
        else if (!strcmp(argv[i], "--rom") && i + 1 < argc) rom_arg = argv[++i];
        else if (!strcmp(argv[i], "--page") && i + 1 < argc) g.page = (int)strtol(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--test-card")) g.test_card = 1;
        else if (!strcmp(argv[i], "--input") && i + 1 < argc) {
            if (load_script(argv[++i])) return 2;
        }
        else if (!strcmp(argv[i], "--gfx") && i + 1 < argc) {
            if (gfx_parse(argv[++i], &g.gfx)) {
                fprintf(stderr, "joust: unknown --gfx preset '%s' (one of: %s)\n", argv[i], gfx_preset_names());
                return 2;
            }
        } else {
            fprintf(stderr,
                    "usage: %s [--windowed] [--gfx preset] [--rom path] [--test-card] [--page N]\n"
                    "          [--input script.txt] [--frames N] [--screenshot out.png]\n"
                    "       %s --check-rom <dir|zip>\n"
                    "       %s --dump-rom <dir|zip> <first> <last>\n"
                    "       %s --list-images\n",
                    argv[0], argv[0], argv[0], argv[0]);
            return 2;
        }
    }

    if (fb_init(&fb, 1)) {
        fprintf(stderr, "joust: out of memory\n");
        return 1;
    }
    if (rom_arg) snprintf(g.rom_path, sizeof g.rom_path, "%s", rom_arg);
    else rom_locate(argv[0], g.rom_path, sizeof g.rom_path);
    if (g.rom_path[0] && rom_load(&g.rom, g.rom_path, g.rom_error, sizeof g.rom_error))
        fprintf(stderr, "joust: %s\n", g.rom_error);
    if (g.rom.loaded && !rom_images_decode(&g.rom, &g.sheet)) { /* the image pages */
        static EnhanceRect rects[MAX_ROM_IMAGES];
        for (int i = 0; i < ROM_IMAGE_COUNT; i++)
            rects[i] = (EnhanceRect){ g.sheet.frames[i].x, g.sheet.frames[i].y, g.sheet.frames[i].w, g.sheet.frames[i].h };
        enhance_add_image(&g.sheet.image, rects, ROM_IMAGE_COUNT, 0);
        g.have_sheet = 1;
        g.image_pages = layout_images(-1, NULL);
    }
    if (g.page < 0 || g.page > g.image_pages) g.page = 0;
    if (g.page) g.test_card = 1;
    if (!g.rom.loaded) g.test_card = 1; /* nothing to run: show why */
    if (!g.test_card) board_start();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }
#ifdef __EMSCRIPTEN__
    windowed = true; /* the page's canvas */
#endif
    if (!video_init(windowed)) {
        SDL_Quit();
        return 1;
    }
    input_init();
    if (max_frames < 0) audio_init(argv[0]); /* headless runs stay silent */

    step_ticks = SDL_GetPerformanceFrequency() / FRAME_HZ;
    last = SDL_GetPerformanceCounter();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(web_frame, 0, 1); /* never returns */
#endif
    while (run_once()) {}

    if (!g.test_card) save_cmos();
    audio_shutdown();
    input_shutdown();
    video_shutdown();
    SDL_Quit();
    enhance_clear_images();
    rom_images_free(&g.sheet);
    fb_free(&fb_big);
    fb_free(&fb);
    return rc;
}
