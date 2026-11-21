-- Capture native PCSX-Redux de NOTRE programme (independante de TTY/AddSIO).
-- Le chronometre demarre a l'entree dans main : le meme script fonctionne en
-- -exe direct et en -iso apres boot CD. Les captures sont declenchees par
-- GPU::Vsync (jamais depuis un callback de breakpoint : takeScreenShot() y
-- ralentit fortement l'emulation, observe le 2026-11-21).
--
-- Les deux premieres images sont suffisamment tardives (frames 60 et 120)
-- pour que la boucle MIPS ait deja present plusieurs frames, y compris en
-- mode -interpreter : fond bleu, carre rouge, barre verte deja visibles.
--
-- Images PPM P6 16bpp -> PNG via tools/ppm_to_png.py.
--
-- Usage :
--   set SESSION_OUT=<dir>
--   pcsx-redux -testmode -stdout -lua_stdout -bios <openbios.bin> \
--     -exe <alundra2.exe> -run -debugger -interpreter -dofile capture_boot.lua
-- Remplacer -exe par -iso <alundra2_ps1.cue> (-fastboot) pour le boot CD.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x8001055c')
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local gameFrame = 0
local gameStarted = false
local schedule = {}
local bps = {}

local function at(f, action)
	schedule[f] = schedule[f] or {}
	table.insert(schedule[f], action)
end

local function shot(name)
	return function()
		local ss = PCSX.GPU.takeScreenShot()
		local w, h = ss.width, ss.height
		if ss.data.size ~= w * h * 2 then
			print('shot ' .. name .. ': not a 16bpp display, skipped')
			return
		end
		local p = ffi.cast('const uint16_t*', ss.data.data)
		local px = {}
		for i = 0, w * h - 1 do
			local c = p[i]
			local r = bit.band(c, 31) * 8
			local g = bit.band(bit.rshift(c, 5), 31) * 8
			local b = bit.band(bit.rshift(c, 10), 31) * 8
			px[#px + 1] = string.char(r, g, b)
		end
		local file = io.open(outdir .. '/' .. name .. '.ppm', 'wb')
		file:write(string.format('P6\n%d %d\n255\n', w, h))
		file:write(table.concat(px))
		file:close()
		print(string.format('shot %s %dx%d at frame %d', name, w, h, gameFrame))
	end
end

local function dumpRAM(name)
	return function()
		local file = io.open(outdir .. '/' .. name .. '.bin', 'wb')
		file:write(ffi.string(PCSX.getMemPtr(), 0x200000))
		file:close()
		print(string.format('ram %s at frame %d', name, gameFrame))
	end
end

at(60, shot('boot_frame60'))
at(60, dumpRAM('ram_frame60'))
at(120, shot('boot_frame120'))
at(120, dumpRAM('ram_frame120'))
-- Une pression de 8 frames au moins : le pad est lu une fois par pfPresent.
at(130, function() pad.setOverride(B.CROSS) end)
at(138, shot('boot_cross'))
at(139, function() pad.clearOverride(B.CROSS) end)
at(160, function() PCSX.quit(0) end)

-- Sonde unique : return false retire le breakpoint apres le premier hit.
-- L'objet est conserve dans bps pour eviter le GC (documentation officielle).
bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'capture: main reached',
	function()
		gameStarted = true
		gameFrame = 0
		local r = PCSX.getRegisters()
		print(string.format('CAPTURE main reached pc=%08x ra=%08x sp=%08x gp=%08x',
			r.pc, r.GPR.n.ra, r.GPR.n.sp, r.GPR.n.gp))
		return false
	end,
	'capture: main reached')

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not gameStarted then return end
	gameFrame = gameFrame + 1
	local actions = schedule[gameFrame]
	if actions then
		for _, action in ipairs(actions) do action() end
	end
end)

print('CAPTURE_BOOT ready, main=0x' .. string.format('%08x', mainAddr) ..
	' outdir=' .. outdir)
