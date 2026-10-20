# Joust native port: plan

A C11 / SDL2 rebuild of Joust (Williams, 1982) that plays like the arcade
game, for PortMaster handhelds (aarch64), desktops (x86_64 Linux, Windows) and
the web. Unlike the sibling Robotron port, there is no remake to port from:
the ROM, running in MAME, is the only reference.

## 1. Decisions (2026-10-20)

1. **No ROM-derived data in the repo, the packages or the binary.** Sprites,
   fonts, tables and sounds all come from the player's own ROM set: read at
   run time, or extracted to `assets/` on first run. CI and releases carry
   none. (Robotron ships sprites and sounds from its web remake; Joust has no
   such source, and this keeps the project clean.)
2. **MAME is the reference.** Behaviour is established by probing the ROM in
   MAME 0.289 (RAM dumps, per-frame traces with scripted input) and checked
   by CTest against the port. Probes are Lua and only dump; C or CMake compares.
3. **Shared code is copied, not linked.** The platform layer, loaders,
   framebuffer, blitter and enhanced graphics came from the Robotron port
   (2026-10-20). Revisit a shared library once Joust plays (end of P3), when
   it is clear what is really common.
4. **Controls:** two players at once, each with left/right and flap; first
   pad is player 1, second player 2. Select = coin, Start = 1P, L1+Start = 2P,
   Select+R1 = operator menu, Select+L1 = graphics presets, hold Select+Start
   1 s = exit (as Robotron). Keyboard: P1 arrows + Space/Ctrl, P2 A/D + W.
5. **Enhanced graphics** as in the Robotron port (FILTER and HD presets,
   glow, scanlines), cosmetic only, classic by default.

## 2. Hardware (Williams first generation)

Confirmed in this repo (each by a `mame` test):
- Program ROM: 12 x 4 KB. Banks 1-9 at $0000-$8FFF under video RAM, read
  when bit 0 of $C900 selects ROM; banks 10-12 at $D000-$FFFF. Reset vector
  $F000. (`mame_rom_map`: all 48 KB.)
- Video (`mame_capture`, three frames pixel for pixel): video RAM
  $0000-$BFFF is 192 columns of 256 bytes, byte (x/2)*256 + y holding pixel x
  in the high nibble; the visible area is x 6-297, y 7-246 (292 x 240).
  Palette $C000-$C00F, one BBGGGRRR byte per entry through resistor weights
  (R, G 1200/560/330 ohms, B 560/330 ohms), each channel scaled to 255 on
  its own.
- Blitter (`mame_images`, 89 images x 5 modes byte for byte): $CA00 control
  (bits: 01 source and 02 destination in screen layout, 04 slow, 08 skip
  zero pixels, 10 solid colour from $CA01, 20 shift one pixel right, 40/80
  keep odd/even destination pixels), $CA02-03 source, $CA04-05 destination,
  $CA06/$CA07 width (bytes) and height XOR 4 (the SC1 quirk). In shift mode
  the shifted-out pixel carries into the next row.
- Sound ROM 4 KB for the 6808 sound board.

To confirm as the work needs it: input PIAs, CMOS layout, watchdog, the
game's RAM map.

### Graphics in the ROM (P1 findings, 2026-10-20)

- The game draws everything with the blitter from ROM sources (romsel set),
  in modes 0A (sprite), 1A (sprite in a solid colour; 00 erases), 2A/3A
  (the same, shifted for odd x) and 12 (solid rectangle).
- Sprites carry a 2-byte header before their data: width and height XOR 4,
  ready for $CA06/$CA07. Font glyphs ($4C2A-$5007, 3 x 7 bytes) have a header
  with the plain width and height. Shifted blits are one byte wider.
- Animation frames are 6-byte records [collision data pointer][dx, dy as
  signed bytes][image header pointer], e.g. the mount frames at $0DF9-$0ECE.
  The collision data is a list of byte pairs per row (P2).
- The in-game palette is a 16-byte table at $E5FA.
- `src/assets/images.c` lists the 89 images seen in a 47 s capture (attract,
  wave 1): platforms, ostrich and buzzard frames, riders and lances, sparks,
  digits, the font. Not yet seen: pterodactyl, eggs hatching, the troll
  hand, later-wave enemies. Next: walk the frame-record tables to list every
  image, then name them.

## 3. Test plan

- **Unit** (`ctest`, no ROM): scalers, presets, blitter, glow (`enhance`);
  the sim's pieces as they arrive.
- **ROM** (label `rom`): the loader (`rom_load`); later the extractor's
  output against hashes recorded when it matched MAME.
- **MAME** (label `mame`): `mame_rom_map`, `mame_capture` (video RAM and
  palette decoding against MAME snapshots), `mame_images` (the blitter
  against MAME's, per image and mode); later per-frame RAM traces of
  scripted games compared with the sim (positions, velocities, PRNG, score),
  and screenshots compared with the port's classic frames.
- **Golden screenshots** of scripted runs, classic and enhanced, once there
  is a game to replay.
- **CI** (`.github/workflows/build.yml`): Linux x86_64/aarch64 in Ubuntu
  20.04, Windows, web; no ROM in CI, so only the unit tests run there.

## 4. Phases

| Phase | Work | Exit criterion |
|---|---|---|
| P0 | Repo, ROM set loader (CRC matching, zip or folder), MAME pipeline, platform layer, test card, packaging, CI (done 2026-10-20) | `enhance`, `rom_load`, `mame_rom_map` pass |
| P1 | (in progress: video and palette decoding, blitter emulation and 89 captured images done, 2026-10-20) Graphics from the ROM: find the image tables (ostriches, buzzards, knights, pterodactyl, eggs, platforms, lava, troll hand, font) and the palette; extractor writes `assets/` at first run | Extracted frames drawn by the port match MAME screenshots of the same scenes |
| P2 | Reverse engineering: RAM map, object lists, PRNG, flap and gravity physics, collisions and joust outcome, wave tables and spawn rules | Lua probes dump each documented variable; notes in this plan |
| P3 | Sim, wave 1: player, Bounders, eggs and hatching, platforms, scoring, death and respawn, deterministic from scripted input | Per-frame traces equal MAME's for scripted wave-1 games |
| P4 | Full game: all wave types (egg, survival, gladiator, pterodactyl), Hunters and Shadow Lords, troll hand, lava, crumbling platforms, extra lives, 2 players | Traces match MAME through wave 10 |
| P5 | Presentation: attract mode, high-score table and entry, operator menu (CMOS settings), sound (6808 + DAC emulation of the sound ROM, or player-side capture) | Device checklist on 3 handhelds + Steam Deck |
| P6 | Release: packages, PortMaster submission, GitHub release | Published |
