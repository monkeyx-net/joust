# Joust - native SDL2 port (work in progress)

A C11 / SDL2 rebuild of the 1982 Williams arcade game for Linux handhelds
(PortMaster: ArkOS, ROCKNIX, muOS, Knulli), desktops (Linux, Windows) and the
web, checked against the original ROM running in MAME.

**Status:** P0. The ROM loader, the MAME reference pipeline and a test card
(ROM status, two-player input tester, graphics presets) work; the game itself
is still to come. See [JOUST_PORT_PLAN.md](JOUST_PORT_PLAN.md).

You need your own MAME `joust` ROM set (`joust.zip`); none is included.

```bash
cmake -S . -B build && cmake --build build -j
build/joust --windowed --rom /path/to/joust.zip
```
