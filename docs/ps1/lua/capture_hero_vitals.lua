-- Capture native des constantes vitales locales du heros (v0.8.11).
-- La sonde ecrit QuestState.zone=13 et la position du heros en RAM, puis
-- attend les contre-attaques de l'ogre en GARDE. Elle n'appuie jamais CROIX
-- apres le menu : seul CARRÉ valide la sortie du Game Over local.
--
-- Variables d'environnement : SESSION_OUT, PS1_MAIN_ADDR, PS1_HERO_X_ADDR,
-- PS1_HERO_Y_ADDR, PS1_QUEST_ADDR, PS1_INTRO_ADDR, PS1_DLG_SHOWN_ADDR,
-- PS1_DLG_PANEL_ADDR, PS1_ENEMY_ADDR, PS1_DUEL_ADDR, PS1_VITALS_ADDR,
-- PS1_MENU_DRAW_ADDR, PS1_INTRO_DRAW_ADDR, PS1_MENU_ADDR, PS1_PFDRAW_ADDR,
-- PS1_AUDIO_DIAG_ADDR.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x8001f0e4')
local heroXAddr = tonumber(os.getenv('PS1_HERO_X_ADDR') or '0x8008ec28')
local heroYAddr = tonumber(os.getenv('PS1_HERO_Y_ADDR') or '0x8008ec24')
local dirAddr = tonumber(os.getenv('PS1_DIR_ADDR') or '0x8008e5f4')
local questAddr = tonumber(os.getenv('PS1_QUEST_ADDR') or '0x800af3b8')
local introAddr = tonumber(os.getenv('PS1_INTRO_ADDR') or '0x800af3fc')
local shownAddr = tonumber(os.getenv('PS1_DLG_SHOWN_ADDR') or '0x8008ec58')
local panelAddr = tonumber(os.getenv('PS1_DLG_PANEL_ADDR') or '0x8008ec5c')
local enemyAddr = tonumber(os.getenv('PS1_ENEMY_ADDR') or '0x8003c4d0')
local duelBase = tonumber(os.getenv('PS1_DUEL_ADDR') or '0x801afa58')
local vitalsBase = tonumber(os.getenv('PS1_VITALS_ADDR') or '0x8003d210')
local menuDrawAddr = tonumber(os.getenv('PS1_MENU_DRAW_ADDR') or '0')
local introDrawAddr = tonumber(os.getenv('PS1_INTRO_DRAW_ADDR') or '0')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR') or '0')
local drawAddr = tonumber(os.getenv('PS1_PFDRAW_ADDR') or '0')
local drawSemiAddr = tonumber(os.getenv('PS1_PFDRAW_SEMI_ADDR') or '0')
local audioAddr = tonumber(os.getenv('PS1_AUDIO_DIAG_ADDR') or '0')
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local mem = PCSX.getMemPtr()
local gameFrame = 0
local started = false
local phase = 'title'
local active = nil
local captured = {}
local menuDraws = 0
local introDraws = 0
local phaseFrame = 0
local pre = nil
local preSfxCount = -1
local lateShot = nil
local vitalsSeen = {}
local crossAfterGame = 0
local veilProbes = 0
local respawnInvulnAtExit = -1

local function read32(addr)
	local off = addr - 0x80000000
	return mem[off] + mem[off + 1] * 256 + mem[off + 2] * 65536 + mem[off + 3] * 16777216
end

local function read16(addr)
	return bit.band(read32(addr), 0xffff)
end

local function write32(addr, value)
	local off = addr - 0x80000000
	local v = math.floor(value) % 4294967296
	mem[off] = v % 256
	mem[off + 1] = math.floor(v / 256) % 256
	mem[off + 2] = math.floor(v / 65536) % 256
	mem[off + 3] = math.floor(v / 16777216) % 256
end

local function heroX() return math.floor(read32(heroXAddr) / 65536) end
local function heroY() return math.floor(read32(heroYAddr) / 65536) end
local function stage() return read32(questAddr) end
local function dialogueId() return read32(questAddr + 8) end
local function bossDown() return read32(questAddr + 48) end
local function zone() return read32(questAddr + 56) end
local function gates() return read32(questAddr + 64) end
local function dlgShown() return read32(shownAddr) end
local function introFrame() return read32(introAddr) end
local function enemyFrames() return bit.band(read32(enemyAddr + 4), 0xffff) end
local function duelHp() return read16(duelBase) end
local function duelPhase() return bit.band(bit.rshift(read32(duelBase + 8), 16), 0xffff) end
local function duelHits() return read16(duelBase + 12) end
local function duelSteps() return read32(duelBase + 16) end

