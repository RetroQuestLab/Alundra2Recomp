-- Capture native du dialogue Prunewell pour la comparaison PC <-> PS1.
-- Derive de capture_interaction.lua : la version actuelle n'ouvre plus le
-- dialogue automatiquement apres l'intro, il faut approcher le PNJ et appuyer
-- sur CROIX. Les attentes de 70 frames garantissent un typewriter termine.
-- SESSION_OUT, PS1_MAIN_ADDR, PS1_INTRO_ADDR, PS1_HERO_X_ADDR,
-- PS1_HERO_Y_ADDR, PS1_QUEST_ADDR, PS1_DLG_SHOWN_ADDR, PS1_DLG_PANEL_ADDR.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x80016fe0')
local introAddr = tonumber(os.getenv('PS1_INTRO_ADDR') or '0x80108e34')
local heroXAddr = tonumber(os.getenv('PS1_HERO_X_ADDR') or '0x800e8388')
local heroYAddr = tonumber(os.getenv('PS1_HERO_Y_ADDR') or '0x800e838c')
local questAddr = tonumber(os.getenv('PS1_QUEST_ADDR') or '0x80108e00')
local shownAddr = tonumber(os.getenv('PS1_DLG_SHOWN_ADDR') or '0x800e8380')
local panelAddr = tonumber(os.getenv('PS1_DLG_PANEL_ADDR') or '0x800e8384')
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local mem = PCSX.getMemPtr()
local gameFrame = 0
local gameStarted = false
local phase = 'title'
local active = nil

local function read32(addr)
	local off = addr - 0x80000000
	return mem[off] + mem[off + 1] * 256 + mem[off + 2] * 65536 + mem[off + 3] * 16777216
end

local function heroX() return math.floor(read32(heroXAddr) / 65536) end
local function heroY() return math.floor(read32(heroYAddr) / 65536) end
local function questStage() return read32(questAddr) end
local function introFrame() return read32(introAddr) end
local function dlgShown() return read32(shownAddr) end
local function dlgPanel() return read32(panelAddr) end

local function shot(name)
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
	print(string.format('shot %s %dx%d frame=%d hero=(%d,%d) stage=%d panel=%d shown=%d',
		name, w, h, gameFrame, heroX(), heroY(), questStage(), dlgPanel(), dlgShown()))
end

local function pulse(button, cb)
	pad.setOverride(button)
	active = { button = button, mode = 'pulse', cb = cb, stage = 1,
		releaseAt = gameFrame + 14, doneAt = gameFrame + 28 }
end

local function holdUntil(button, cond, cb)
	pad.setOverride(button)
	active = { button = button, mode = 'until', cond = cond, cb = cb, stage = 1,
		releaseAt = gameFrame + 2, doneAt = 0 }
end

local function waitFrames(frames, cb)
	active = { button = nil, mode = 'wait', cb = cb, doneAt = gameFrame + frames }
end

local function fail(message)
	print('DLG_PARITE_CAPTURE: FAIL ' .. message)
	PCSX.quit(1)
end

local function advanceTo(target, cb)
	if dlgPanel() >= target then cb() return end
	pulse(B.START, function() waitFrames(6, function() advanceTo(target, cb) end) end)
end

local function capturePrunewell()
	shot('dlg_avant_parole')
	pulse(B.CROSS, function()
		if dlgPanel() ~= 0 then fail('dialogue non ouvert, panel=' .. dlgPanel()) return end
		waitFrames(70, function()
			shot('dlg_panel1_full')
			advanceTo(2, function()
				waitFrames(70, function()
					if dlgPanel() ~= 2 then fail('panneau 3 inattendu, panel=' .. dlgPanel()) return end
					shot('dlg_panel3_full')
					print(string.format('DLG_PARITE_CAPTURE: PASS panel=%d shown=%d frame=%d',
						dlgPanel(), dlgShown(), gameFrame))
					PCSX.quit(0)
				end)
			end)
		end)
	end)
end

PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'capture dialogue parite: main reached',
	function()
		gameStarted = true
		gameFrame = 0
		local r = PCSX.getRegisters()
		print(string.format('CAPTURE main reached pc=%08x ra=%08x sp=%08x gp=%08x',
			r.pc, r.GPR.n.ra, r.GPR.n.sp, r.GPR.n.gp))
		return false
	end,
	'capture dialogue parite: main reached')

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not gameStarted then return end
	gameFrame = gameFrame + 1

	if active and gameFrame % 120 == 0 then
		print(string.format('WAIT phase=%s mode=%s hero=(%d,%d) stage=%d panel=%d shown=%d',
			phase, active.mode, heroX(), heroY(), questStage(), dlgPanel(), dlgShown()))
	end

	if active then
		if active.mode == 'wait' then
			if gameFrame >= active.doneAt then
				local cb = active.cb
				active = nil
				if cb then cb() end
			end
			return
		end
		if active.mode == 'pulse' then
			if active.stage == 1 and gameFrame >= active.releaseAt then
				pad.clearOverride(active.button)
				active.stage = 2
			elseif active.stage == 2 and gameFrame >= active.doneAt then
				local cb = active.cb
				active = nil
				if cb then cb() end
			end
			return
		end
		if active.stage == 1 and gameFrame >= active.releaseAt and active.cond() then
			pad.clearOverride(active.button)
			active.stage = 2
			active.doneAt = gameFrame + 16
		elseif active.stage == 2 and gameFrame >= active.doneAt then
			local cb = active.cb
			active = nil
			if cb then cb() end
		end
		return
	end

	if phase == 'title' and gameFrame >= 60 then
		phase = 'menu'
		pulse(B.START, function() end)
	elseif phase == 'menu' and gameFrame >= 110 then
		phase = 'intro'
		pulse(B.CROSS, function() end)
	elseif phase == 'intro' and introFrame() >= 30 then
		phase = 'parole'
		pulse(B.START, function()
			holdUntil(B.RIGHT, function() return heroX() >= 730 end, capturePrunewell)
		end)
	end

	if gameFrame > 20000 then
		fail('timeout phase=' .. phase .. ' hero=(' .. heroX() .. ',' .. heroY() ..
			') stage=' .. questStage())
	end
end)

print('DLG_PARITE_CAPTURE ready, main=0x' .. string.format('%08x', mainAddr) ..
	' intro=0x' .. string.format('%08x', introAddr) .. ' outdir=' .. outdir)
