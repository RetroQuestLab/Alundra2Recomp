-- Sonde de mixage PS1 : prouve sur l'ISO livree que les fondus PC (descente a
-- zero, swap, remontee) s'executent, que les bruitages restent actifs pendant
-- le streaming CD et que la musique est duckee sous ces bruitages.
--
-- Variables : PS1_MAIN_ADDR, PS1_MIX_DIAG_ADDR, PS1_STREAM_DIAG_ADDR,
--             PS1_AUDIO_DIAG_ADDR, PS1_AUDIO_SFX_ADDR, PS1_STREAM_ADDR,
--             PS1_MENU_DRAW_ADDR, PS1_INTRO_DRAW_ADDR, PS1_MENU_ADDR.
-- Les breakpoints restent dans une table GLOBALE (piege GC PCSX Lua).
-- navigation pilotee par l'etat observe, jamais par un nombre d'images fixe.

local mainAddr     = tonumber(os.getenv('PS1_MAIN_ADDR') or '0')
local mixAddr      = tonumber(os.getenv('PS1_MIX_DIAG_ADDR') or '0')
local streamDiag   = tonumber(os.getenv('PS1_STREAM_DIAG_ADDR') or '0')
local audioDiag    = tonumber(os.getenv('PS1_AUDIO_DIAG_ADDR') or '0')
local sfxAddr      = tonumber(os.getenv('PS1_AUDIO_SFX_ADDR') or '0')
local streamAddr   = tonumber(os.getenv('PS1_STREAM_ADDR') or '0')
local menuDrawAddr = tonumber(os.getenv('PS1_MENU_DRAW_ADDR') or '0')
local introDrawAddr= tonumber(os.getenv('PS1_INTRO_DRAW_ADDR') or '0')
local menuAddr     = tonumber(os.getenv('PS1_MENU_ADDR') or '0')

local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local mem = PCSX.getMemPtr()

local function read32(addr)
	local off = addr - 0x80000000
	return mem[off] + mem[off + 1] * 256 + mem[off + 2] * 65536 + mem[off + 3] * 16777216
end

local gameFrame = 0
local gameStarted = false
local menuDraws, introDraws = 0, 0
local active = nil
local phase = 'title'
local phaseFrame = 0
local finished = false
local sfxIds, seenId = {}, {}
local streamThemes, seenTheme = {}, {}
bps = {}

local maxDuck = 0
local minVolume = 65535
local minDuckedVolume = 65535
local sawOut = false
local sawSfx = false
local minFadeLevel = 30
local maxFadeLevel = 0

local function countKeys(t)
	local n = 0
	for _ in pairs(t) do n = n + 1 end
	return n
end

local function addBp(addr, label, callback)
	if addr == 0 then return end
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, label, callback, label)
end

addBp(mainAddr, 'mix probe: main', function()
	gameStarted = true
	gameFrame = 0
	return true
end)

