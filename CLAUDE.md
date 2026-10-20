# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A native C11 / SDL2 port of Joust (Williams, 1982) for Linux handhelds
(aarch64: ArkOS, ROCKNIX, muOS, Knulli), x86_64 (Steam Deck, desktops),
Windows and the web. **The plan, decisions and test plan live in
[JOUST_PORT_PLAN.md](JOUST_PORT_PLAN.md)**; read it before larger changes and
keep its phase table up to date.

The platform layer, asset loaders, framebuffer, blitter and enhanced graphics
were copied from the sibling Robotron port (`../robotron`, branch `sdl2`);
improvements to that shared code may be worth carrying across, but ask first.

## Ground Rules

- **MAME is the behaviour reference.** There is no remake to port from: every
  behaviour comes from the ROM, read in MAME (RAM probes, per-frame traces)
  and documented in the plan. Cite ROM addresses in short comments.
- **No ROM-derived data in the repo or the binary** (plan, decision 1). Not
  sprites, fonts, tables or sounds: the program reads the player's ROM set at
  run time (or extracts it to `assets/` on first run).
- **All new code and tooling is C** (built with CMake). The one exception is
  MAME probe scripts, which must be Lua (MAME's scripting language); they only
  dump RAM to text files, and C or CMake does the comparing.
- New files go in `src/`, `CMakeLists.txt`, `.github/` or `JOUST_PORT_PLAN.md`.
  Ask before adding files elsewhere.
- The user's ROM set lives in `roms/` (git-ignored, here a symlink); never
  commit ROMs or zips, and never send them anywhere.

## Commands

```bash
cmake -S . -B build && cmake --build build -j     # build
ctest --test-dir build                            # all tests
ctest --test-dir build -L rom                     # tests needing the ROM set (roms/joust.zip)
ctest --test-dir build -L mame                    # tests needing MAME + the ROM set
build/joust --windowed                            # run (finds roms/joust.zip in dev builds)
build/joust --windowed --gfx hd-glow              # enhanced graphics (presets: classic ... hd-glow-crt)
SDL_VIDEODRIVER=dummy build/joust --frames 60 --screenshot out.png   # headless screenshot
build/joust --check-rom roms/joust.zip            # verify a ROM set
build/joust --dump-rom roms/joust.zip D000 D0FF   # main-CPU ROM bytes, "ADDR BYTE" hex
cmake --build build --target package_linux package_portmaster   # release archives
cmake -S . -B build-san -DJOUST_SANITIZE=ON       # ASan/UBSan build
emcmake cmake -S . -B build-web && cmake --build build-web --target serve   # web build on http://localhost:8000/
```

Release builds: `-DJOUST_DEV_ASSETS=OFF` (no source-tree fallbacks).

## Layout (`src/`)

- `main.c` - SDL setup, fixed 60 Hz loop, the test card, graphics pipeline,
  `--frames`, `--screenshot`, `--rom`, `--check-rom`, `--dump-rom`
- `platform/` - the only code that includes SDL: `video.c` (292x240 framebuffer
  at any scale, integer scaling, scanline overlay), `input.c` (two players:
  move + flap; pads and keyboard), `audio.c` (priority mixer; the game
  registers its sounds)
- `gfx/` - draws into the framebuffer in arcade coordinates (`fb.scale` > 1 for
  HD): `fb.c`, `blit.c`, `scale.c` (Scale2x/3x, xBR), `enhance.c` (graphics
  presets, HD image registry, whole-frame filter, glow), `debugfont.c` (3x5
  font for diagnostics, not the ROM's)
- `assets/` - `rom.c` (the joust set from a zip or folder, matched by CRC;
  CPU address map), `zip.c`, `inflate.c`, `png.c`, `assets.c` (asset folder lookup)
- `sim/` - (to come) pure C game simulation, no SDL, no drawing
- `tests/` - C tests, MAME probes (`tests/mame/`)
- `packaging/` - release layout script, PortMaster launcher and metadata
- `web/` - page shell for the Emscripten build

## Conventions

- C11, `-Wall -Wextra`, no warnings. Builds must also pass in Ubuntu 20.04
  (GCC 9, SDL 2.0.10, the PortMaster base): use only the SDL 2.0.10 API.
- The sim (when it exists) is deterministic: step once per frame, no wall
  clock, randomness from the ROM's own generator in the ROM's call order.
- `sim/` must not include SDL or touch the framebuffer; `gfx/` only reads sim state.
- Enhanced graphics are cosmetic: classic output stays the arcade frame.
- Match the surrounding code: comment density, naming, idiom.

## Testing

Every change keeps `ctest` green.

- `enhance` - always runs (scalers, presets, blitter at scale 2, glow)
- `rom_load` - label `rom`: loads the set from the zip, from renamed copies
  in a folder (CRC matching), and fails cleanly on an empty folder
- `mame_rom_map` - label `mame`: MAME's view of the main CPU's ROM
  ($0000-$8FFF with ROM selected, $D000-$FFFF) equals `joust --dump-rom`
- Tests that run the game use their own save dir; never write to the user's
  home folder.

## Reference

- ROM: MAME `joust` (white/green label, 0.289 names); CRCs in `src/assets/rom.c`.
- Hardware: Williams first generation, as Robotron (6809 main CPU, 6808 sound
  CPU, blitter, 16-colour palette). Notes go in the plan as they are confirmed.
- MAME 0.289 runs it headless; see `src/tests/mame/rom_map.cmake` for the options.
