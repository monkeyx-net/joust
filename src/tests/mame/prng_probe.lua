-- MAME probe: every change of the random state $A00D:$A00E from power-on,
-- with what made it. Only dumps; src/tests/test_prng.c replays it.
--
--   PROBE_OUT=prng.txt PROBE_COUNT=20000 PROBE_SCRIPT="1200:coin 1215: 1300:start 1315:" \
--   mame joust -autoboot_script prng_probe.lua
--
-- Both operations write $A00E last, so a write tap there sees each finished
-- change. Lines: "I hilo" (the state when the probe starts), then "S hilo"
-- (the IRQ's stir; the tap reports the next PC, $E399) or "R hilo" (a step
-- of $E4C0, next PC $E4CC), each with the state after it; "W hilo" for any
-- other writer (the power-on RAM test at $F0C7 and RAM clear at $E04F).

local out_path = os.getenv("PROBE_OUT") or "prng.txt"
local limit = tonumber(os.getenv("PROBE_COUNT") or "20000")

local script = {}
for f, what in string.gmatch(os.getenv("PROBE_SCRIPT") or "", "(%d+):([%w,]*)") do
    table.insert(script, { frame = tonumber(f), inputs = what })
end
local fields = {}
for _, port in pairs(manager.machine.ioport.ports) do
    for name, field in pairs(port.fields) do fields[name] = field end
end
local NAMES = { coin = "Coin 1", start = "1 Player Start", flap = "P1 Button 1", left = "P1 Left", right = "P1 Right" }

local cpu = manager.machine.devices[":maincpu"]
local space = cpu.spaces["program"]
local out = assert(io.open(out_path, "w"))
out:write(string.format("I %02X%02X\n", space:read_u8(0xA00D), space:read_u8(0xA00E)))
local count, other = 0, 0

_G.prng_tap = space:install_write_tap(0xA00E, 0xA00E, "prng", function(offset, data, mask)
    if count >= limit then return end
    local pc = cpu.state["PC"].value
    local kind = pc == 0xE399 and "S" or pc == 0xE4CC and "R" or "W"
    out:write(string.format("%s %02X%02X\n", kind, space:read_u8(0xA00D), data & 0xFF))
    count = count + 1
    if count == limit then
        out:close()
        manager.machine:exit()
    end
end)

local frame, held = 0, {}
_G.prng_frames = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    for _, s in ipairs(script) do
        if s.frame == frame then
            held = {}
            for what in string.gmatch(s.inputs, "%a+") do held[what] = true end
        end
    end
    for what, name in pairs(NAMES) do
        if fields[name] then fields[name]:set_value(held[what] and 1 or 0) end
    end
end)