addBp(sfxAddr, 'mix probe: audioPlaySfxIdPS1', function()
	local id = PCSX.getRegisters().GPR.n.a0
	seenId[id] = true
	sfxIds[#sfxIds + 1] = id
	return true
end)

addBp(streamAddr, 'mix probe: audioStreamMusicPS1', function()
	local theme = PCSX.getRegisters().GPR.n.a0
	seenTheme[theme] = true
	streamThemes[#streamThemes + 1] = theme
	return true
end)

addBp(menuDrawAddr, 'mix probe: menuDrawPS1', function()
	menuDraws = menuDraws + 1
	return true
end)

addBp(introDrawAddr, 'mix probe: introDrawPS1', function()
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

local function finish(label)
	if finished then return end
	finished = true

	local fadeState  = read32(mixAddr)
	local fadeLevel  = read32(mixAddr + 4)
	local activeTheme= read32(mixAddr + 8)
	local fades      = read32(mixAddr + 16)
	local fadesIn    = read32(mixAddr + 20)
	local duck       = read32(mixAddr + 24)
	local sfxActive  = read32(mixAddr + 32)
	local sfxStarts  = read32(mixAddr + 36)
	local volume     = read32(mixAddr + 48)
	local audioReady = read32(audioDiag)
	local streamOn   = read32(streamDiag)
	local underruns  = read32(streamDiag + 24)
	local lastError  = read32(streamDiag + 28)

	print(string.format('MIX_DIAG fade=%d level=%d theme=%d fades=%d fadesIn=%d duckMax=%d sfxActive=%d sfxStarts=%d vol=%d',
		fadeState, fadeLevel, activeTheme, fades, fadesIn, maxDuck, sfxActive, sfxStarts, volume))
	print(string.format('MIX_OBS minVolume=%d minDuckedVolume=%d sawOut=%d sawSfx=%d fadeLevelMin=%d fadeLevelMax=%d themes=%d',
		minVolume, minDuckedVolume, sawOut and 1 or 0, sawSfx and 1 or 0,
		minFadeLevel, maxFadeLevel, countKeys(seenTheme)))
	print(string.format('MIX_STREAM active=%d underruns=%d lastError=%d audioReady=%d phase=%s frame=%d ids=%s',
		streamOn, underruns, lastError, audioReady, phase, gameFrame,
		table.concat(sfxIds, ',')))
	local checks = {
		audio=audioReady == 1, stream=streamOn == 1, underruns=underruns == 0,
		err=lastError == 0, fades=fades >= 1, fadesIn=fadesIn >= 2,
		theme=activeTheme == 17, out=sawOut, zero=minFadeLevel == 0,
		full=maxFadeLevel == 30, sfxStarts=sfxStarts >= 2, sawSfx=sawSfx,
		duck=maxDuck >= 6, duckVol=minDuckedVolume < 16383,
		themes=countKeys(seenTheme) >= 2, id1=seenId[1] or false,
		id63=seenId[63] or false, id3=seenId[3] or false }
	local failed = {}
	for k, v in pairs(checks) do if not v then failed[#failed + 1] = k end end
	table.sort(failed)
	print('MIX_CHECKS failed=' .. table.concat(failed, ','))

	local ok = #failed == 0
	if ok then
		print('MIX_PROBE: PASS')
	else
		print('MIX_PROBE: FAIL ' .. label)
	end
	PCSX.quit(ok and 0 or 1)
end

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not gameStarted or finished then return end
	gameFrame = gameFrame + 1

	local fadeState = read32(mixAddr)
	local level     = read32(mixAddr + 4)
	local duck      = read32(mixAddr + 24)
	local hold      = read32(mixAddr + 28)
	local activeSfx = read32(mixAddr + 32)
	local volume    = read32(mixAddr + 48)
	local fadesInNow = read32(mixAddr + 20)
	if gameFrame % 120 == 0 then
		print(string.format('MIX_TRACE frame=%d phase=%s fade=%d level=%d fades=%d fi=%d duck=%d hold=%d theme=%d',
			gameFrame, phase, fadeState, level, read32(mixAddr + 16), fadesInNow,
			duck, read32(mixAddr + 28), read32(mixAddr + 8)))
	end
	if duck > maxDuck then maxDuck = duck end
	if hold > 0 and volume < minDuckedVolume then minDuckedVolume = volume end
	if volume < minVolume then minVolume = volume end
	if fadeState == 1 then sawOut = true end
	if activeSfx > 0 then sawSfx = true end
	if level < minFadeLevel then minFadeLevel = level end
	if level > maxFadeLevel then maxFadeLevel = level end

	if active then
		if active.mode == 'wait' then
			if gameFrame >= active.doneAt then
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

	if phase == 'title' and menuDraws >= 2 then
		phase = 'menu'
		phaseFrame = gameFrame
		pulse(B.START, function() end)
	elseif phase == 'menu' and read32(menuAddr) == 1 then
		phase = 'intro'
		phaseFrame = gameFrame
		pulse(B.CROSS, function() end)
	elseif phase == 'intro' and introDraws >= 2 and gameFrame >= phaseFrame + 45 then
		-- On laisse le fondu du theme du titre atteindre 30 avant de changer
		-- de zone : les deux fondus (1 puis 17) doivent etre complets.
		phase = 'game'
		phaseFrame = gameFrame
		pulse(B.START, function() end)  -- saute l'intro, declenche le theme de zone
	elseif phase == 'game' then
		-- Le fondu du theme de zone (1 -> 17) doit etre termine avant d'ouvrir
		-- l'inventaire, sinon le ducking se melangerait a la descente.
		if countKeys(seenTheme) >= 2 and gameFrame >= phaseFrame + 90 then
			phase = 'inventory'
			phaseFrame = gameFrame
			pulse(B.START, function()
				waitFrames(30, function()
					pulse(B.CIRCLE, function()
						waitFrames(60, function() finish('sequence terminee') end)
					end)
				end)
			end)
		end
	elseif gameFrame >= 1800 then
		finish('timeout')
	end
end)

print(string.format('MIX_PROBE ready bps=%d mix=%08x streamDiag=%08x audioDiag=%08x sfx=%08x',
	#bps, mixAddr, streamDiag, audioDiag, sfxAddr))