-- PS1HeroVitalsState : 16 octets, hp/invuln/attackTimer/hitsTaken en uint16_t
-- aux offsets 0/2/4/6, steps (uint32_t) a 8, reserved a 12.
local function vitalsHp() return read16(vitalsBase) end
local function vitalsInvuln() return read16(vitalsBase + 2) end
local function vitalsAttackTimer() return read16(vitalsBase + 4) end
local function vitalsHitsTaken() return read16(vitalsBase + 6) end
local function vitalsSteps() return read32(vitalsBase + 8) end

-- PS1AudioDiag : playCount @4, lastId @16. Le SFX de degats est l'ID A2 71.
local function audioReady() return audioAddr ~= 0 and read32(audioAddr) == 1 end
local function audioPlayCount()
	if audioAddr == 0 then return -1 end
	return read32(audioAddr + 4)
end
local function audioLastId()
	if audioAddr == 0 then return -1 end
	return read32(audioAddr + 16)
end

local function teleport(x, y)
	write32(heroXAddr, x * 65536)
	write32(heroYAddr, y * 65536)
	write32(dirAddr, 2)
end

local function snapshot()
	return { zone = zone(), stage = stage(), boss = bossDown(),
		gates = gates(), dialogue = dialogueId(), shown = dlgShown() }
end

local function sameState(a, b)
	return a.zone == b.zone and a.stage == b.stage and a.boss == b.boss and
	       a.gates == b.gates and a.dialogue == b.dialogue and a.shown == b.shown
end

local function vitalsText()
	return string.format('hp=%d invuln=%d timer=%d hits=%d steps=%d',
		vitalsHp(), vitalsInvuln(), vitalsAttackTimer(),
		vitalsHitsTaken(), vitalsSteps())
end

