-- Capture native duel de l'ogre de Kindra (v0.8.9).
-- Navigation reprise telle quelle de capture_enemy_anim.lua (jalon prouve).
-- La sonde ecrit QuestState.zone=13 et la position du heros en RAM ; aucun
-- fichier de jeu ni etat sauvegarde n'est modifie. Elle simule uniquement des
-- appuis CARRE, jamais CROIX, et verifie que l'etat de quete reste identique.
--
-- Variables d'environnement : SESSION_OUT, PS1_MAIN_ADDR, PS1_HERO_X_ADDR,
-- PS1_HERO_Y_ADDR, PS1_DIR_ADDR, PS1_QUEST_ADDR, PS1_INTRO_ADDR,
-- PS1_DLG_SHOWN_ADDR, PS1_DLG_PANEL_ADDR, PS1_ENEMY_ADDR, PS1_DUEL_ADDR,
-- PS1_MENU_DRAW_ADDR, PS1_INTRO_DRAW_ADDR, PS1_MENU_ADDR, PS1_AUDIO_DIAG_ADDR.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x80015600')
local heroXAddr = tonumber(os.getenv('PS1_HERO_X_ADDR') or '0x8008ec28')
local heroYAddr = tonumber(os.getenv('PS1_HERO_Y_ADDR') or '0x8008ec24')
local dirAddr = tonumber(os.getenv('PS1_DIR_ADDR') or '0x8008e5f4')
local questAddr = tonumber(os.getenv('PS1_QUEST_ADDR') or '0x800af3b8')
local introAddr = tonumber(os.getenv('PS1_INTRO_ADDR') or '0x800af3fc')
local shownAddr = tonumber(os.getenv('PS1_DLG_SHOWN_ADDR') or '0x8008ec58')
local panelAddr = tonumber(os.getenv('PS1_DLG_PANEL_ADDR') or '0x8008ec5c')
local enemyAddr = tonumber(os.getenv('PS1_ENEMY_ADDR') or '0x8003c4d0')
local duelBase = tonumber(os.getenv('PS1_DUEL_ADDR') or '0x801af1d0')
local menuDrawAddr = tonumber(os.getenv('PS1_MENU_DRAW_ADDR') or '0')
local introDrawAddr = tonumber(os.getenv('PS1_INTRO_DRAW_ADDR') or '0')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR') or '0')
local drawAddr = tonumber(os.getenv('PS1_PFDRAW_ADDR') or '0')
local audioAddr = tonumber(os.getenv('PS1_AUDIO_DIAG_ADDR') or '0')
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local mem = PCSX.getMemPtr()
local gameFrame = 0
local started = false
local phase = 'title'
local active = nil
local captured = {}
local rectProbes = 0
local menuDraws = 0
local introDraws = 0
local phaseFrame = 0
local pre = nil
local preSfxCount = -1
local sfxSeen = {}

local function read32(addr)
	local off = addr - 0x80000000
	return mem[off] + mem[off + 1] * 256 + mem[off + 2] * 65536 + mem[off + 3] * 16777216
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

-- PS1AudioDiag (POD) : ready @0, playCount @4, uploadBytes @8, verify @12,
-- lastId @16, lastVoice @20. LastId n'est ecrit que par playClip(), donc il
-- reflechit le dernier SFX (ou le jingle de fin, jamais atteint ici).
local function audioReady() return audioAddr ~= 0 and read32(audioAddr) == 1 end
local function audioPlayCount()
	if audioAddr == 0 then return -1 end
	return read32(audioAddr + 4)
end
local function audioLastId()
	if audioAddr == 0 then return -1 end
	return read32(audioAddr + 16)
end
local function recordSfx(name, step)
	sfxSeen[name] = { id = audioLastId(), count = audioPlayCount(), step = step }
end

-- PS1OgreDuelState : hp/cooldown @0, stun/flash @4, phaseTimer/state @8,
-- hits/prevSquare @12, steps @16 (20 octets, POD).
local function duelHp() return bit.band(read32(duelBase), 0xffff) end
local function duelCooldown() return bit.band(bit.rshift(read32(duelBase), 16), 0xffff) end
local function duelStun() return bit.band(read32(duelBase + 4), 0xffff) end
local function duelFlash() return bit.band(bit.rshift(read32(duelBase + 4), 16), 0xffff) end
local function duelPhase() return bit.band(bit.rshift(read32(duelBase + 8), 16), 0xffff) end
local function duelHits() return bit.band(read32(duelBase + 12), 0xffff) end
local function duelSteps() return read32(duelBase + 16) end

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

