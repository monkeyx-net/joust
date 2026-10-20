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

Confirmed in this repo:
- Program ROM: 12 x 4 KB. Banks 1-9 at $0000-$8FFF under video RAM, read
  when bit 0 of $C900 selects ROM; banks 10-12 at $D000-$FFFF. Reset vector
  $F000. (`mame_rom_map` checks all 48 KB against MAME.)
- Sound ROM 4 KB for the 6808 sound board.

To confirm in MAME as the work needs it (the Robotron port's notes are a
guide, not proof): the 292x240 visible area, 4 bpp video RAM layout, the
16-entry palette at $C000, the blitter at $CA00, input PIAs, CMOS, watchdog,
and the game's RAM map.

## 3. Test plan

- **Unit** (`ctest`, no ROM): scalers, presets, blitter, glow (`enhance`);
  the sim's pieces as they arrive.
- **ROM** (label `rom`): the loader (`rom_load`); later the extractor's
  output against hashes recorded when it matched MAME.
- **MAME** (label `mame`): `mame_rom_map` now; later per-frame RAM traces of
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
| P1 | Graphics from the ROM: find the image tables (ostriches, buzzards, knights, pterodactyl, eggs, platforms, lava, troll hand, font) and the palette; extractor writes `assets/` at first run | Extracted frames drawn by the port match MAME screenshots of the same scenes |
| P2 | Reverse engineering: RAM map, object lists, PRNG, flap and gravity physics, collisions and joust outcome, wave tables and spawn rules | Lua probes dump each documented variable; notes in this plan |
| P3 | Sim, wave 1: player, Bounders, eggs and hatching, platforms, scoring, death and respawn, deterministic from scripted input | Per-frame traces equal MAME's for scripted wave-1 games |
| P4 | Full game: all wave types (egg, survival, gladiator, pterodactyl), Hunters and Shadow Lords, troll hand, lava, crumbling platforms, extra lives, 2 players | Traces match MAME through wave 10 |
| P5 | Presentation: attract mode, high-score table and entry, operator menu (CMOS settings), sound (6808 + DAC emulation of the sound ROM, or player-side capture) | Device checklist on 3 handhelds + Steam Deck |
| P6 | Release: packages, PortMaster submission, GitHub release | Published |
