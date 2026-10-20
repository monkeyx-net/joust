# Joust - native SDL2 port (work in progress)

A C11 / SDL2 rebuild of the 1982 Williams arcade game for Linux handhelds
(PortMaster: ArkOS, ROCKNIX, muOS, Knulli), desktops (Linux, Windows) and the
web, checked against the original ROM running in MAME.

**Status:** playable. The game is your ROM's own code on an emulated Williams
board (6809 main board, 6808 sound board), written in C and checked against
MAME cycle for cycle; the port adds enhanced graphics presets, pad controls,
CMOS saving and packaging for handhelds, desktops and the web. See [JOUST_PORT_PLAN.md](JOUST_PORT_PLAN.md).

You need your own MAME `joust` ROM set (`joust.zip`); none is included.

Converted by monkeyx.

```bash
cmake -S . -B build && cmake --build build -j
build/joust --windowed --rom /path/to/joust.zip
```