local lateShot = nil

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
	local sfx = sfxSeen[name] or { id = -1, count = -1, step = -1 }
	print(string.format('shot %s %dx%d frame=%d hero=(%d,%d) zone=%d stage=%d boss=%d gates=%d dialogue=%d duel=(hp=%d,phase=%d,hits=%d,flash=%d,stun=%d,steps=%d) audio=(last=%d,count=%d,hit=%d)',
		name, w, h, gameFrame, heroX(), heroY(), zone(), stage(), bossDown(),
		gates(), dialogueId(), duelHp(), duelPhase(), duelHits(), duelFlash(),
		duelStun(), duelSteps(), audioLastId(), audioPlayCount(), sfx.id))
end

-- La sonde lit l'etat au debut du VSync, avant que le jeu n'execute la liste
-- GPU de l'image courante. Les captures avec flash sont donc differees de
-- 3 frames ; sans ce delai, l'etat RAM est correct mais le flash n'est pas
-- encore present dans le tampon affiche (defaut observe et corrige).
local function shot(name)
	shotNow(name)
end

local function shotDeferred(name)
	lateShot = { at = gameFrame + 3, name = name }
end

local function fail(message)
	print('DUEL_CAPTURE: FAIL ' .. message)
	PCSX.quit(1)
end

local function waitFrames(frames, cb)
	active = { button = nil, mode = 'wait', cb = cb, doneAt = gameFrame + frames }
end

local function pulse(button, cb)
	pad.setOverride(button)
	active = { button = button, mode = 'pulse', cb = cb, stage = 1,
		releaseAt = gameFrame + 14, doneAt = gameFrame + 28 }
end

-- Appui CARRE bref : 1 frame appuyee, relachement a F+1, callback a F+2.
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
	if duelHp() ~= 0 or duelPhase() ~= 3 or duelHits() ~= 7 then
		fail(string.format('duel non termine hp=%d phase=%d hits=%d',
			duelHp(), duelPhase(), duelHits()))
		return
	end
	for _, name in ipairs({ 'duel_guard_hit', 'duel_open_hit', 'duel_down' }) do
		local seen = false
		for _, got in ipairs(captured) do if got == name then seen = true end end
		if not seen then fail('capture absente: ' .. name) return end
	end

	-- Rétroaction sonore : les identifiants A2 sont lus au moment du coup, pas
	-- à la capture différée, pour éviter toute contamination par une image
	-- ultérieure. playCount doit croître à chaque coup accepté.
	if not audioReady() then fail('diagnostic audio non pret') return end
	local expectedSfx = { duel_guard_hit = 20, duel_open_hit = 66, duel_down = 41 }
	local lastCount = preSfxCount
	local lastStep = -1
	for _, name in ipairs({ 'duel_guard_hit', 'duel_open_hit', 'duel_down' }) do
		local sfx = sfxSeen[name]
		if not sfx then fail('aucun SFX enregistre pour ' .. name) return end
		if sfx.id ~= expectedSfx[name] then
			fail(string.format('SFX inattendu sur %s: id=%d attendu=%d', name, sfx.id, expectedSfx[name]))
			return
		end
		if sfx.count <= lastCount then
			fail(string.format('playCount non croissant sur %s: %d apres %d', name, sfx.count, lastCount))
			return
		end
		if sfx.step <= lastStep then
			fail(string.format('ordre des coups inattendu sur %s: step=%d apres %d', name, sfx.step, lastStep))
			return
		end
		lastCount = sfx.count
		lastStep = sfx.step
	end
	print(string.format('DUEL_SFX guard=%d open=%d down=%d steps=%d/%d/%d count=%d->%d ready=%d',
		sfxSeen.duel_guard_hit.id, sfxSeen.duel_open_hit.id, sfxSeen.duel_down.id,
		sfxSeen.duel_guard_hit.step, sfxSeen.duel_open_hit.step, sfxSeen.duel_down.step,
		preSfxCount, lastCount, audioReady() and 1 or 0))
	print(string.format('DUEL_CAPTURE: PASS hp=%d phase=%d hits=%d steps=%d rectProbes=%d shots=%s zone=%d stage=%d boss=%d gates=%d dialogue=%d',
		duelHp(), duelPhase(), duelHits(), duelSteps(), rectProbes, table.concat(captured, ','),
		post.zone, post.stage, post.boss, post.gates, post.dialogue))
	PCSX.quit(0)
end