local function shotNow(name)
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
	captured[#captured + 1] = name
	vitalsSeen[name] = { hp = vitalsHp(), invuln = vitalsInvuln(),
		hits = vitalsHitsTaken(), steps = vitalsSteps(),
		count = audioPlayCount(), id = audioLastId(), frame = gameFrame }
	print(string.format('shot %s %dx%d frame=%d hero=(%d,%d) zone=%d stage=%d boss=%d gates=%d dialogue=%d %s duel=(hp=%d,phase=%d,hits=%d,steps=%d) audio=(last=%d,count=%d) veilProbes=%d',
		name, w, h, gameFrame, heroX(), heroY(), zone(), stage(), bossDown(),
		gates(), dialogueId(), vitalsText(), duelHp(), duelPhase(),
		duelHits(), duelSteps(), audioLastId(), audioPlayCount(), veilProbes))
end

local function shot(name)
	shotNow(name)
end

local function shotDeferred(name)
	lateShot = { at = gameFrame + 3, name = name }
end

local function fail(message)
	print('VITALS_CAPTURE: FAIL ' .. message)
	PCSX.quit(1)
end

local function waitFrames(frames, cb)
	active = { button = nil, mode = 'wait', cb = cb, doneAt = gameFrame + frames }
end

local function pulse(button, cb)
	if phase == 'game' and button == B.CROSS then crossAfterGame = crossAfterGame + 1 end
	pad.setOverride(button)
	active = { button = button, mode = 'pulse', cb = cb, stage = 1,
		releaseAt = gameFrame + 14, doneAt = gameFrame + 28 }
end

local function tapSquare(cb)
	pad.setOverride(B.SQUARE)
	active = { button = B.SQUARE, mode = 'tap', cb = cb,
		releaseAt = gameFrame + 1, doneAt = gameFrame + 2 }
end

local function waitUntil(pred, label, cb, timeout)
	active = { button = nil, mode = 'until', pred = pred, label = label, cb = cb,
		doneAt = gameFrame + (timeout or 400) }
end

local function verifyFinish()
	local post = snapshot()
	if not sameState(pre, post) then
		fail(string.format('etat de quete modifie pre=(z%d,s%d,b%d,g%d,d%d) post=(z%d,s%d,b%d,g%d,d%d)',
			pre.zone, pre.stage, pre.boss, pre.gates, pre.dialogue,
			post.zone, post.stage, post.boss, post.gates, post.dialogue))
		return
	end
	if crossAfterGame ~= 0 then fail('CROIX appuyee apres le menu: ' .. crossAfterGame) return end
	if duelHp() ~= 14 or duelHits() ~= 0 then
		fail(string.format('duel modifie hp=%d hits=%d', duelHp(), duelHits()))
		return
	end
	for _, name in ipairs({ 'hero_vitals_full', 'hero_vitals_damaged',
	                        'hero_vitals_game_over', 'hero_vitals_respawn' }) do
		local seen = false
		for _, got in ipairs(captured) do if got == name then seen = true end end
		if not seen then fail('capture absente: ' .. name) return end
	end
	local full = vitalsSeen.hero_vitals_full
	local damaged = vitalsSeen.hero_vitals_damaged
	local over = vitalsSeen.hero_vitals_game_over
	local respawn = vitalsSeen.hero_vitals_respawn
	if not full or full.hp ~= 10 or full.invuln ~= 0 then fail('PV initiaux inattendus') return end
	if not damaged or damaged.hp > 8 or damaged.hp < 1 then fail('PV endommages inattendus') return end
	if not over or over.hp ~= 0 then fail('Game Over sans 0 PV') return end
	-- L'invulnerabilite de grace est posee a 90 a la sortie, puis decremente
	-- d'une unite par image : la capture et la verification sont plus tardives.
	if not respawn or respawn.hp ~= 10 or respawn.invuln <= 70 or respawn.invuln > 90
	   or respawnInvulnAtExit < 85 or respawnInvulnAtExit > 90 then
		fail('respawn inattendu: ' .. (respawn and vitalsText() or 'absent')
			.. ' sortie=' .. respawnInvulnAtExit) return
	end
	if damaged.count <= full.count then fail('playCount audio non croissant apres coup') return end
	if over.count <= damaged.count then fail('playCount audio non croissant jusqu a 0 PV') return end
	if over.id ~= 71 then fail('lastId audio inattendu: ' .. over.id .. ' (attendu 71)') return end
	if over.hits ~= 10 then fail('hitsTaken inattendu: ' .. over.hits .. ' (attendu 10)') return end
	if over.steps == full.steps then fail('steps vital non avance') return end
	if veilProbes == 0 then fail('aucun voile semi-transparent observe') return end
	print(string.format('VITALS_SFX: damaged=%d over=%d respawn=%d count=%d->%d->%d->%d ready=%d',
		damaged.id, over.id, respawn.id, full.count, damaged.count,
		over.count, respawn.count, audioReady() and 1 or 0))
	print(string.format('VITALS_CAPTURE: PASS full=%d damaged=%d over=%d respawn=%d/invuln=%d hits=%d steps=%d/%d veilProbes=%d shots=%s zone=%d stage=%d boss=%d gates=%d dialogue=%d',
		full.hp, damaged.hp, over.hp, respawn.hp, respawn.invuln, over.hits,
		full.steps, over.steps, veilProbes, table.concat(captured, ','),
		post.zone, post.stage, post.boss, post.gates, post.dialogue))
	PCSX.quit(0)
end

local function afterRespawn()
	waitFrames(5, function()
		shotDeferred('hero_vitals_respawn')
		waitFrames(6, verifyFinish)
	end)
end

local function exitGameOver()
	-- Les compteurs vitaux sont geles en Game Over : l'invulnerabilite du
	-- dernier coup reste a 70, elle ne doit pas continuer a decremente.
	if vitalsHp() ~= 0 or vitalsInvuln() > 70 then fail('Game Over non stable avant CARRE') return end
	tapSquare(function()
		if vitalsHp() ~= 10 then fail('CARRÉ na pas fait revivre le heros: hp=' .. vitalsHp()) return end
		respawnInvulnAtExit = vitalsInvuln()
		afterRespawn()
	end)
end

local function captureGameOver()
	if vitalsHp() ~= 0 then fail('PV non nuls avant capture Game Over') return end
	waitFrames(55, function()
		if vitalsHp() ~= 0 then fail('sortie automatique avant capture') return end
		shot('hero_vitals_game_over')
		exitGameOver()
	end)
end

local function waitDown()
	waitUntil(function() return vitalsHp() == 0 end, 'DOWN', captureGameOver, 5000)
end

local function captureDamaged()
	if vitalsHp() > 8 then fail('PV non endommages: ' .. vitalsHp()) return end
	shotDeferred('hero_vitals_damaged')
	waitFrames(6, waitDown)
end

local function startVitals()
	-- Capture de reference AVANT d'approcher l'ogre : des que le heros entre a
	-- portee en phase GARDE, le contre-coup est accepte des la premiere image.
	if vitalsHp() ~= 10 then fail('PV initiaux inattendus: ' .. vitalsHp()) return end
	preSfxCount = audioPlayCount()
	shotDeferred('hero_vitals_full')
	waitFrames(6, function()
		write32(questAddr + 56, 13)
		teleport(540, 462)
		waitFrames(20, function()
			if zone() ~= 13 then fail('zone QuestState 013 non posee, lue ' .. zone()) return end
			if enemyFrames() ~= 6 then fail('blob P1AN non initialize') return end
			if bossDown() ~= 0 then fail('ogre deja vaincu avant capture') return end
			pre = snapshot()
			if pre.stage ~= 0 or pre.gates ~= 0 or pre.dialogue ~= 0xffffffff then
				fail('etat initial inattendu stage=' .. pre.stage .. ' gates=' .. pre.gates
					.. ' dialogue=' .. pre.dialogue)
				return
			end
			waitUntil(function() return vitalsHp() <= 8 end, 'DAMAGED', captureDamaged, 5000)
		end)
	end)
end

bps = {}
local function addBp(addr, label, cb)
	if addr == 0 then return end
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, label, cb, label)
end
addBp(menuDrawAddr, 'capture vitals: menuDrawPS1', function() menuDraws = menuDraws + 1 return true end)
addBp(introDrawAddr, 'capture vitals: introDrawPS1', function() introDraws = introDraws + 1 return true end)

