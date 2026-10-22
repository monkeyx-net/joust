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
  (overflow on `SUBD`) and returns a collision in the carry. The outcome
  ($8418): both types without bit 0 go to $7535 (or a `SWI`); with bit 2
  (the pterodactyl's kind) a bounce ($8487, the sound $EF9D: by their
  boxes, $85D7, against a type $97, else by their lines, $84B7); with bit
  3 straight to the lances' lines ($8466); else level lances (within 3, or
  2 for a low frame, of +$26 + line) facing each other where U faces X
  unhorse X ($850B). At $8466 the lance lines (+$26 + the 16-bit line):
  equal is a tie ($847D: the sound $EFCE, both bounced, $852D: up/down by
  +$23 and the velocities, apart by <$34's sign with turned speeds and
  pushes of half their speed, facings away; the walk moves to the next
  pair), the lower one unhorsed ($84C6 U, $850B X; $84E0: the type's bit
  7 off, +$1A cleared, the brain $EEC2 (its mount flies off), the loser's
  table's +$10 routine, then its +$16 scores the winner with the table's
  +$2F points; then they bounce, or come off a type $97 by their boxes).
  $74C3 lays an enemy's egg (the sound $EF97, <$8E less its +$34, <$A1
  down; a task $7611 of type $82 after it: 8 lines up, 4 right, its
  velocity and speed, its table one rank up below $89E3 (+$31), +$33 from
  its own less 1, and at 0 the winner catches it at once). A player's
  death ($8279 player 1, $827F player 2, `sim/death.c`): the egg count
  (<$36/<$37) cleared, the deaths (<$A5/<$A6) counted, <$AC = $25; killed
  by the other player, the gladiator bounty when that player's count
  ([table+$18]: <$A3/<$A4) comes up to 0: 3000 points, a task showing
  them ($7861) and the sound $EF80 (an enemy's kill with <$90 and <$A0
  set <$B0 = 2); then the death's task ($8139, type $20), 50 points, the
  ROM check $85C1 (a sum of $22 bytes from [$8636] must be $32, else the
  free list is corrupted), the sound $EFB7, the player out of <$73-<$76,
  a life gone (+6); the last with <$90 set: the other player on ($834B,
  type 9) if <$52 | <$5C, else game over ($61F0, type $34, after the
  list's first task). $753B (also $7535, types without bit 0 in
  common): an egg collected (the sound $EF86, the collector's egg count,
  [+D]+$12, up to 4; points by it from $75A1: 250, 500, 750, 1000, plus
  500 caught in the air; one enemy fewer, <$AA; its waiting mount sent
  off; the egg's task on to $77AC in 4 frames to show its points).
- **Scores** (`sim/score.c`, $E6A8-$E71C, $8719): a table's +4 points at
  the score block (player 1's $A04C: +0-3 BCD, +4 the change flag, +5-6
  the lives, +7-9 the next extra life), 0 for the enemies'. $E6A8 adds A
  (BCD, `DAA`) at +2 and carries up (a carry out of the top adds $10);
  $E6B1 adds A's high nibble at +3 and its low nibble with the carry at
  +2. While the interval <$97-<$98 (from the CMOS) is set and +1 + +7
  (BCD) : +2 reaches +8-9: a life ($8719: +5 and +6 up, kept from
  wrapping; the icon blitted from the table's +8 to its +6 + 3 x lives -
  3, while lives are 5 or fewer), the sound $EF67, CMOS audit counter 5
  ($4110: 3 BCD bytes as 6 nibbles at $CCFC + 6n) up, and +8-9 raised by
  the interval (+$80 at +7 and +8 when +8 passes $30). The C game keeps
  the CMOS (`Game.cmos`) and a faithful `DAA` (`game_daa`).
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
- **The display list** (`sim/display.c`): 10-byte nodes [control][solid]
  [source][destination][width][height][next], the blitter's registers in
  order (width and height XOR 4), taken one after another from <$2A. Four
  bands of 64 lines, each with two chains whose tails ($A004 and $A006, +
  $40 a band) point at the last node's next field; a node is linked there,
  so the interrupt draws a band's nodes as the beam leaves it. $E2A1 /
  $E2B8 (via $E01B / $E01E): a node on chain +4 / +6 for line B; $E2CF /
  $E2F9 ($E021 / $E024): for the object at U (+B its line, +7 its x), <$31
  the line, <$30 the column (x / 2), control $1A (solid 0) or $0A, | $20
  for an odd x (shifted); $E322 / $E358 ($E027 / $E02A): two such nodes.
- **Controls** ($6BA6): both players' through PIA 0's multiplexer: CB2
  high (`$C807` | 8) shows player 1's (A = $C804 & $37, with the start
  buttons), low player 2's (B = $C804 & $07). The C game keeps the
  controls, $C807 and the multiplexer (`Game.in0`, `inp1`, `inp2`,
  `pia0_crb`, `mux`).
