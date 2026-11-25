-- Courte sequence video PS1 (images natives) pour juger une animation.
-- Parcours joueur : titre -> START -> Nouvelle partie (CROIX) -> intro, puis
-- optionnellement START pour passer l'intro et un script de manette.
-- Env : SESSION_OUT, PS1_MAIN_ADDR, PS1_MENU_DRAW_ADDR, PS1_INTRO_DRAW_ADDR,
--       PS1_MENU_ADDR, SEQ_PHASE (intro|game), SEQ_DELAY (images apres le
--       debut de la phase), SEQ_COUNT, SEQ_STEP, SEQ_PAD (ex. "RIGHT:60,UP:40").
local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR'))
local menuDrawAddr = tonumber(os.getenv('PS1_MENU_DRAW_ADDR') or '0')
local introDrawAddr = tonumber(os.getenv('PS1_INTRO_DRAW_ADDR') or '0')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR'))
local seqPhase = os.getenv('SEQ_PHASE') or 'intro'
local seqDelay = tonumber(os.getenv('SEQ_DELAY') or '0')
local seqCount = tonumber(os.getenv('SEQ_COUNT') or '40')
local seqStep = tonumber(os.getenv('SEQ_STEP') or '4')
local seqPad = os.getenv('SEQ_PAD') or ''
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local mem = PCSX.getMemPtr()

local frame, started, phase, phaseFrame = 0, false, 'title', 0
local menuDraws, introDraws, shots = 0, 0, 0
local held = nil
local script, scriptIdx, scriptEnd = {}, 1, 0

for item in string.gmatch(seqPad, '[^,]+') do
	local name, n = string.match(item, '(%w+):(%d+)')
	if name then script[#script + 1] = { button = B[name], frames = tonumber(n) } end
end

local function read32(addr)
	local off = addr - 0x80000000
	return mem[off] + mem[off + 1] * 256 + mem[off + 2] * 65536 + mem[off + 3] * 16777216
end

local function shot(name)
	local ss = PCSX.GPU.takeScreenShot()
	local w, h = ss.width, ss.height
	if ss.data.size ~= w * h * 2 then return end
	local p = ffi.cast('const uint16_t*', ss.data.data)
	local px = {}
	for i = 0, w * h - 1 do
		local c = p[i]
		px[#px + 1] = string.char(bit.band(c, 31) * 8,
			bit.band(bit.rshift(c, 5), 31) * 8, bit.band(bit.rshift(c, 10), 31) * 8)
	end
	local f = io.open(outdir .. '/' .. name .. '.ppm', 'wb')
	f:write(string.format('P6\n%d %d\n255\n', w, h))
	f:write(table.concat(px))
	f:close()
end

local function press(button, frames)
	if held then pad.clearOverride(held) end
	held = button
	pad.setOverride(button)
	scriptEnd = frame + frames
end

bps = {}
local function addBp(addr, label, cb)
	if addr == 0 then return end
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, label, cb, label)
end
addBp(menuDrawAddr, 'seq: menu', function() menuDraws = menuDraws + 1 return true end)
addBp(introDrawAddr, 'seq: intro', function() introDraws = introDraws + 1 return true end)
addBp(mainAddr, 'seq: main', function() started = true frame = 0 return false end)

listener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not started then return end
	frame = frame + 1
	if held and frame >= scriptEnd then
		pad.clearOverride(held)
		held = nil
		if phase == 'record' and scriptIdx <= #script then
			press(script[scriptIdx].button, script[scriptIdx].frames)
			scriptIdx = scriptIdx + 1
		end
	end
	if phase == 'title' and menuDraws >= 2 then
		phase = 'menu'
		press(B.START, 14)
	elseif phase == 'menu' and read32(menuAddr) == 1 and not held then
		phase = 'intro'
		phaseFrame = frame
		press(B.CROSS, 14)
	elseif phase == 'intro' and introDraws >= 2 and not held then
		if seqPhase == 'intro' then
			phase = 'record'
			phaseFrame = frame
		elseif frame >= phaseFrame + 45 then
			phase = 'skip'
			phaseFrame = frame
			press(B.START, 14)
		end
	elseif phase == 'skip' and frame >= phaseFrame + 90 then
		phase = 'record'
		phaseFrame = frame
		if #script > 0 then
			press(script[1].button, script[1].frames)
			scriptIdx = 2
		end
	elseif phase == 'record' then
		local t = frame - phaseFrame - seqDelay
		if t >= 0 and t % seqStep == 0 then
			shot(string.format('seq_%03d', shots))
			shots = shots + 1
			if shots >= seqCount then
				print('SEQUENCE_CAPTURE: PASS frames=' .. shots)
				PCSX.quit(0)
			end
		end
	end
	if frame > 20000 then print('SEQUENCE_CAPTURE: FAIL timeout ' .. phase) PCSX.quit(1) end
end)
print('SEQUENCE ready phase=' .. seqPhase)
