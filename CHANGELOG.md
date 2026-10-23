# Joust — native SDL2 port — Changelog

---
## Changelog

### v0.2.3 (2026-10-23)

#### Port

- **Lower CPU use** — the main loop sleeps until the next frame is due
  instead of redrawing and presenting again (no spinning where vsync does
  not block, no extra frames on 120/144 Hz displays); the graphics
  presets' scalers (Scale2x/3x, xBR) read their neighbours without bounds
  checks inside the picture, write flat areas at once and skip the
  premultiply pass for the opaque game frame. Pictures unchanged, bit for
  bit.

### v0.2.2 (2026-10-22)

#### Build & Platforms

- **macOS build** — Joust now builds on macOS with Homebrew's SDL2
  (`brew install sdl2 pkg-config`, then the usual CMake build); saves go to
  `$XDG_DATA_HOME/joust` or `~/.local/share/joust`, as on Linux.
- **macOS package** — new `package_macos` target writes
  `joust-<version>-macos-<arch>.tar.gz` (the program, `README-native.txt`,
  `LICENSE` and `NOTICE`; no ROM data, players add their own `joust.zip`).
- **CI** — a `build-macos` job on GitHub's arm64 macOS runners builds with
  ccache, runs the tests, makes the arm64 tarball and adds it to the release
  beside the Linux, PortMaster, Windows and web archives.
- **README** — macOS in the platform list, a Homebrew build section and the
  macOS step of the CI process.

### v0.2.1 (2026-10-22)

#### Game

- **The game in C by default** — the whole ROM program rewritten in C
  (`src/sim/`), routine by routine, each mirroring the ROM's arithmetic and
  order of stores: the task kernel, the random generator, the ROM check, the
  lava and its troll, collisions and their outcomes (unhorsings, bounces,
  ties), the players and enemies in the air and on the ground, platforms,
  eggs and hatching, the riderless mounts, the pterodactyl, spawning, deaths
  and turns, scoring and extra lives, the waves and their messages, burning
  platforms and lava bridges, the colour cycle, the secret code, the shell
  (start buttons, game over, the demo), the attract title page and the high
  score table and entry. `--emulate` still runs the ROM on the emulated
  board.
- **The interrupt and the frame loop in C** — palette, switches, coins and
  the display list's blits; the C game runs from its own power-on on a fixed
  schedule, with no board.
- **The system program in C** — the self-test (including the endless one the
  adjustments can select), the CMOS check and factory restore, the test
  mode, bookkeeping and adjustments.

#### Data & ROM

- **The extractor** — `joust --extract` (or the first start with the ROM set
  present) writes the program image, the factory CMOS and the sounds to
  `assets/`, which the C game reads instead of the ROM set; first-start
  extraction honours `JOUST_ASSETS`.

#### Build & Platforms

- **Packages** — the packaged README describes the C game; the package
  screenshot is always the test card.

#### Tests

- **The C game against the board** — `game`, `game_runner` and `game_two`
  follow the board's scripted games task step by task step; `frame` and
  `frame_two` run the C game alone against them frame by frame; `routines`
  checks ROM routines against the C ones on 20000 random cases each;
  `operator` checks the system program's routines; `extract` checks the
  extractor and that the C game from it equals the one from the ROM set.

### v0.1.0 (2026-10-20)

#### Game

- **Playable on the emulated board** — a 6809 core and Joust's Williams
  board (memory map, interrupts, blitter, CMOS, video per line) run the
  ROM's code, matching MAME; the sound board (6808, PIA, DAC) runs line by
  line beside it.
- **Controls and saves** — two players (move + flap) on pads and keyboard
  mapped to the cabinet's switches; the CMOS (settings, high scores) saved
  to `joust.cmos`, primed on first start.
- **Controls page** — after 30 s without input in the attract mode a page
  lists the pad and keyboard controls.

#### Data & ROM

- **ROM set loader** — the MAME `joust` set from a zip or folder, matched by
  CRC; `--check-rom`, `--dump-rom`.
- **Image catalogue** — 166 named ROM images decoded through the blitter and
  the ROM's palette (`--list-images`, the test card's image pages).

#### Graphics & Sound

- **Platform layer** — from the Robotron port: framebuffer at any scale,
  integer scaling, scanline overlay, enhanced graphics presets (Scale2x/3x,
  xBR, glow, CRT), the priority mixer with a stream input for the sound
  board.
- **Test card** — ROM status, input tester and graphics presets
  (`--test-card`).

#### Build & Platforms

- **Linux, Windows and web builds** — CMake builds for Linux (x86_64 and
  aarch64 handhelds), Windows (MinGW-w64) and the web (Emscripten).
- **Release packages** — `package_linux`, `package_portmaster` and
  `package_windows`.
- **CI workflow** — `.github/workflows/build.yml` builds Linux, Windows and
  web on every push to `main`, runs the tests and publishes a GitHub release
  when the version is new.

#### Tests

- **MAME reference tests** — the ROM map, video RAM and palette captures,
  every catalogued image in five blitter modes, the random generator's every
  stir and step, and the emulated board from power-on (random-state changes
  and frames pixel for pixel) against MAME.

#### Documentation

- **README, CLAUDE.md and JOUST_PORT_PLAN.md** — every key and pad button,
  build options, tests and CI; the port's ground rules and plan.
