-- Sonde SPU native : prouve que le pilote PS1 s'execute, que la banque VAG est
-- reellement presentee au SPU (reliture DMA) et que les evenements A2 jouent.
-- Variables d'environnement :
--   PS1_AUDIO_INIT_ADDR, PS1_AUDIO_MUSIC_ADDR, PS1_AUDIO_SFX_ADDR,
--   PS1_AUDIO_DIAG_ADDR, PS1_MAIN_ADDR, PS1_INTRO_ADDR, PS1_INV_ADDR,
--   PS1_MENU_DRAW_ADDR, PS1_INTRO_DRAW_ADDR, PS1_MENU_ADDR.
-- Les objets breakpoint restent dans une table GLOBALE (sinon le GC Lua les
-- collecte et les points d'arret disparaissent silencieusement).

local mainAddr  = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x80017070')
local initAddr  = tonumber(os.getenv('PS1_AUDIO_INIT_ADDR') or '0')
local musicAddr = tonumber(os.getenv('PS1_AUDIO_MUSIC_ADDR') or '0')
local sfxAddr   = tonumber(os.getenv('PS1_AUDIO_SFX_ADDR') or '0')
local diagAddr  = tonumber(os.getenv('PS1_AUDIO_DIAG_ADDR') or '0')
local introAddr = tonumber(os.getenv('PS1_INTRO_ADDR') or '0x800b0000')
local invAddr   = tonumber(os.getenv('PS1_INV_ADDR') or '0x800b0004')
local menuDrawAddr = tonumber(os.getenv('PS1_MENU_DRAW_ADDR') or '0')
local introDrawAddr = tonumber(os.getenv('PS1_INTRO_DRAW_ADDR') or '0')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR') or '0')

local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local mem = PCSX.getMemPtr()

local function read32(addr)
	local off = addr - 0x80000000
	return mem[off] + mem[off + 1] * 256 + mem[off + 2] * 65536 + mem[off + 3] * 16777216
end

local function introFrame() return read32(introAddr) end
local function invOpen() return bit.band(read32(invAddr), 255) ~= 0 end

local gameFrame = 0
local gameStarted = false
local menuDraws = 0
local introDraws = 0
local gamePhaseFrame = 0
local initHits = 0
local musicHits = 0
local sfxHits = 0
local sfxIds = {}
local seenId = {}
local active = nil
local phase = 'title'
local finished = false
bps = {}

local function addBp(addr, label, callback)
	if addr == 0 then return end
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, label, callback, label)
end

addBp(mainAddr, 'audio probe: main', function()
	gameStarted = true
	gameFrame = 0
	local r = PCSX.getRegisters()
	print(string.format('AUDIO_BP main pc=%08x gp=%08x', r.pc, r.GPR.n.gp))
	return true
end)

addBp(initAddr, 'audio probe: audioInitPS1', function()
	initHits = initHits + 1
	print('AUDIO_BP init')
	return true
end)

addBp(musicAddr, 'audio probe: audioPlayMusicPS1', function()
	musicHits = musicHits + 1
	local id = PCSX.getRegisters().GPR.n.a0
	print(string.format('AUDIO_BP music theme=%d', id))
	return true
end)

addBp(sfxAddr, 'audio probe: audioPlaySfxIdPS1', function()
	sfxHits = sfxHits + 1
	local id = PCSX.getRegisters().GPR.n.a0
	seenId[id] = true
	sfxIds[#sfxIds + 1] = id
	print(string.format('AUDIO_BP sfx id=%d', id))
	return true
end)

addBp(menuDrawAddr, 'audio probe: menuDrawPS1', function()
	menuDraws = menuDraws + 1
	return true
end)

addBp(introDrawAddr, 'audio probe: introDrawPS1', function()
	introDraws = introDraws + 1
	return true
end)

local function pulse(button, cb)
	pad.setOverride(button)
	active = { button = button, cb = cb, stage = 1,
		releaseAt = gameFrame + 12, doneAt = gameFrame + 24 }
end

local function waitFrames(frames, cb)
	active = { mode = 'wait', cb = cb, doneAt = gameFrame + frames }
end

local function waitOpen(cb)
	active = { mode = 'until', cb = cb, stage = 1,
		cond = function() return invOpen() end, releaseAt = gameFrame + 2 }
end

local function waitClosed(cb)
	active = { mode = 'until', cb = cb, stage = 1,
		cond = function() return not invOpen() end, releaseAt = gameFrame + 2 }
end

local function finish(label)
	if finished then return end
	finished = true
	local ready = read32(diagAddr)
	local count = read32(diagAddr + 4)
	local bytes = read32(diagAddr + 8)
	local verify = read32(diagAddr + 12)
	local lastId = read32(diagAddr + 16)
	local lastVoice = read32(diagAddr + 20)
	print(string.format('AUDIO_DIAG ready=%d count=%d bytes=%d verify=%d lastId=%d lastVoice=%d',
		ready, count, bytes, verify, lastId, lastVoice))

	local ok = ready == 1 and verify == 1 and bytes > 0 and
		initHits >= 1 and musicHits >= 1 and
		seenId[1] and seenId[63] and seenId[3]
	if ok then
		print('AUDIO_PROBE: PASS')
	else
		print('AUDIO_PROBE: FAIL ' .. label)
	end
	PCSX.quit(ok and 0 or 1)
end

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not gameStarted or finished then return end
	gameFrame = gameFrame + 1

	if active then
		if active.mode == 'wait' then
			if gameFrame >= active.doneAt then
				local cb = active.cb
				active = nil
				if cb then cb() end
			end
			return
		end
		if active.mode == 'until' then
			if active.stage == 1 and gameFrame >= active.releaseAt and active.cond() then
				active.stage = 2
				active.doneAt = gameFrame + 10
			elseif active.stage == 2 and gameFrame >= active.doneAt then
				local cb = active.cb
				active = nil
				if cb then cb() end
			end
			return
		end
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

	-- Entrees pilotees par l'etat observe : le pre-remplissage CD decale le
	-- demarrage du menu d'environ 120 images, les anciennes images fixes tiraient
	-- a cote. Le titre attend le premier rendu du menu, l'intro son premier rendu.
	if phase == 'title' and menuDraws >= 2 then
		phase = 'menu'
		pulse(B.START, function() end)
	elseif phase == 'menu' and read32(menuAddr) == 1 then
		phase = 'intro'
		pulse(B.CROSS, function() end)
	elseif phase == 'intro' and introDraws >= 2 then
		phase = 'game'
		gamePhaseFrame = 0
		pulse(B.START, function() end)  -- saute l'intro
	elseif phase == 'game' then
		if gamePhaseFrame == 0 then gamePhaseFrame = gameFrame end
		if gameFrame >= gamePhaseFrame + 90 then
			phase = 'open'
			pulse(B.START, function()
				waitOpen(function()
					pulse(B.CIRCLE, function()
						waitClosed(function() finish('sequence terminee') end)
					end)
				end)
			end)
		end
	elseif gameFrame >= 1500 then
		finish('timeout')
	end
end)

print(string.format('AUDIO_PROBE ready bps=%d init=%08x music=%08x sfx=%08x diag=%08x',
	#bps, initAddr, musicAddr, sfxAddr, diagAddr))
