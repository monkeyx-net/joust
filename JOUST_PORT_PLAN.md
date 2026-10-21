# Joust native port: plan

A C11 / SDL2 rebuild of Joust (Williams, 1982) that plays like the arcade
game, for PortMaster handhelds (aarch64), desktops (x86_64 Linux, Windows) and
the web. Unlike the sibling Robotron port, there is no remake to port from:
the ROM, running in MAME, is the only reference. Today the game is the
player's ROM running on an emulated board. The goal (decision 6, changed
2026-10-21) is the game rewritten in C, as the Defender port's is
(`../defender`, its plan and CLAUDE.md), reading only assets extracted once
from the ROM set, with the emulated board as its reference.

## 1. Decisions (2026-10-20)

1. **The ROM set is read once** (changed 2026-10-21, as Defender's decision
   1). The binary never embeds ROM-derived data; it loads it at run time
   from an `assets/` folder. On first run, if `assets/` is missing,
   `joust --extract <joust.zip>` (also run by the launcher when it finds
   the zip in the port folder) writes the graphics, fonts, tables and sound
   data there from the player's own ROM set; after that the ROM set is no
   longer needed. The repo, CI and releases still carry no ROM-derived data
   (the extractor's tables name ROM addresses only). This only pays off
   with the game written in C (decision 6): while the emulated board runs
   the ROM's code, it still reads the ROM set. The extractor is phase X1.
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
6. **The game in C** (changed 2026-10-21, as Defender's decision 6; it
   replaces running the ROM's code for good, decided 2026-10-20). The game
   logic is rewritten in C (`src/sim/`), routine by routine, each mirroring
   a ROM routine's arithmetic and its order of stores, from the ROM's
   behaviour (traces, probes, the disassembly MAME writes to `build/re`).
   The emulated board (`src/emu/`: a 6809, the Williams main board with
   PIAs, interrupts, blitter and CMOS, and the sound board's 6800 and DAC,
   checked against MAME by `mame_emu`) stays: it is the reference the C
   game is compared with, step by step and frame by frame, and it runs the
   game until the C version reaches parity. It also records the sounds at
   extraction; the 6808 is not needed at play time once the C game plays
   the WAVs. The P2 notes below are the start of the C game's reverse
   engineering.

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
- Tasks (the Williams cooperative kernel): a linked list from $A00A; a task
  node holds +0 next, +2 type, +3 a flag that defers it to the other of two
  passes per frame ($A065), +4 frames left to wait, +5-6 the address to
  resume at. `LDA #n; JSR $E037` ($E157) saves the caller's return address
  in the node and sleeps n frames; the dispatcher ($E15F-$E194) counts the
  waits down and resumes at $E152 (`STU <$2E; JMP [5,U]`; $A02E = current
  task). $E20F ends the current task (its node goes to the free list at
  $A008); $E24F ends every task of a type.
- Frame loop ($E0E2): two display lists, $A9C0 and $AC21, swapped each frame
  ($A027); it waits until the IRQ has drawn the previous one, clears four
  layer heads ($A000-$A0C6), and sends queued sound commands to the sound
  board through the PIA at $C80E ($A061-$A064).
- Next: the object structures that tasks build display-list nodes from;
  then flap, gravity and collisions.

### Game state (2026-10-20)

- $A09F: $00 in the first attract mode, $03 from a start until the game is
  under way, $02 while a game runs (1 or 2 players), $03 again from game
  over on (`ram_probe.lua`). The port shows its controls page over the
  attract mode (after 30 s without input) while it is not $02.

### Board timing (from the Defender port, 2026-10-20)

Found by comparing instruction traces in the Defender port (`../defender`,
same board family) and carried across; Joust's tests still pass, and 15
MAME frames of play with movement (1400-2400) match pixel for pixel:
- A write to PIA port B's data direction register also changes the pins.
- The sound CPU catches up to the main CPU before a command reaches it;
  the command lands as the main CPU's store instruction ends.
- The video counter gives the beam's line when the load's read lands, at
  the instruction's end (`M6809.op_end`), not at its start.
- Each line is drawn whole from RAM and the palette as they are 2 cycles
  before the line starts; a store or blit landing after that waits for it.
- `Sound.step_hook` traces the sound CPU.

### The game program in C (S0, started 2026-10-21)

Direct page is $A0 throughout, so `<$xx` is $A0xx. Ported routines are
named; `game` checks each (section 3).
- **Tasks** (`sim/tasks.c`): 40 blocks of $38 bytes at $A100-$A9BF, linked
  at power-on ($E091-$E0A3; the last block's link points at $A9C0, the
  first display list, not 0). The list runs from <$0A (the head, $A00A,
  sits where a block's link would); free blocks from <$08. A block: +0
  next, +2 type, +3 a flag that runs it in the second pass, +4 frames to
  sleep, +5-6 where it resumes, then its own bytes. <$2E the task running,
  <$2C the block before it, <$66 where the second pass resumes. $E284
  (via $E030; $E282 via $E009 after the current task) takes a free block,
  links it after U and sets +4 = 1, +5 = X, +2 = A, +3 = B. $E157 (via
  $E037) sleeps A frames, resuming after the call; $E20F (via $E00F) ends
  the current task; $E24D (via $E00C) ends every other task whose type &
  B equals A, moving <$66 off a block it frees.
- **The frame loop** ($E0E2): takes this frame's display list ($A9C0 or
  $AC21 by <$27), waits until the interrupt has drawn it, sends the queued
  sound when <$61 counts down to 0 (script pointer <$63, priority <$62,
  to the PIA at $C80E: $FF, then the command & $3F), clears the four layer
  heads at $A000, $A040, $A080 and $A0C0, then the passes: the first
  ($E163) counts every unflagged task down and runs those at zero; the
  second ($E180) the flagged ones from <$66 on, stopping when the byte at
  [<$68] is 0 (the interrupt has drawn the previous list) and saving its
  place; then the layers are chained into the new display list ($E197-
  $E20C).
- **Sound requests** ($E4CD via $E02D, X = a script): its first byte is
  the priority; one without bit 7 is dropped unless <$90 has bit 7; one
  below the queued sound's priority is dropped while it waits.
- **The ROM check** ($ECF8-$ED17, `sim/checksum.c`): a task adding every
  other byte of $0000-$7FFF, one a frame (+D-E the pointer, +F the sum),
  about 4.5 minutes a pass; a pass ending on any sum but $EEC1's byte
  calls $7A75 (not ported).
- **The lava** ($E4E6-$E5E6, `sim/lava.c`): at a game's start the surface
  fills a line every 20 frames, from line $FF up to <$9B (<$9C-<$9D the
  column and row), $44 pairs drawn in from each side of the screen until
  they meet something; then bubbles: a random column from the 16 at
  $E5E9, and by the random carry a bubble ($40, $04/$40, $44 back, 8
  frames apart, with the sound at $E692) or a dip (cleared, drawn back over
  three frames). It writes the screen with the ROM unselected (`$BFFF` is
  RAM's copy of $C900).
- **Collisions** ($837A-$8417, $DC21, `sim/collide.c`): two tasks walk
  the task list every frame. $837F pairs each task whose type has bit 7
  with every such task after it whose type shares bit 0; $83A4 counts <$A8
  and <$A9 down, then takes each task of type $89 against the riders after
  it. A pair goes through $83D8: the boxes first (+7 x, +$29 the right
  edge, 16-bit signed; +$27 and +$28 the top and bottom lines; the x
  difference to <$34, the other task to <$49), then the collision data
  ([+$1C]: rows of left and right edges as words, ending in a negative
  word) through $DC21, an unrolled loop that decides by the 6809's flags
  (overflow on `SUBD`) and returns a collision in the carry. A collision
  goes on to the handlers at $8418 (who wins, eggs, bounces): not ported,
  so a step meeting one is a path not ported (8 in the scripted game).
- **Scores** ($E785-$E80C, `sim/scores.c`): a task per score, +D its
  table in ROM ($E71D for player 1's): +0 the change flag ($A050), +2 the
  colours (the second blit's control and solid), then 6-byte entries from
  +4: the BCD byte ($A04C-$A04F), the screen address, the digit routine
  ($E7D4 high nibble, $E7D8 low); a negative address ends them. On a
  change ($E7A6, every 2 frames) the flag is cleared and the digits drawn
  from the first that is not a leading zero, one a frame ($E79B): a solid
  $EE blit over the 3 x 7 cell (control $12), then the glyph from
  [[$4A68]] + 2 + digit x $17 in the score's colour, interrupts masked.
  The C game blits on its own RAM (`game_blit`: gfx/williams.c's blitter,
  reading the ROM image).
- The scripted game (`game`) runs 79 task addresses; ported so far: the
  ROM check, the lava, the collision walks and the scores (16863 of 32206
  steps). The most frequent left: $8F3D and $8F04 (a rider's step: the
  controls at $D83D, then `[$30,U]`, $8E05, $D9A3), $65AE, $6364, $8D4D.

## 3. Test plan

- **Unit** (`ctest`, no ROM): scalers, presets, blitter, glow (`enhance`);
  the sim's pieces as they arrive.
- **ROM** (label `rom`): the loader (`rom_load`); later the extractor's
  output (as Defender's `extract`).
- **Task steps** (label `rom`): `game` keeps a C model of all of RAM
  (`sim/game.h`) on the board's scripted game: the board's writes (the
  CPU's and the blitter's) are copied to it, except during a ported task
  step, which runs in C when the ROM's dispatcher starts it ($E154); as the
  ROM's step writes each byte the model takes the C value, and when it
  yields ($E15F) the model, and the ROM select, must equal the board's.
  Steps an interrupt broke into must match with the interrupt's writes
  before or after the C step, or replayed where they fell among its
  writes; a race the C game cannot have (the ROM reading partly before the
  interrupt) is counted and must stay under 1 in 1000. Between steps the
  model must equal RAM every frame. The stack page $BE00-$BEFF is not
  compared. A planted bug in either ported task is caught at the byte.
  Next, as Defender's: checks of the interrupt's routines per call, and
  `frame` (the C game alone against the board).
- **Routines on chosen inputs** (label `rom`): `routines` runs a ROM
  routine on the emulated CPU (interrupts masked, direct page $A0) from
  chosen RAM and registers until it returns, and the C one on a model of
  the same RAM; results and all of RAM must match. So far $DC21 (20000
  random row lists, well formed and noise: 5384 overlap) and $83D8 (20000
  random pairs of task blocks: 1024 collide, where the ROM goes on to
  $8418).
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
| E1 | (done 2026-10-20) Emulated board: 6809 core, main board, sound board; the game plays from the player's ROM with sound | `mame_emu`: 20000 random-state changes and three frames equal MAME's; instruction traces of frames 600-900 matched cycle for cycle |
| E2 | Polish: HD presets drawing upscaled sprites from the board's blit list, settings persistence, web build with IndexedDB CMOS, device testing | Device checklist |
| P2-P5 | (superseded: the PRNG and kernel notes became S0's start) The first plan for a C game, from MAME alone | - |
| X1 | Extractor (`joust --extract`, as Defender's `src/assets/extract.c`): the images (the catalogue, decoded through the blitter), fonts, palette and sounds (WAVs recorded from the emulated sound board) to `assets/`, then the game's tables as the C game needs them | `extract`; the C game runs from `assets/` alone |
| S0 | (started 2026-10-21) The C game's foundation: a model of all of RAM (`sim/game.c`), the task kernel (`sim/tasks.c`), the `game` and `routines` harnesses; first tasks: the ROM check (`checksum.c`), the lava (`lava.c`), the collision walks and tests (`collide.c`) | `game`, `routines` |
| S1 | Task steps in C: the players (flap, gravity, running, braking, the joust's outcome), enemies (Bounders, Hunters, Shadow Lords), eggs and hatching, the pterodactyl, the troll's hand, platforms, waves and their tables, scoring, extra lives, two players | `game` on scripted games (and a second, longer one), `routines` |
| I1 | The interrupt in C: the display lists to the blitter (the C game draws with `gfx/williams.c`'s blitter into video RAM laid out as the arcade's), the random stir, the controls and coins | Each routine checked per call (as Defender's) |
| F1 | The frame in C (both passes, the sound queue, the layers) and `--c-game`: the C game alone, from power-on | `frame`: the C game against the board every frame |
| C3 | Attract mode, high-score table and entry, operator pages, sounds from the WAVs; the C game becomes the default, the release needs only `assets/` | Full scripted run matches; release with `assets/` only |
| P6 | Release: packages, PortMaster submission, GitHub release | Published |
