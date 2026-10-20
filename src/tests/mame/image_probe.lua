-- MAME probe: MAME's own blitter draws each catalogued image, in several
-- modes, over a known pattern; the probe dumps what it wrote. Only dumps;
-- src/tests/test_images.c runs the port's blitter the same way and compares.
--
--   PROBE_OUT=out.txt PROBE_IMAGES=list.txt PROBE_AT=600 mame joust -autoboot_script image_probe.lua
--
-- PROBE_IMAGES holds `joust --list-images` lines (ADDR:W:H, hex address).
-- For each image and each control byte in MODES the probe fills a scratch
-- area at $0000 (W+1 columns of H rows) with PATTERN, selects ROM, writes
-- $CA01-$CA07 then $CA00 (the blit runs during that write), reads the area
-- back and restores it. Output lines: "IMG addr ctrl <bytes column by column>".

local out_path = os.getenv("PROBE_OUT") or "image_probe.txt"
local at_frame = tonumber(os.getenv("PROBE_AT") or "600")
local MODES = { 0x02, 0x0A, 0x1A, 0x2A, 0xCA }
local PATTERN, SOLID = 0x5A, 0x37

local images = {}
for line in io.lines(os.getenv("PROBE_IMAGES") or "images.txt") do
    local a, w, h = line:match("^(%x+):(%d+):(%d+)")
    if a then table.insert(images, { addr = tonumber(a, 16), w = tonumber(w), h = tonumber(h) }) end
end

local space = manager.machine.devices[":maincpu"].spaces["program"]
local vram = manager.machine.memory.shares[":videoram"]
local romsel = 0
_G.image_romsel_tap = space:install_write_tap(0xC900, 0xC900, "image_romsel", function(offset, data, mask)
    romsel = data & 1
end)

local function run()
    local out = assert(io.open(out_path, "w"))
    for _, im in ipairs(images) do
        local cols = im.w + 1
        for _, ctrl in ipairs(MODES) do
            local saved = {}
            for c = 0, cols - 1 do
                for y = 0, im.h - 1 do
                    saved[c * 256 + y] = vram:read_u8(c * 256 + y)
                    vram:write_u8(c * 256 + y, PATTERN)
                end
            end
            local keep_romsel = romsel
            space:write_u8(0xC900, 1)
            space:write_u8(0xCA01, SOLID)
            space:write_u8(0xCA02, im.addr >> 8)
            space:write_u8(0xCA03, im.addr & 0xFF)
            space:write_u8(0xCA04, 0x00)
            space:write_u8(0xCA05, 0x00)
            space:write_u8(0xCA06, im.w ~ 4)
            space:write_u8(0xCA07, im.h ~ 4)
            space:write_u8(0xCA00, ctrl)
            local bytes = {}
            for c = 0, cols - 1 do
                for y = 0, im.h - 1 do bytes[#bytes + 1] = string.format("%02X", vram:read_u8(c * 256 + y)) end
            end
            out:write(string.format("IMG %04X %02X %s\n", im.addr, ctrl, table.concat(bytes)))
            for addr, v in pairs(saved) do vram:write_u8(addr, v) end
            space:write_u8(0xC900, keep_romsel)
        end
    end
    out:close()
    manager.machine:exit()
end

local frame = 0
_G.image_frames = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if frame == at_frame then run() end
end)
