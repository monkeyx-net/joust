-- RE helper (not a test): MAME's disassembly of the main CPU's ROM, for
-- reading the game's code. Writes text files only, to DASM_OUT (default
-- build/re); never commit them, they hold the ROM's code.
--
--   DASM_OUT=build/re mame joust -debug -debugger none -video none -sound none \
--     -seconds_to_run 30 -autoboot_script src/tests/mame/dasm_probe.lua
--
-- Writes d000.asm ($D000-$FFFF) and rom0.asm ($0000-$8FFF, with ROM selected).
-- The debugger runs its commands after the frame callback returns, so the
-- probe exits some frames later.

local out = os.getenv("DASM_OUT") or "build/re"
local dbg = manager.machine.debugger
local frame = 0
_G.dasm_frames = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if frame == 5 then
        manager.machine.devices[":maincpu"].spaces["program"]:write_u8(0xC900, 1)
        dbg:command("dasm " .. out .. "/d000.asm,d000,3000")
        dbg:command("dasm " .. out .. "/rom0.asm,0,9000")
    elseif frame == 60 then
        manager.machine:exit()
    end
end)
