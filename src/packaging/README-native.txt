Joust (native SDL2 build) - work in progress
============================================

Run ./joust, or joust.exe on Windows (add --windowed for a window on a
desktop). This early build shows a test card: ROM status, a two-player
input tester and the graphics presets. The game itself is still to come.

ROM set
  The game needs your own MAME joust ROM set. Put joust.zip (or a joust/
  folder) next to the program, set JOUST_ROM to its path, or start with
  --rom /path/to/joust.zip. FBNeo sets and old file names work too: ROMs
  are matched by checksum. Check a set with: ./joust --check-rom joust.zip

Controls (player 1 / player 2)
  Move: left stick / D-pad; keyboard Left Right / A D
  Flap: A, B, X or Y; keyboard Space or Ctrl / W
  Coin: Select / 5
  Start: Start / 1 or Enter;  2-player start: L1+Start / 2
  Operator menu: Select+R1 / F2
  Graphics: Select+L1 / F3 steps the presets, F4 glow, F5 CRT scanlines
  Exit: hold Select+Start for 1 second / Esc
  The first pad is player 1, the second player 2.

Graphics presets (--gfx <preset>)
  classic, classic-crt, crisp, smooth, hd-crisp, hd-smooth, hd-glow, hd-glow-crt

See NOTICE for the third-party material this project refers to.