local function landNext()
	waitUntil(function() return duelPhase() == 1 and duelCooldown() == 0 end,
		'OUVERT', function()
			tapSquare(function()
				local hit = duelHits()
				if hit == 1 then
					recordSfx('duel_open_hit', duelSteps())
					shotDeferred('duel_open_hit')
				end
				if duelHp() == 0 then
					recordSfx('duel_down', duelSteps())
					shot('duel_down')
					waitFrames(6, verifyFinish)
				else
					landNext()
				end
			end)
		end)
end

local function runDuel()
	waitUntil(function() return duelPhase() == 0 end, 'GARDE', function()
		tapSquare(function()
			recordSfx('duel_guard_hit', duelSteps())
			shotDeferred('duel_guard_hit')
			if duelHp() ~= 14 or duelHits() ~= 0 then
				fail('coup en garde a retire des PV : hp=' .. duelHp() .. ' hits=' .. duelHits())
				return
			end
			landNext()
		end)
	end)
end

local function ogreAvant()
	write32(questAddr + 56, 13)
	teleport(540, 462)
	waitFrames(20, function()
		if zone() ~= 13 then fail('zone QuestState 013 non posee, lue ' .. zone()) return end
		if enemyFrames() ~= 6 then fail('blob P1AN non initialize') return end
		if bossDown() ~= 0 then fail('ogre deja vaincu avant capture') return end
		pre = snapshot()
		preSfxCount = audioPlayCount()
		if pre.stage ~= 0 or pre.gates ~= 0 or pre.dialogue ~= 0xffffffff then
			fail('etat initial inattendu stage=' .. pre.stage .. ' gates=' .. pre.gates
				.. ' dialogue=' .. pre.dialogue)
			return
		end
		runDuel()
	end)
end

bps = {}
local function addBp(addr, label, cb)
	if addr == 0 then return end
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, label, cb, label)
end
addBp(menuDrawAddr, 'capture duel: menuDrawPS1', function() menuDraws = menuDraws + 1 return true end)
addBp(introDrawAddr, 'capture duel: introDrawPS1', function() introDraws = introDraws + 1 return true end)

-- Diagnostic cible : verifier que les 3 tiles du flash atteignent pfDrawRect.
-- Filtre y=60..100 pour ignorer le fond d'ecran (y=0) et le HUD.
if drawAddr ~= 0 then
	bps[#bps + 1] = PCSX.addBreakpoint(drawAddr, 'Exec', 4, 'capture duel: pfDrawRect flash',
		function()
			if duelFlash() == 0 then return true end
			local r = PCSX.getRegisters()
			local x = r.GPR.n.a0
			local y = r.GPR.n.a1
			if y >= 60 and y <= 100 then
				local w = r.GPR.n.a2
				local h = r.GPR.n.a3
				local color = read32(r.GPR.n.sp + 16)
				local z = read32(r.GPR.n.sp + 20)
				rectProbes = rectProbes + 1
				print(string.format('RECT_PROBE frame=%d x=%d y=%d w=%d h=%d color=%04x z=%d flash=%d',
					gameFrame, x, y, w, h, bit.band(color, 0xffff), z, duelFlash()))
			end
			return true
		end, 'capture duel: pfDrawRect flash')
end

bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'capture duel: main reached',
	function()
		started = true
		gameFrame = 0
		local r = PCSX.getRegisters()
		print(string.format('CAPTURE main reached pc=%08x ra=%08x sp=%08x gp=%08x',
			r.pc, r.GPR.n.ra, r.GPR.n.sp, r.GPR.n.gp))
		return false
	end,
	'capture duel: main reached')

listener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not started then return end
	gameFrame = gameFrame + 1

	if lateShot and gameFrame >= lateShot.at then
		local name = lateShot.name
		lateShot = nil
		shotNow(name)
	end

	if gameFrame % 120 == 0 then
		print(string.format('WAIT phase=%s hero=(%d,%d) zone=%d stage=%d duel=(hp=%d,phase=%d,hits=%d,steps=%d)',
			phase, heroX(), heroY(), zone(), stage(), duelHp(), duelPhase(),
			duelHits(), duelSteps()))
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
				fail('condition non observer: ' .. active.label .. ' hp=' .. duelHp()
					.. ' phase=' .. duelPhase() .. ' stun=' .. duelStun())
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
		waitFrames(20, ogreAvant)
	end

	if gameFrame > 20000 then
		fail('timeout phase=' .. phase .. ' zone=' .. zone() .. ' hp=' .. duelHp())
	end
end)

print('CAPTURE_DUEL ready, main=0x' .. string.format('%08x', mainAddr) ..
	' duel=0x' .. string.format('%08x', duelBase) .. ' outdir=' .. outdir)
