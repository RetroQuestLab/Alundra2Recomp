-- Capture du heros au repos (aucune touche) apres stabilisation.
-- SESSION_OUT=... PS1_MAIN_ADDR=0x... pcsx-redux ... -dofile capture_idle.lua
local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x80011980')
local gameFrame = 0
local gameStarted = false
local bps = {}

local function shot(name)
	local ss = PCSX.GPU.takeScreenShot()
	local w, h = ss.width, ss.height
	if ss.data.size ~= w * h * 2 then return end
	local p = ffi.cast('const uint16_t*', ss.data.data)
	local px = {}
	for i = 0, w * h - 1 do
		local c = p[i]
		px[#px + 1] = string.char(bit.band(c, 31) * 8,
			bit.band(bit.rshift(c, 5), 31) * 8,
			bit.band(bit.rshift(c, 10), 31) * 8)
	end
	local file = io.open(outdir .. '/' .. name .. '.ppm', 'wb')
	file:write(string.format('P6\n%d %d\n255\n', w, h))
	file:write(table.concat(px))
	file:close()
	print(string.format('shot %s %dx%d at frame %d', name, w, h, gameFrame))
end

bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'capture_idle: main',
	function()
		gameStarted = true
		gameFrame = 0
		return false
	end,
	'capture_idle: main')

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not gameStarted then return end
	gameFrame = gameFrame + 1
	if gameFrame == 60 then shot('idle_60') end
	if gameFrame == 120 then shot('idle_120') end
	if gameFrame == 180 then shot('idle_180') end
	if gameFrame == 200 then PCSX.quit(0) end
end)

print('CAPTURE_IDLE ready, main=0x' .. string.format('%08x', mainAddr) .. ' outdir=' .. outdir)