- **Flow** (`sim/flow.c`, the frequent paths): the start buttons' task
  ($6359 kills tasks of type $48, then $6364 every frame: <$B3 a coin to
  show, then the start buttons at $C804; a coin or a start, $6368 and
  $6380, not ported: credits in CMOS at <$F2 and $CC06, the $3Bxx CMOS
  helpers); a countdown ($62AD: +D-E frames, <$11 counted up each, then
  +$10 frames more watching both players' controls, reset to $3C while
  player 1 holds right and player 2 left; its end at $62CE not ported);
  idle paths: $65AE (the platforms met, <$4B, every 2 frames), $62DA/
  $62DF (the same controls every 5 frames), $8B0E/$8B1A (a rider waiting
  its turn to appear: a number from <$3A into +$16, then <$38 = <$39 and
  <$3B its number; type 9 goes to $8ABA).
- **Riders** (`sim/rider.c`): a task each; type $89 player 1, $8F the
  enemies, $0F/$09 riders the collision walks skip. The block: +7-8 x,
  +A-B the line (8.8), +D a table in ROM ($88EE player 1's, $8981-$89E3
  the enemies'), +F facing ($FF left), +$11-12 vertical velocity, +$13
  frames since a flap, +$14 the speed index (-8..8), +$15 a flap's frames,
  +$16 x's fraction, +$17 the frame, +$18/+$1A frame tables (+$1A the
  rider, 0 none), +$1C/+$1E the pictures, +$20 the run animation, +$22 a
  push, +$23 the ceiling hit, +$24 the brain ($DDB2/$DDB9 the players'
  controls: CB2 set, $C804 read, <$32 the direction -1/0/1 and <$33 the
  flap; an enemy's is a coroutine address in $7A83-$80A5), +$26-29 the box,
  +$30 the motion ($D858 in the air). In the air ($8F04 flapping, $8F3D
  gliding): $D83D (brain, facing, +$13 counted to $FF, erase nodes $8E8E),
  a new flap $D813 (lift ((+$13 x $60) >> 8) - $60, the direction twice
  into the speed index, clamped to ±8), $D858 (gravity <$48 into the
  velocity, the 8.8 line, a bounce under line $20, x by the speed table at
  $DCD5 with the fraction, $DDD2: a push spent 3 a frame, x wrapping at
  $124 and -10), $8E05 (frame $48 or $60, box, pictures), $D9A3 (the
  platform maps: RAM $B002 + x against ROM $EA97 + line, then $AEA2 + x
  against $E837 + line; neither: keep flying), $8E4D (draw nodes), a sleep
  of [[+D]+$1A] frames. Landing ($8F87): the run animation by the speed
  (the table at $8FB5), at rest, frame $0C, then standing ($8DEF: a box 13
  lines tall, the draw nodes, a sleep; $8D4D resumes). On the ground
  ($8D4D): the brain; a fresh flap takes off ($8EA4: velocity $FF80, up a
  line, then a flap's sound and frames); else +$13 keeps the flap, the
  direction (turned by the facing) steps the run animation (+$20: records
  [frames][routine][..][next by direction at +4..][speed at +6]; $DD2C the
  skid, with its sound), and when its frames run out the record's routine
  gives the frame and dx: $DD3A/$DD3C stand (or turn), $DD4D the run cycle
  ($DD43's frames and dx, a footstep sound from the table's +$24/+$26 every
  4 frames), $DD8E, $DD9C the skid (its sound once); x moves; no ground
  under it ($B002 + x against $EA98 + line) falls off ($8EC8: into the air,
  flapping or gliding); a wall ($AEA8 + x against $E837 + line) bumps it
  (+$22 = |push| + 3, facing cleared, <$4B). Brains: the players' ($DDB2,
  $DDB9) and $EEC2/$EF12, which flies to a line ($45, $81 or $D0 by where
  it is, a flap when below it and not rising, alternate frames) and off
  the screen's side, where it sleeps 60 frames in the brain (the task
  resumes at $EF08: not ported). The lava ($D899) and the enemies' other
  brains are not ported yet.
