-- Sonde de streaming musical CD : prouve que le pilote VAGi lit le CD, remplit
-- le ring buffer et que le SPU recoit des chunks d'IRQ, avec changement de theme
-- entre le titre et la premiere zone jouable.
-- Variables : PS1_MAIN_ADDR, PS1_STREAM_ADDR, PS1_STREAM_DIAG_ADDR,
--             PS1_INTRO_ADDR, PS1_MENU_DRAW_ADDR, PS1_INTRO_DRAW_ADDR,
--             PS1_MENU_ADDR.
-- Les breakpoints restent dans une table GLOBALE (piege GC PCSX Lua).
-- La sequence d'entrees attend des ETATS OBSERVES, pas des images fixes :
-- le pre-remplissage CD decale le demarrage du menu d'environ 120 images.

local mainAddr  = tonumber(os.getenv('PS1_MAIN_ADDR') or '0')
local streamAddr = tonumber(os.getenv('PS1_STREAM_ADDR') or '0')
local diagAddr  = tonumber(os.getenv('PS1_STREAM_DIAG_ADDR') or '0')
local introAddr = tonumber(os.getenv('PS1_INTRO_ADDR') or '0')
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

local function introFrame() return introAddr == 0 and 99 or read32(introAddr) end

local menuDraws = 0
local introDraws = 0
local gameFrame = 0
local gamePhaseFrame = 0
local gameStarted = false
local streamHits = 0
local themes = {}
local seenTheme = {}
local active = nil
local phase = 'title'
local finished = false
bps = {}

local function countThemes()
	local n = 0
	for _ in pairs(seenTheme) do n = n + 1 end
	return n
end

local function addBp(addr, label, callback)
	if addr == 0 then return end
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, label, callback, label)
end

addBp(mainAddr, 'stream probe: main', function()
	gameStarted = true
	gameFrame = 0
	local r = PCSX.getRegisters()
	print(string.format('STREAM_BP main pc=%08x gp=%08x', r.pc, r.GPR.n.gp))
	return true
end)

addBp(streamAddr, 'stream probe: audioStreamMusicPS1', function()
	streamHits = streamHits + 1
	local theme = PCSX.getRegisters().GPR.n.a0
	seenTheme[theme] = true
	themes[#themes + 1] = theme
	print(string.format('STREAM_BP music theme=%d hits=%d', theme, streamHits))
	return true
end)

addBp(menuDrawAddr, 'stream probe: menuDrawPS1', function()
	menuDraws = menuDraws + 1
	return true
end)

addBp(introDrawAddr, 'stream probe: introDrawPS1', function()
	introDraws = introDraws + 1
	return true
end)

local function pulse(button, cb)
	pad.setOverride(button)
	active = { button = button, cb = cb, stage = 1,
		releaseAt = gameFrame + 12, doneAt = gameFrame + 24 }
end

local function finish(label)
	if finished then return end
	finished = true
	local activeF = read32(diagAddr)
	local theme = read32(diagAddr + 4)
	local found = read32(diagAddr + 8)
	local sectors = read32(diagAddr + 12)
	local fed = read32(diagAddr + 16)
	local chunks = read32(diagAddr + 20)
	local under = read32(diagAddr + 24)
	local err = read32(diagAddr + 28)
	local len0 = read32(diagAddr + 32)
	local len1 = read32(diagAddr + 36)
	local len2 = read32(diagAddr + 40)
	local len3 = read32(diagAddr + 44)
	local preStart = read32(diagAddr + 48)
	local feedCalls = read32(diagAddr + 52)

	print(string.format('STREAM_DIAG active=%d theme=%d fileFound=%d sectors=%d feedBytes=%d chunks=%d underruns=%d lastError=%d distinct=%d irqLens=%d,%d,%d,%d preStart=%d feedCalls=%d',
		activeF, theme, found, sectors, fed, chunks, under, err, countThemes(), len0, len1, len2, len3, preStart, feedCalls))

	local ok = activeF == 1 and found == 1 and sectors > 0 and fed > 0 and
		chunks > 0 and under == 0 and err == 0 and countThemes() >= 2
	if ok then
		print('STREAM_PROBE: PASS')
	else
		print('STREAM_PROBE: FAIL ' .. label)
	end
	PCSX.quit(ok and 0 or 1)
end

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not gameStarted or finished then return end
	gameFrame = gameFrame + 1

	if gameFrame % 120 == 0 then
		print(string.format('STREAM_PHASE phase=%s frame=%d menuDraws=%d introDraws=%d intro=%d menuScreen=%d distinct=%d',
			phase, gameFrame, menuDraws, introDraws, introFrame(), read32(menuAddr), countThemes()))
	end

	if active then
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
		-- Le titre est reellement affine : START ouvre le menu principal.
		phase = 'menu'
		pulse(B.START, function() end)
	elseif phase == 'menu' and read32(menuAddr) == 1 then
		-- MENU_SCR_MAIN : CROSS valide "NEW GAME" (curseur 0).
		phase = 'intro'
		pulse(B.CROSS, function() end)
	elseif phase == 'intro' and introDraws >= 2 then
		-- L'intro tourne : START la saute et declenche le theme de zone.
		phase = 'game'
		gamePhaseFrame = 0
		pulse(B.START, function() end)
	elseif phase == 'game' then
		if gamePhaseFrame == 0 then gamePhaseFrame = gameFrame end
		if countThemes() >= 2 and gameFrame >= gamePhaseFrame + 120 then
			finish('sequence terminee')
		end
	elseif gameFrame >= 1500 then
		finish('timeout')
	end
end)

print(string.format('STREAM_PROBE ready bps=%d stream=%08x diag=%08x intro=%08x',
	#bps, streamAddr, diagAddr, introAddr))
