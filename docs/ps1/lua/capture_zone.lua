-- Capture native de la premiere zone jouable PS1 (zone 011).
-- Declenche par GPU::Vsync, jamais depuis un breakpoint.
-- SESSION_OUT=... PS1_MAIN_ADDR=0x... pcsx-redux ... -dofile capture_zone.lua

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x80011980')
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

local function press(...)
	local buttons = { ... }
	return function()
		for _, b in ipairs(buttons) do pad.setOverride(b) end
	end
end

local function release(...)
	local buttons = { ... }
	return function()
		for _, b in ipairs(buttons) do pad.clearOverride(b) end
	end
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

at(30, shot('zone_idle'))
at(40, press(B.RIGHT))
at(100, shot('zone_walk_right'))
at(101, release(B.RIGHT))
at(110, press(B.RIGHT, B.R1))
at(150, shot('zone_run_right'))
at(151, release(B.RIGHT, B.R1))
at(160, press(B.RIGHT, B.CROSS))
at(175, shot('zone_jump'))
at(176, release(B.RIGHT, B.CROSS))
at(185, press(B.SQUARE))
at(195, shot('zone_attack'))
at(196, release(B.SQUARE))
at(210, function() PCSX.quit(0) end)

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

print('CAPTURE_ZONE ready, main=0x' .. string.format('%08x', mainAddr) ..
	' outdir=' .. outdir)
