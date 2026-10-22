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
The sibling Defender port (`../defender`, from this repo) is further along
the same road: its `DEFENDER_PORT_PLAN.md` and `CLAUDE.md` show how the C
game, the extractor and their tests are built; follow its patterns.

## Ground Rules

- **The game is being rewritten in C** (plan decision 6), as the Defender
  port's is: game logic from the ROM's behaviour, routine by routine, each
  mirroring the ROM routine's arithmetic and order of stores, checked step
  by step against the emulated board (`src/emu/`). Until it reaches parity
  the emulated board runs the game. MAME is the reference for the
  emulation: changes to `emu/` must keep `mame_emu` (and the other `mame`
  tests) passing. Write all emulation from the hardware's behaviour, not
  from MAME's source.
- **The ROM set is to be read once** (plan decision 1): `joust --extract`
  (phase X1, not written yet) will write pictures, fonts, palette, sounds
  and tables to `assets/`, which the C game reads instead of the ROM. No
  ROM-derived data in the repo, the binary or the packages: not sprites,
  fonts, tables or sounds; the extractor's tables name ROM addresses only.
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
build/joust --windowed                            # play (finds roms/joust.zip in dev builds)
build/joust --windowed --test-card                # the test card and image pages
build/joust --windowed --c-game                   # the game in C (sim/), from assets/ (extracted once)
build/joust --extract roms/joust.zip [dir]        # program, factory CMOS and sounds into assets/
build/joust --windowed --gfx hd-glow              # enhanced graphics (presets: classic ... hd-glow-crt)
SDL_VIDEODRIVER=dummy build/joust --frames 60 --screenshot out.png   # headless screenshot
build/joust --check-rom roms/joust.zip            # verify a ROM set
build/joust --dump-rom roms/joust.zip D000 D0FF   # main-CPU ROM bytes, "ADDR BYTE" hex
build/joust --list-images                         # the image catalogue, ADDR:W:H
build/joust --windowed --page 1                   # the test card's image pages (Select+R1 / F2 steps)
cmake --build build --target package_linux package_portmaster   # release archives
cmake -S . -B build-san -DJOUST_SANITIZE=ON       # ASan/UBSan build
emcmake cmake -S . -B build-web && cmake --build build-web --target serve   # web build on http://localhost:8000/
```

Release builds: `-DJOUST_DEV_ASSETS=OFF` (no source-tree fallbacks).

## Layout (`src/`)

- `main.c` - SDL setup, fixed 60 Hz loop running one board frame per tick
  (or with `--c-game` one frame of the C game, sim/cgame.c, from its own
  power-on, the board then only sounding),
  the graphics pipeline, CMOS saving, the controls page over the attract
  mode (after 30 s idle; `$A09F` = 2 tells a game from the attract mode),
  the test card (`--test-card`),
  `--input`, `--frames`, `--screenshot`, `--rom`, `--check-rom`, `--dump-rom`
- `emu/` - the emulated hardware: `m6809.c` (main CPU), `pia6821.c`,
  `board.c` (Joust's main board: memory map, interrupts, blitter, CMOS,
  video per line), `m6800.c` + `sound.c` (the sound board and its DAC)
- `platform/` - the only code that includes SDL: `video.c` (292x240 framebuffer
  at any scale, integer scaling, scanline overlay), `input.c` (two players:
  move + flap; pads and keyboard), `audio.c` (priority mixer; the game
  registers its sounds)
- `gfx/` - draws into the framebuffer in arcade coordinates (`fb.scale` > 1 for
  HD): `fb.c`, `blit.c`, `scale.c` (Scale2x/3x, xBR), `enhance.c` (graphics
  presets, HD image registry, whole-frame filter, glow), `debugfont.c` (3x5
  font for diagnostics, not the ROM's), `williams.c` (video RAM and palette
  decoding, the SC1 blitter as MAME emulates it)
- `assets/` - `extract.c` (`--extract`: what the C game needs, from the ROM
  set once), `savefile.c` (the save folder: joust.cmos), `rom.c` (the joust set from a zip or folder, matched by CRC;
  CPU address map), `images.c` (the image catalogue: ROM addresses and
  sizes, decoded through the blitter and the ROM's palette), `zip.c`,
  `inflate.c`, `png.c` (RGB and RGBA), `assets.c` (asset folder lookup)
- `sim/` - the game in C, piece by piece (plan decision 6, notes in the
  plan's "The game program in C"): `game.c` (the C game's state: a model of
  all of RAM, raw bytes in the ROM's layout, and the ROM select; the
  random step and sound requests the tasks share; `game_step` runs a task
  step in C), `tasks.c` (the task kernel: create, sleep, end, kill by
  type), `prng.c` (the random generator, $E4C0, and the IRQ's stir),
  `checksum.c` (the ROM check task, $ECF8), `lava.c` (the lava's surface
  and bubbles, $E4E6), `collide.c` (the collision walks $837F/$83A4, the
  pair test $83D8 and the data test $DC21; a collision's handlers not yet),
  `scores.c` (the score digits, $E785; blitted with `game_blit`),
  `flow.c` (riders' turns to appear; the secret code $6B25); the model also keeps the controls, PIA 0's $C807 and
  its multiplexer (`game_controls`, $6BA6), `display.c` (the display
  list's nodes, $E2A1-$E38C), `rider.c` (the riders: in the air,
  $8F04/$8F3D, landing, on the ground, $8D4D, taking off and falling off;
  the players' brains and $EEC2's), `wave.c` (the wave manager's frequent path, $6D91), `eggs.c` (eggs in
  flight and at rest, hatching), `spawn.c` (a rider appearing on a
  pad and waiting on it), `death.c` (a player's death, $8279), `score.c` (scores, extra lives, CMOS audits;
  the model keeps the CMOS and a faithful `DAA`), `brains.c` (the enemies' brains,
  $7A83-$80D5: state machines as basic blocks by ROM address; $80D6 the
  target), `platform.c` (landing on a platform, bouncing off one, $D9A3),
  `ptero.c` (the pterodactyl: start, brain, flight, death; $8209's pixels
  are `display_pixels`), `points.c` (points shown where they were won),
  `text.c` (the ROM's text printer, $4A50), `waves.c` (between waves:
  the settings, messages, platforms, enemies and eggs; return addresses
  kept in task fields across sleeps, as the ROM does), `burn.c` (platforms
  burning away, the lava bridges and flames), `cycle.c` (the colour cycle),
  `start.c` (the shell: start buttons, a game's set-up, game over, the
  demo), `attract.c` (the title page: logo, border lights), `hiscores.c`
  (the system ROM's high score table and entry), `troll.c` (the lava
  troll's hand), `irq.c` (the interrupt: palette, switches, coins, the
  display list's blits), `frame.c` (the frame loop: both passes, the
  sound queue, the layers; hooks for the interrupts' timing), `cgame.c`
  (the C game alone: the interrupt on a fixed schedule); each mirrors a ROM routine's arithmetic exactly
- `tests/` - C tests, MAME probes (`tests/mame/`)
- `packaging/` - release layout script, PortMaster launcher and metadata
- `web/` - page shell for the Emscripten build

## Conventions

- C11, `-Wall -Wextra`, no warnings. Builds must also pass in Ubuntu 20.04
  (GCC 9, SDL 2.0.10, the PortMaster base): use only the SDL 2.0.10 API.
- The C game is deterministic: a frame per tick, no wall clock, randomness
  from the ROM's own generator in the ROM's call order.
- `sim/` must not include SDL or touch the framebuffer: it draws into its model of
  video RAM, laid out as the arcade's, which `gfx/` decodes.
- Enhanced graphics are cosmetic: classic output stays the arcade frame.
- Match the surrounding code: comment density, naming, idiom.

## Testing

Every change keeps `ctest` green.

- `enhance` - always runs (scalers, presets, blitter at scale 2, glow)
- `extract` - label `rom`: the extractor (assets/extract.c) is repeatable,
  its program image is the ROM by address, its CMOS the factory settings,
  every sounding command has a WAV, and the C game from it equals the one
  from the ROM set
- `rom_load` - label `rom`: loads the set from the zip, from renamed copies
  in a folder (CRC matching), and fails cleanly on an empty folder
- `mame_rom_map` - label `mame`: MAME's view of the main CPU's ROM
  ($0000-$8FFF with ROM selected, $D000-$FFFF) equals `joust --dump-rom`
- `mame_capture` - label `mame`: video RAM and palette dumped from a scripted
  run, decoded by `gfx/williams.c`, equal MAME's snapshots pixel for pixel
- `mame_images` - label `mame`: MAME's blitter draws every catalogued image in
  five modes over a pattern; `gfx/williams.c` must write the same bytes
- Probes that are not tests: `ram_probe.lua` (work RAM at chosen frames, with
  scripted input and cheat pokes) and `dasm_probe.lua` (MAME's disassembly to
  `build/re`; never commit it). `capture_probe.lua` also takes `PROBE_CYCLE`
  (repeating input) and `PROBE_POKE` / `PROBE_POKE_FROM` (e.g. `A051=05
  A052=05` for unlimited lives once a game runs).
- `mame_prng` - label `mame`: every stir and step of the random state from
  power-on, replayed through `sim/prng.c`
- `game`, `game_runner`, `game_two` - label `rom`: a C model of all of RAM follows the board's
  scripted game (writes copied, the blitter's too, through `WilliamsBus.on_write`);
  ported task steps run in C when the dispatcher starts them ($E154) and the
  model must equal RAM and the ROM select when the ROM's step yields ($E15F).
  A step an interrupt broke into must match with the interrupt's writes all
  before or all after it, or replayed where they fell among its writes; a
  race the C game cannot have is counted and must stay under 1 in 1000
  (reported with where the interrupt broke in: so far always $E4CA, inside
  the random step). `game_runner` is a second, longer game, `game_two` a
  two-player one.
  The stack page $BE00-$BEFF is not compared. It prints the steps not
  ported by resume address (the to-do list) and, for ported ones, how
  many ran all the way in C (matched/run). Port a task's steps with the
  stores in the ROM's order (the interleaved replay depends on it), and
  check a new port by planting a bug: the test must catch it.
- `routines` - label `rom`: ROM routines run on the emulated CPU (direct page
  $A0, interrupts masked) from chosen RAM and registers until they return,
  against the C ones on the same RAM: $DC21, $83D8 with its outcome
  (unhorsings, eggs, pterodactyls), the display list's nodes, $80D6,
  scoring, $D9A3, the pterodactyl's and a riderless mount's brains and
  points placed ($77BE) (on a game in progress) on
  20000 random cases each, and the pterodactyl's, the wave task and the
  burning tasks step by step, and the high score entry's tasks as a list
  run frame by frame (each step run to its sleep, $E15F; the CPU's
  Y at random, `Game.y`; `Game.b` likewise for B); RAM and the CMOS.
  Keep the random cases possible in the game (e.g. only players collect
  eggs): an impossible state sends the ROM through garbage. Each call
  starts with DP $A0 and the ROM selected. `ONLY=n` traces one pair case. Use it where scripted play rarely reaches a path; aim cases at the
  edges. `TRACE=1` prints the ROM's path for the first cases.
- `frame`, `frame_two` - label `rom`: the C game alone (sim/frame.c: every
  step, the interrupt, the loop) against the board's scripted games frame
  by frame, given the board's timing through hooks (where interrupts came,
  their S and beam reads, the controls at each dispatch, where the second
  pass stopped); all of RAM and the CMOS at each $E0E2; 97% must match
  (the rest: interrupts inside a random step). `DBG=1` lists differences,
  `WATCH=n` the hooks around board frame n.
- `mame_emu` - label `mame`: the emulated board from power-on with the primed
  CMOS and the probes' script: all 20000 random-state changes in MAME's order
  and frames 1500/1650/1800 pixel for pixel. Inputs a probe sets in a frame
  callback reach the game at the start of the frame after next. To debug a
  timing difference, compare instruction traces: MAME's debugger `trace` with
  `tracelog "%d ",totalcycles` (needs `-debug -debugger none`) against
  `Board.step_hook`; MAME's frames start at line 247, reset takes 4 cycles.
- More RE probes: `access_probe.lua` (which code reads and writes given addresses)
- MAME tests share `build/mame` (one at a time) and start each run from a
  fresh copy of the primed CMOS (`src/tests/mame/mame_common.cmake`); MAME
  needs `-frameskip 0` for snapshots to show the frame they are taken in
- Tests that run the game use their own save dir; never write to the user's
  home folder.

## Reference

- ROM: MAME `joust` (white/green label, 0.289 names); CRCs in `src/assets/rom.c`.
- Hardware: Williams first generation, as Robotron (6809 main CPU, 6808 sound
  CPU, blitter, 16-colour palette). Notes go in the plan as they are confirmed.
- MAME 0.289 runs it headless; see `src/tests/mame/rom_map.cmake` for the options.
