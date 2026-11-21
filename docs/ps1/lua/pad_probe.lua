-- Sonde brute de la manette : lit g_pad_buf[0] directement en RAM emulee.
-- Prouve la polarite des bits btn (actif a l'etat bas sur PS1) et verifie que
-- pfPresent() convertit correctement en masque PF_BTN_* actif en haut.
--
-- Usage : pcsx-redux -testmode -stdout -lua_stdout -bios <openbios.bin> \
--   -exe <alundra2.exe> -fastboot -run -debugger -interpreter \
--   -dofile docs/ps1/lua/pad_probe.lua
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local mem = PCSX.getMemPtr()
local bufAddr = tonumber(os.getenv('PS1_PADBUF_ADDR') or '0x80079174')
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x80011980')
local started = false
local frame = 0
local bps = {}

local function raw()
	local b0 = mem[bufAddr - 0x80000000 + 0]
	local b1 = mem[bufAddr - 0x80000000 + 1]
	local lo = mem[bufAddr - 0x80000000 + 2]
	local hi = mem[bufAddr - 0x80000000 + 3]
	return string.format('stat=%02x hdr=%02x btnbytes=%02x %02x btn=%04x',
		b0, b1, lo, hi, lo + hi * 256)
end

bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'pad probe: main',
	function()
		started = true
		return false
	end, 'pad probe: main')

local function report(label)
	print(string.format('PAD_PROBE %-12s frame=%d %s', label, frame, raw()))
end

local function press(...)
	for _, b in ipairs({ ... }) do pad.setOverride(b) end
end
local function release(...)
	for _, b in ipairs({ ... }) do pad.clearOverride(b) end
end

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not started then return end
	frame = frame + 1
	if frame == 20 then report('idle') end
	if frame == 30 then press(B.RIGHT); report('right_held') end
	if frame == 40 then report('right_2') end
	if frame == 50 then release(B.RIGHT); press(B.CROSS); report('cross_held') end
	if frame == 60 then release(B.CROSS); press(B.SQUARE); report('square_held') end
	if frame == 70 then release(B.SQUARE); press(B.R1); report('r1_held') end
	if frame == 80 then release(B.R1); report('idle_again'); PCSX.quit(0) end
end)

print('PAD_PROBE ready main=0x' .. string.format('%08x', mainAddr)
	.. ' buf=0x' .. string.format('%08x', bufAddr))
