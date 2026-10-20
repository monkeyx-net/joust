-- MAME probe: blitter operations, palette and video RAM, with scripted input.
-- Only dumps; C does the decoding and comparing.
--
--   PROBE_OUT=<dir> PROBE_FRAMES="600 1500" PROBE_LAST=1800 \
--   PROBE_SCRIPT="1200:coin 1260:start 1400:right,flap ..." mame joust -autoboot_script capture_probe.lua
--
-- Writes into PROBE_OUT:
--   blits.txt          one line per blit, hex: frame ctrl solid src dst w h romsel
--                      (w and h as written to $CA06/$CA07; the SC1 blitter XORs them with 4)
--   frame_<n>.txt      at each PROBE_FRAMES frame: "palette" + 16 bytes, then
--                      "vram <col>" + 256 bytes for each of the 192 two-pixel columns
--   and a MAME snapshot per PROBE_FRAMES frame (snap/joust/NNNN.png, in order)
--
-- The blitter's registers ($CA00-$CA07) are write-only, so the probe keeps
-- the last value written to each; a write to $CA00 starts a blit. $C900 bit 0
-- selects ROM under video RAM for reads (and blitter sources).

local out_dir = os.getenv("PROBE_OUT") or "."
local last_frame = tonumber(os.getenv("PROBE_LAST") or "1800")

local snap_frames = {}
for n in string.gmatch(os.getenv("PROBE_FRAMES") or "", "%d+") do snap_frames[tonumber(n)] = true end

-- "frame:input,input frame:..." where input is coin, start, start2, left, right, flap
-- (player 1); each entry holds until the next one.
local script = {}
for f, what in string.gmatch(os.getenv("PROBE_SCRIPT") or "", "(%d+):([%w,]*)") do
    table.insert(script, { frame = tonumber(f), inputs = what })
end

local fields = {}
for _, port in pairs(manager.machine.ioport.ports) do
    for name, field in pairs(port.fields) do fields[name] = field end
end
local NAMES = { coin = "Coin 1", start = "1 Player Start", start2 = "2 Players Start",
                left = "P1 Left", right = "P1 Right", flap = "P1 Button 1" }

local cpu = manager.machine.devices[":maincpu"]
local space = cpu.spaces["program"]
local vram = manager.machine.memory.shares[":videoram"]
local pal = manager.machine.memory.shares[":paletteram"]

local frame = 0
local regs = { 0, 0, 0, 0, 0, 0, 0, 0 }
local romsel = 0
local blits = assert(io.open(out_dir .. "/blits.txt", "w"))

-- Kept in globals: MAME removes a tap once its handle is garbage-collected.
_G.capture_blit_tap = space:install_write_tap(0xCA00, 0xCA07, "capture_blit", function(offset, data, mask)
    local r = offset - 0xCA00
    regs[r + 1] = data & 0xFF
    if r == 0 then
        blits:write(string.format("%d %02X %02X %04X %04X %02X %02X %d\n", frame, regs[1], regs[2],
            regs[3] << 8 | regs[4], regs[5] << 8 | regs[6], regs[7], regs[8], romsel))
    end
end)
_G.capture_romsel_tap = space:install_write_tap(0xC900, 0xC900, "capture_romsel", function(offset, data, mask)
    romsel = data & 1
end)

local function dump_frame(n)
    local f = assert(io.open(string.format("%s/frame_%d.txt", out_dir, n), "w"))
    local p = {}
    for i = 0, 15 do p[#p + 1] = string.format("%02X", pal:read_u8(i)) end
    f:write("palette " .. table.concat(p, " ") .. "\n")
    for col = 0, 191 do
        local row = {}
        for y = 0, 255 do row[#row + 1] = string.format("%02X", vram:read_u8(col * 256 + y)) end
        f:write(string.format("vram %d ", col) .. table.concat(row) .. "\n")
    end
    f:close()
end

local held = {}
_G.capture_frames = emu.add_machine_frame_notifier(function()
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
    if snap_frames[frame] then
        dump_frame(frame)
        manager.machine.video:snapshot()
    end
    if frame >= last_frame then
        blits:close()
        manager.machine:exit()
    end
end)
