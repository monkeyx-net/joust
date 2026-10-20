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
- Frame-record tables found so far: $0DF9 (36 records: the ostrich, stork
  and buzzard mounts), $232A (18), $2F0B (7: eggs, hatching), $34A8 (6:
  pterodactyl). A scan for more by pattern alone gives mostly noise.
- Drawing: the IRQ handler ($E460-$E4B1) walks a display list in RAM; each
  node is [ctrl][solid][src hi][src lo][dst hi][dst lo][w][h][next], copied
  to the blitter with `LDS #$CA08; PULU CC,D,DP,X,Y; PSHS Y,X,DP,D,CC` (the
  push order lands the bytes on $CA00-$CA07, $CA00 last). Text goes through
  $4A70-$4B2F (font tables at $4B5C and $4BC6), digits through $E7D4.
- RAM: player 1's lives are kept in $A051 and $A052 (both counted down per
  death; poking both every frame gives unlimited lives for captures).
- `src/assets/images.c` lists 166 images from the attract mode, a 30-minute
  capture with unlimited lives and the frame-record tables: platforms and
  bridge pieces, the ostrich (P1) and stork (P2) in every frame, buzzards,
  riders and lances in each colour, eggs and hatching knights, the troll's
  hand and the lava flames, the pterodactyl, digits, two fonts. Not yet
  seen: whatever only later waves draw (the unlimited-lives run stays on
  the early layout). Each entry is named by what it shows (groups:
  platforms, ostrich, stork, buzzard, riders by colour, eggs, hatching,
  troll hand, flames, sparks, pterodactyl, two fonts, life icons).
- Table $0DF9 holds two mounts x 9 poses x 2 facings (records 2p, 2p+1 are
  a mirrored pair): records 0-17 the stork, 18-35 the ostrich; per mount,
  poses 0 braking, 1-5 the run cycle (pose 5 repeats pose 1), 6-8 flying.
- `src/tests/mame/dasm_probe.lua` writes MAME's disassembly to `build/re`
  for reading the code (never commit its output).

### Game code (P2 findings)

- Direct page $A0 (RAM $A000-$A0FF) holds the game's globals. Jump table at
  $E009-$E03A (e.g. $E018 random, $E037 wait some frames: `LDA #n; JSR $E037`).
- Random numbers: a 16-bit shift register at $A00D:$A00E. `$E4C0` steps it:
  A = ((lo >> 3) ^ lo) >> 1, the bit shifted out goes into hi's top bit and
  hi's low bit into lo's top bit (ROR $0D; ROR $0E); callers use A and the
  carry (lo's old bit 0). The IRQ handler stirs it 4 times a frame
  (INC $0D; DEC $0E at $E395). The only caller of the step found so far is
  $E54F. `sim/prng.c`; `mame_prng` replays ~20000 transitions from power-on.
- `src/tests/mame/access_probe.lua` logs who reads and writes chosen
  addresses (program counter and count): how the PRNG's users were found.
- Next: the task scheduler behind $E037 and the object structures the
  display list is built from; then flap, gravity and collisions.

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
| P1 | (done 2026-10-20 bar later-wave-only images: video and palette decoding, blitter emulation, 166 named images checked against MAME) Graphics from the ROM: find the image tables (ostriches, buzzards, knights, pterodactyl, eggs, platforms, lava, troll hand, font) and the palette; extractor writes `assets/` at first run | Extracted frames drawn by the port match MAME screenshots of the same scenes |
| P2 | (started 2026-10-20: PRNG ported and checked) Reverse engineering: RAM map, object lists, PRNG, flap and gravity physics, collisions and joust outcome, wave tables and spawn rules | Lua probes dump each documented variable; notes in this plan |
| P3 | Sim, wave 1: player, Bounders, eggs and hatching, platforms, scoring, death and respawn, deterministic from scripted input | Per-frame traces equal MAME's for scripted wave-1 games |
| P4 | Full game: all wave types (egg, survival, gladiator, pterodactyl), Hunters and Shadow Lords, troll hand, lava, crumbling platforms, extra lives, 2 players | Traces match MAME through wave 10 |
| P5 | Presentation: attract mode, high-score table and entry, operator menu (CMOS settings), sound (6808 + DAC emulation of the sound ROM, or player-side capture) | Device checklist on 3 handhelds + Steam Deck |
| P6 | Release: packages, PortMaster submission, GitHub release | Published |
