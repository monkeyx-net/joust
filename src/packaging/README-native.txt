Joust (native SDL2 build)
=========================

Converted by monkeyx.

Run ./joust, or joust.exe on Windows (add --windowed for a window on a
desktop). The game is the arcade's program rewritten in C; the port adds
the graphics presets, pad controls and saving. On the first start it
extracts what it needs from your ROM set (the program, the factory
settings and the sounds) into an assets folder next to the program
($JOUST_ASSETS if set); after that the ROM set is no longer needed.
--emulate runs your ROM on an emulated Williams board instead (it needs
the ROM set every time). Your settings and high scores (the
arcade's CMOS) are saved as joust.cmos in $JOUST_SAVE_DIR, else
$XDG_DATA_HOME/joust, %APPDATA%\joust on Windows, or ~/.local/share/joust.
./joust --test-card shows the ROM status, an input tester and the images
decoded from your ROM.
In the attract mode, when nobody touches the controls for 30 seconds, a
page lists the controls (and comes back every 40 seconds); any button
hides it.

ROM set
  The first start needs your own MAME joust ROM set. Put joust.zip (or a joust/
  folder) next to the program, set JOUST_ROM to its path, or start with
  --rom /path/to/joust.zip. FBNeo sets and old file names work too: ROMs
  are matched by checksum. Check a set with: ./joust --check-rom joust.zip

Controls (player 1 / player 2)
  Move: left stick / D-pad; keyboard Left Right / A D
  Flap: A, B, X or Y; keyboard Space or Ctrl / W
  Coin: Select / 5
  Start: Start / 1 or Enter;  2-player start: L1+Start / 2
  Operator menu: Select+R1 / F2 (the cabinet's Advance button)
  Graphics: Select+L1 / F3 steps the presets, F4 glow, F5 CRT scanlines
  Exit: hold Select+Start for 1 second / Esc
  The first pad is player 1, the second player 2.

Graphics presets (--gfx <preset>)
  classic, classic-crt, crisp, smooth, hd-crisp, hd-smooth, hd-glow, hd-glow-crt
  (in the game the hd- presets upscale the whole picture, like crisp/smooth)

See NOTICE for the third-party material this project refers to.
