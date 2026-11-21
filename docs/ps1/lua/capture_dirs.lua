-- Capture native des 4 directions du heros apres correction du mapping A1SP.
-- SESSION_OUT=... PS1_MAIN_ADDR=0x... pcsx-redux ... -dofile capture_dirs.lua
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
		if ss.data.size ~= w * h * 2 then return end
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

-- Une direction a la fois, assez longtemps pour que l'animation s'etablisse.
at(30,  press(B.RIGHT))
at(90,  shot('dir_right'))
at(91,  release(B.RIGHT))
at(100, press(B.LEFT))
at(160, shot('dir_left'))
at(161, release(B.LEFT))
at(170, press(B.UP))
at(230, shot('dir_up'))
at(231, release(B.UP))
at(240, press(B.DOWN))
at(300, shot('dir_down'))
at(301, release(B.DOWN))
at(320, function() PCSX.quit(0) end)

bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'capture_dirs: main reached',
	function()
		gameStarted = true
		gameFrame = 0
		return false
	end,
	'capture_dirs: main reached')

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not gameStarted then return end
	gameFrame = gameFrame + 1
	local actions = schedule[gameFrame]
	if actions then
		for _, action in ipairs(actions) do action() end
	end
end)

print('CAPTURE_DIRS ready, main=0x' .. string.format('%08x', mainAddr) .. ' outdir=' .. outdir)
