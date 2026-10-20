-- MAME probe: the main CPU's view of program ROM, as "ADDR BYTE" hex lines
-- (the format of `joust --dump-rom`). $D000-$FFFF is always ROM; $0000-$8FFF
-- is video RAM until bit 0 of the ROM select latch at $C900 is set, which the
-- probe sets first.
--
--   PROBE_OUT=out.txt mame joust -autoboot_script rom_map_probe.lua ...

local out_path = os.getenv("PROBE_OUT") or "rom_map_probe.txt"

local cpu = manager.machine.devices[":maincpu"]
local space = cpu.spaces["program"]
space:write_u8(0xC900, 1)

local out = assert(io.open(out_path, "w"))
local function dump(first, last)
    for addr = first, last do
        out:write(string.format("%04X %02X\n", addr, space:read_u8(addr)))
    end
end
dump(0x0000, 0x8FFF)
dump(0xD000, 0xFFFF)
out:close()
manager.machine:exit()