-- Le voile du Game Over utilise pfDrawRectSemi (pas pfDrawRect) : son point
-- d'entree propre est pose sur le symbole ELF. Le filtre plein ecran distingue
-- le voile du bandeau du menu, qui utilise aussi le semi-transparent.
if drawSemiAddr ~= 0 then
	bps[#bps + 1] = PCSX.addBreakpoint(drawSemiAddr, 'Exec', 4, 'capture vitals: veil',
		function()
			local r = PCSX.getRegisters()
			local x = r.GPR.n.a0
			local y = r.GPR.n.a1
			local w = r.GPR.n.a2
			local h = r.GPR.n.a3
			if x == 0 and y == 0 and w == 320 and h == 240 and vitalsHp() == 0 then
				veilProbes = veilProbes + 1
			end
			return true
		end, 'capture vitals: veil')
end

bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'capture vitals: main reached',
	function()
		started = true
		gameFrame = 0
		local r = PCSX.getRegisters()
		print(string.format('CAPTURE main reached pc=%08x ra=%08x sp=%08x gp=%08x',
			r.pc, r.GPR.n.ra, r.GPR.n.sp, r.GPR.n.gp))
		return false
	end,
	'capture vitals: main reached')

listener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not started then return end
	gameFrame = gameFrame + 1

	if lateShot and gameFrame >= lateShot.at then
		local name = lateShot.name
		lateShot = nil
		shotNow(name)
	end

	if gameFrame % 120 == 0 then
		print(string.format('WAIT phase=%s hero=(%d,%d) zone=%d stage=%d %s duel=(hp=%d,phase=%d,hits=%d,steps=%d)',
			phase, heroX(), heroY(), zone(), stage(), vitalsText(),
			duelHp(), duelPhase(), duelHits(), duelSteps()))
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
		if active.mode == 'tap' then
			if gameFrame >= active.releaseAt then pad.clearOverride(active.button) end
			if gameFrame >= active.doneAt then
				local cb = active.cb
				active = nil
				if cb then cb() end
			end
			return
		end
		if active.mode == 'until' then
			if active.pred() then
				local cb = active.cb
				active = nil
				if cb then cb() end
			elseif gameFrame >= active.doneAt then
				fail('condition non observer: ' .. active.label .. ' ' .. vitalsText())
			end
			return
		end
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
		phase = 'game'
		phaseFrame = gameFrame
		pulse(B.START, function() end)
	elseif phase == 'game' and gameFrame >= phaseFrame + 40 and enemyFrames() == 6 and duelSteps() > 0 then
		phase = 'duel'
		waitFrames(20, startVitals)
	end

	if gameFrame > 20000 then
		fail('timeout phase=' .. phase .. ' zone=' .. zone() .. ' ' .. vitalsText())
	end
end)