- **The wave** (`sim/wave.c`, its manager's frequent path): $6D91 every 8
  frames while enemies are left (<$AA): <$AC frames held, then <$AD-AE
  counted down to the pterodactyl ($68A8, at most 3 a wave by <$AB; not
  ported; the next wait from $6DE2 by +$1B); while <$A1 is set, every $70
  looks (+B) one more brain may run at once (<$8F up, kept from wrapping,
  when <$90). No enemies left: the wave's end ($6BDC, not ported).
- **The spawn** (`sim/spawn.c`, $87B0-$883D, $8B2D-$8D22): four pads, the
  records at $8A5D-$8A75 (8 bytes: [a word holding the pad's node
  template's address][x][line][its use count, <$6E-<$71]). $87B0: the use
  counts to <$6A-<$6D, then every rider (type bit 7; $82 only with +$2B
  set) whose x some frames ahead (the table at the given table's first
  word by its speed index, wrapped) lies in a pad's zone for its line (the
  7-byte zones after it: [outside?][low][high][the pad's flag]) marks that
  pad busy. $8B2D: two random bits pick a pad, the next free one taken in
  turn (none: a frame's wait at $8B15); its count up, the rider on it
  (+$20 the record), <$39 or <$3B up (type 9 or not), the table's +$2C
  sound, +$10 = $1E frames, +$1A the rider's frames ([[+D]+$A]). Each
  frame ($8BAF) the pad ($65EE: a fixed picture's node on chain +6 from
  its template) in the solid colour of the table's +$2E; from frame $14
  the rider's nodes grown down in that colour ($8BCB-$8C15) and ($8C1C)
  the mount's node a solid erase down the pad's column. On the pad ($8C56)
  the brain; the rows shimmer through the colours at the table's +$C
  ($8CA5; +$26 the colour, +$15 its pace, +$16 set once +$35 runs low)
  until the controls move it (the table's +$2A sound) or +$35 halves to
  0; then the pad plain again, its count down, the table's +$14 routine
  (players: $8267 sets <$73 or <$75 and <$A8/<$A9 from <$A7; enemies'
  $8A7D returns), and onto the ground ($8CF1, then $8D50). The attract
  mode's demo rider (type 9, brain $61DA) is not ported.
- **Eggs** (`sim/eggs.c`): an egg's task in flight ($7611 drawn, $7619
  every <$B0 frames: erased ($6AF3), moved ($75B0: gravity <$48, the
  ceiling under line $20, x by the speed table and $DDD2, then wrapped at
  $120 and 4, the box +$27-29), the platforms beside it ($779B into <$4B;
  the four-corner maps $AEA2/$AE98 + x against $E83D/$E844 + line, then
  $DA3E's bounce), landing ($D9C2 on the three-column top map: the speed
  index slowed by 2; a falling egg bounces up at a quarter of its speed
  until it is slower than $FFE0 and still), or into the lava at line $E3
  (<$AA one enemy fewer, the task ends). At rest ($76A7 every 12 frames:
  pushed off, it falls again) +$2D counts down from $B1B7 to hatching
  when <$A1 is below <$A2 ($76D0: <$A1 up, the sound $EF89, the knight's
  task $8EAF; not ported). The picture ($78E3): by the speed index's sign
  the frames at $791E/$7921, one by vertical speed (rising, falling past
  $80, or between), offset into the records at [$0028], clipped at line
  $E0 ($D8F4).
- **A secret code** ($6B20-$6BA5, in `sim/flow.c`): during a game a task
  watches both players' controls ($6BA6) for $2502, then $1204, then
  $0506 (player 1 left+flap with start 1 and player 2 right; start 2 with
  player 1 right and player 2 flap; player 1 left+flap and player 2
  right+flap), each held until it changes, the next looked for 15 frames
  later; a wrong one starts over after 30 frames. $6B90 keeps the wanted
  controls in +D and the continuation in +F, looking every frame at
  $6B9B. At the end ($6B4C, not ported) a message, then `JMP [$FFFE]`:
  the machine restarts. `game_runner` enters it at frame 5000.
- **The enemies' brains** ($7A83-$80D5, `sim/brains.c`): state machines,
  the state's address at +$24; each state sets <$32-<$33 and usually the
  next state, and returns. Three families ($7A83 on, $7C95 on, $7ECF on),
  each starting over at its first state; shared exits ($7BD3, $7E25,
  $802F: the direction by the facing, a push at +$22 taking it over;
  $7C7D/$7C87: flapping above the lava, below line $D3 or $D0). They use
  the wave's settings ($B1B4-$B1FF: frames between looks, line gaps,
  velocities, countdowns), +$2B-2C a remembered line or velocity sum, +$2D
  a countdown, +$2E/+$2F a turn when the target's speed index matches,
  +$32 frames to the next look, the platform maps (an edge ahead: $AEA2 +
  x against $E829 + line; a wall where the rider will be in 8 frames:
  $AE83/$AEC1 + x against $E837 + line + (velocity << 3) >> 8), and $80D6
  (the nearer player: <$73, <$75, <$A8/<$A9 for who is out; nearness by
  -|line difference|, or by x when within 256, the last comparison by the
  second player's x byte against the first's measure). $7AD5 (<$8E below
  <$8F: one more brain running, +$34) and $7C90 jump to the table's +2.
  Not ported: $793C (a riderless mount walking to its knight, $7A2F
  picking it up).
- **Platforms** ($D9A3-$DC20, `sim/platform.c`): a platform's top under a
  rider ($B002 + x against $EA97 + line) lands it ($D9C2: its line set to
  $44, $50, $80, $89, $A2 or $D2 by the bits; the lava's line, $D9FC, is
  where the troll's hand may start, not ported); one beside or above it
  ($AEA2 + x against $E837 + line, $DA3E) is tested with the platform's
  collision data ([[$0000]]-[[$000E]] by the bits) through $DBF9 (<$34 the
  x difference from the given column, the lists offset by the lines
  between the rider's bottom and the given line, then $DC21, whose end
  rows are then looked at): from above (velocity complemented and halved,
  +$23 $FE, +$14 set to ±2 if it was 0), from below (a rising velocity
  reversed and halved, +$23 2), or off a side (+$14 turned, the push +$22
  of ∓6 more, the velocity at most $FFC0 up); each with the sound at
  $EF8F; <$4B collects the bits met.
- The scripted game (`game`) runs 79 task addresses; ported so far: the
  ROM check, the lava, the collision walks, the scores, the flow's
  frequent paths and the riders whose brains are ported, in the air and
  on the ground (29799 of 32206 steps run in ported tasks, 28089 all the
  way in C; `game_runner` 55524; `game_two` 56499). Left: $793C, the
  spawn sequence ($8B2D, $8C1C/$8C56:
  materialising on a pad), the lava's troll ($D9FC, $69DD), eggs and
  hatching, the pterodactyl, a collision's handlers ($8418), scoring,
  death and the waves, and the attract mode ($5F91-$5FA5, $D0FD, $D1E7,
  $6925, $62DF's end). The most frequent left: the
  enemies' brains ($8F3D/$8F04 with types $8F), $65AE (<$4B's bits,
  drawing through $65EE), $8D4D (on the ground), $6D91, $7619.

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
  interrupt) is counted and must stay under 1 in 1000: bytes that differ
  were written by the interrupt, or the C step read one it wrote. The
  races seen so far are all the interrupt breaking into the random step
  at $E4CA, between `ROR <$0D` and `ROR <$0E`, so its stir lands half-way
  through the generator's update (3 in `game_runner`'s 57872 steps).
  `game_runner` is a second, longer game (12000 frames: running on the
  ground, a flap now and then, idle to the game's end), `game_two` a
  two-player game (9000 frames). Between steps the
  model must equal RAM every frame. The stack page $BE00-$BEFF is not
  compared. A planted bug in either ported task is caught at the byte.
  Next, as Defender's: checks of the interrupt's routines per call, and
  `frame` (the C game alone against the board).
- **Routines on chosen inputs** (label `rom`): `routines` runs a ROM
  routine on the emulated CPU (interrupts masked, direct page $A0) from
  chosen RAM and registers until it returns, and the C one on a model of
  the same RAM; results and all of RAM must match. So far $DC21 (20000
  random row lists, well formed and noise: 5384 overlap) and $83D8 (20000
  random pairs of task blocks, a third with level lances, through the
  outcome, on a game in progress with the riders' real tables: about 530
  ties, 270 box bounces, 416 unhorsings with their eggs, 115 eggs
  collected; it found the facing test reversed, which no scripted game had
  reached), scoring ($E6A8/$E6B1 on random BCD scores, points and
  thresholds: 14572 extra lives; it catches a `DAA` bug the games miss),
  the display list's six node entries (20000 random chains, free
  pointers and objects: the node returned and all of RAM), $D9A3 on a
  game in progress with the player at random x, line, velocity, speed and
  push (20000: 2192 land, 2236 bounce; $DC21's end rows checked too),
  $80D6 on random players near each other or not, missing or out (20000:
  X and the Z flag).
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
