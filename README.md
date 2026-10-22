# Joust - native SDL2 port (work in progress)

A C11 / SDL2 port of the 1982 Williams arcade game for Linux handhelds
(PortMaster: ArkOS, ROCKNIX, muOS, Knulli), desktops (Linux, Windows, macOS) and the
web, checked against the original ROM running in MAME.

**Status:** playable. The game is rewritten in C, routine by routine, from
the arcade's program (checked step by step and frame by frame against an
emulated Williams board, which is itself checked against MAME cycle for
cycle and is still there with `--emulate`); the port adds enhanced graphics presets, pad controls,
CMOS saving and packaging for handhelds, desktops and the web. See
[JOUST_PORT_PLAN.md](JOUST_PORT_PLAN.md).

You need your own MAME `joust` ROM set (`joust.zip`); none is included.

Converted by monkeyx.

```bash
cmake -S . -B build && cmake --build build -j
build/joust --windowed --rom /path/to/joust.zip
```

## Controls

Two players at once. The first pad is player 1, the second player 2.

| Action | Pad | Keyboard (player 1 / player 2) |
|---|---|---|
| Move left / right | left stick or D-pad | Left Right / A D |
| Flap | A, B, X or Y | Space or Ctrl / W |
| Coin | Select | 5 |
| 1-player start | Start | 1 or Enter |
| 2-player start | L1 + Start | 2 |
| Operator menu (the cabinet's Advance button) | Select + R1 | F2 |
| Next graphics preset | Select + L1 | F3 |
| Glow on / off | | F4 |
| CRT scanlines on / off | | F5 |
| Quit | hold Select + Start for 1 s | Esc |

In the operator menu's adjustments page, as on the cabinet, player 1's
stick picks an adjustment and player 2's changes it (keyboard A D); with
one pad, up / down on player 1's stick or D-pad (keyboard Up Down) change
it too. Advance (Select + R1 / F2) moves on.

In the attract mode, after 30 seconds without input, a page lists these
controls (for 10 seconds, every 40 seconds; any input hides it).

Pads use SDL's game controller database; set `JOUST_GAMECONTROLLERDB` to a
`gamecontrollerdb.txt` to add mappings.

## Running

```bash
build/joust [--windowed] [--gfx preset] [--rom path] [--test-card] [--page N]
            [--emulate] [--input script.txt] [--frames N] [--screenshot out.png]
build/joust --check-rom roms/joust.zip            # verify a ROM set
build/joust --extract roms/joust.zip [dir]        # what the C game needs, into assets/
build/joust --dump-rom roms/joust.zip D000 D0FF   # main-CPU ROM bytes, "ADDR BYTE" hex
build/joust --list-images                         # the image catalogue, ADDR:W:H
```

- `--gfx`: `classic` (default), `classic-crt`, `crisp`, `smooth`, `hd-crisp`,
  `hd-smooth`, `hd-glow`, `hd-glow-crt`.
- `--test-card`: ROM status, an input tester, the presets, and pages of the
  images decoded from the ROM (`--page N`, or Select + R1 / F2 to step).
- The game runs rewritten in C (`src/sim/`) from `assets/`: the program
  image, factory settings and sounds, extracted once from your ROM set
  (automatically on the first start with the set present, or with
  `--extract`); after that the ROM set is no longer needed. The self-test,
  the operator's test mode, bookkeeping and adjustments are in C too.
- `--emulate`: run your ROM on the emulated board instead (cycle-exact
  against MAME); it needs the ROM set every time.
- `--input`, `--frames`, `--screenshot`: scripted, headless runs
  (`SDL_VIDEODRIVER=dummy`); see the comment at the top of `src/main.c`.

The ROM set is found from `--rom`, else `$JOUST_ROM`, else `joust.zip` or a
`joust/` folder next to the program or in the current folder (development
builds also look in the source tree's `roms/`). Files are matched by CRC, so
MAME, older MAME and FBNeo sets all work.

The arcade's CMOS (settings, high scores) is saved as `joust.cmos` in
`$JOUST_SAVE_DIR`, else `$XDG_DATA_HOME/joust`, `%APPDATA%\joust` on
Windows, or `~/.local/share/joust`. On the first start (no CMOS yet) the
game restores its factory settings before the window opens.

## Building

Needs CMake 3.13+, a C11 compiler and the SDL2 development files (2.0.10 or
newer; releases build against 2.0.10 in Ubuntu 20.04).

### Linux desktops and the Steam Deck

```bash
sudo apt install build-essential cmake pkg-config libsdl2-dev   # Debian / Ubuntu
cmake -S . -B build && cmake --build build -j
ctest --test-dir build
```

On aarch64 handhelds build natively on the device or in an aarch64 Ubuntu
20.04 container (as CI does) so the binary runs on their older glibc.

### CMake options

| Option | Default | Effect |
|---|---|---|
| `CMAKE_BUILD_TYPE` | `Release` | the usual CMake build types |
| `JOUST_DEV_ASSETS` | `ON` | development builds fall back to the source tree (`roms/`, `assets/`); set `OFF` for releases |
| `JOUST_SANITIZE` | `OFF` | build with ASan and UBSan |
| `JOUST_ROM` | `roms/joust.zip` (or `roms/joust/`) | the ROM set the `rom` and `mame` tests use |
| `JOUST_WEB_ROM` | empty | web only: bundle this `joust.zip` into the page, for local testing only (never publish such a build) |
| `JOUST_WEB_PORT` | `8000` | web only: port of the `serve` target |

```bash
cmake -S . -B build-san -DJOUST_SANITIZE=ON && cmake --build build-san -j   # sanitizers
```

### Windows (MinGW-w64 in MSYS2)

From the MSYS2 MINGW64 shell:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
          mingw-w64-x86_64-pkgconf mingw-w64-x86_64-SDL2
cmake -S . -B build -G Ninja -DJOUST_DEV_ASSETS=OFF
cmake --build build --target joust
cp /mingw64/bin/SDL2.dll build/          # the package takes every DLL next to the exe
cmake --build build --target package_windows
```

The tests use POSIX calls, so on Windows only the game is built.

### macOS (Homebrew)

```bash
brew install sdl2 pkg-config
cmake -S . -B build -DJOUST_DEV_ASSETS=OFF && cmake --build build -j
cmake --build build --target package_macos   # joust-<version>-macos-<arch>.tar.gz
```

### Web (Emscripten)

With the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
active:

```bash
emcmake cmake -S . -B build-web
cmake --build build-web --target serve    # builds, then serves http://localhost:8000/
```

The page is `build-web/web/index.{html,js,wasm}`. No ROM is bundled, so a
plain web build shows the test card; for local play only, add your own set
with `-DJOUST_WEB_ROM=/path/to/joust.zip` (do not publish that build). CMOS
saving is not persisted in the browser yet.

### Release packages

```bash
cmake -S . -B build -DJOUST_DEV_ASSETS=OFF && cmake --build build -j
cmake --build build --target package_linux package_portmaster
```

Writes `build/packages/joust-<version>-linux-<arch>.tar.gz` and
`joust-<version>-portmaster-<arch>.zip` (the PortMaster launcher
`Joust.sh` and a `joust/` folder; players add their `joust.zip` there).
Packages hold the program, `README-native.txt`, `LICENSE` and `NOTICE`, never
ROM data.

## Tests

```bash
ctest --test-dir build              # everything available
ctest --test-dir build -L rom       # needs the ROM set (roms/joust.zip)
ctest --test-dir build -L mame      # needs MAME 0.289 and the ROM set
```

- `enhance` (always): scalers, presets, glow.
- `rom_load` (`rom`): the loader, from the zip and from renamed files.
- `extract` (`rom`): the extractor, and the C game from its output.
- `game`, `game_runner`, `game_two` (`rom`): every task step of three
  scripted games run in C against the emulated board's, byte for byte.
- `routines` (`rom`): ROM routines and task steps on random inputs against
  the C ones (collisions, scoring, brains, the waves, the high score entry,
  the troll and more).
- `operator` (`rom`): the system program (self-test, CMOS check, test
  mode, bookkeeping, adjustments) routine by routine against the ROM's.
- `frame`, `frame_two` (`rom`): the C game alone, frame by frame against
  the board, given the board's interrupt timing.
- `mame_rom_map`, `mame_capture`, `mame_images`, `mame_prng`, `mame_emu`
  (`mame`): the port against MAME: the ROM map, video and palette decoding,
  the blitter on every catalogued image, the random number generator, and
  the emulated board from power-on (20000 random-state changes in order and
  frames pixel for pixel).

## Continuous integration

`.github/workflows/build.yml` runs on every push and pull request to `main`,
and by hand (Actions > Build > Run workflow):

1. **Version:** reads the top `### vx.y.z` heading of `CHANGELOG.md` and
   fails unless `project(joust VERSION x.y.z)` in `CMakeLists.txt` agrees.
   On a push, if the release `vx.y.z` already exists, the builds are skipped.
2. **Linux x86_64 and aarch64:** in an Ubuntu 20.04 container (GCC 9,
   SDL 2.0.10, glibc 2.31 for PortMaster) on GitHub's x86_64 and arm64
   runners, with ccache: build with `JOUST_DEV_ASSETS=OFF`, run `ctest` (CI
   has no ROM set, so only `enhance` runs), make the Linux and PortMaster
   archives.
3. **Windows x86_64:** MSYS2 MinGW64 with Ninja; builds the game, copies the
   MinGW DLLs it links (SDL2.dll) next to it and makes the Windows zip.
4. **macOS arm64:** Homebrew SDL2 with ccache; build, run `ctest`, make the
   macOS tarball.
5. **Web:** Emscripten; zips `build-web/web` as `joust-<version>-web.zip`
   (no ROM: the page shows the test card until a ROM is added).
6. **Release:** on a push whose version has no release yet, downloads every
   build's archives and publishes GitHub release `vx.y.z` with generated
   notes.

Every job uploads its archives as workflow artifacts. To publish a release,
add a `### vx.y.z (date)` section at the top of `CHANGELOG.md`, bump the
version in `CMakeLists.txt` to match and push to `main`.

## License

Apache 2.0 for the source code ([LICENSE](LICENSE)). Joust itself, its ROMs,
artwork and sounds are not covered; see [NOTICE](NOTICE).
