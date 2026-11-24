-- Parcours rejouable ogre -> progression sur l'ISO v0.8.23 (capture-only).
-- La sonde pose UNIQUEMENT QuestState.zone=13 et la position/direction du
-- heros (diagnostic de placement). Aucun autre champ de QuestState n'est ecrit.
-- Les consequences de la quete doivent venir du jeu :
--   A) CROIX avant la victoire ne contourne pas le combat obligatoire ;
--   B) les coups CARRE jusqu'a hp=0 posent bossDown/stage/dialogue ;
--   C) CROIX apres victoire confirme le dialogue puis ouvre le coffre lettre,
--      sans recompense double.
--
-- Variables d'environnement : SESSION_OUT, PS1_MAIN_ADDR, PS1_HERO_X_ADDR,
-- PS1_HERO_Y_ADDR, PS1_DIR_ADDR, PS1_QUEST_ADDR, PS1_INTRO_ADDR,
-- PS1_DLG_SHOWN_ADDR, PS1_DLG_PANEL_ADDR, PS1_ENEMY_ADDR, PS1_DUEL_ADDR,
-- PS1_MENU_DRAW_ADDR, PS1_INTRO_DRAW_ADDR, PS1_MENU_ADDR.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x8002055c')
local heroXAddr = tonumber(os.getenv('PS1_HERO_X_ADDR') or '0x8008ec28')
local heroYAddr = tonumber(os.getenv('PS1_HERO_Y_ADDR') or '0x8008ec24')
local dirAddr = tonumber(os.getenv('PS1_DIR_ADDR') or '0x8008e5f4')
local questAddr = tonumber(os.getenv('PS1_QUEST_ADDR') or '0x801c3b98')
local introAddr = tonumber(os.getenv('PS1_INTRO_ADDR') or '0x800af3fc')
local shownAddr = tonumber(os.getenv('PS1_DLG_SHOWN_ADDR') or '0x8008ec58')
local panelAddr = tonumber(os.getenv('PS1_DLG_PANEL_ADDR') or '0x8008ec5c')
local dlgActiveAddr = tonumber(os.getenv('PS1_DLG_ACTIVE_ADDR') or '0')
local enemyAddr = tonumber(os.getenv('PS1_ENEMY_ADDR') or '0x8003c4d0')
local duelBase = tonumber(os.getenv('PS1_DUEL_ADDR') or '0x801af250')
local menuDrawAddr = tonumber(os.getenv('PS1_MENU_DRAW_ADDR') or '0')
local introDrawAddr = tonumber(os.getenv('PS1_INTRO_DRAW_ADDR') or '0')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR') or '0x801c3fdc')
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local mem = PCSX.getMemPtr()

local gameFrame = 0
local started = false
local phase = 'title'
local phaseFrame = 0
local active = nil
local menuDraws = 0
local introDraws = 0
local captured = {}

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
local function chestLettre() return read32(questAddr + 52) end
local function zone() return read32(questAddr + 56) end
local function gates() return read32(questAddr + 64) end
local function dlgShown() return read32(shownAddr) end
local function dlgPanel() return read32(panelAddr) end
-- g_shown est un compteur de glyphes (76 observe), pas un booleen : seule
-- l'adresse de g_active donne l'etat ouvert/ferme de la fenetre.
local function dlgIsActive() return dlgActiveAddr ~= 0 and read32(dlgActiveAddr) == 1 end
local function introFrame() return read32(introAddr) end
local function enemyFrames() return bit.band(read32(enemyAddr + 4), 0xffff) end

local function duelHp() return bit.band(read32(duelBase), 0xffff) end
local function duelCooldown() return bit.band(bit.rshift(read32(duelBase), 16), 0xffff) end
local function duelPhase() return bit.band(bit.rshift(read32(duelBase + 8), 16), 0xffff) end
local function duelHits() return bit.band(read32(duelBase + 12), 0xffff) end
local function duelSteps() return read32(duelBase + 16) end

local function teleport(x, y, dir)
	write32(heroXAddr, x * 65536)
	write32(heroYAddr, y * 65536)
	write32(dirAddr, dir)
end

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
		px[#px + 1] = string.char(bit.band(c, 31) * 8,
			bit.band(bit.rshift(c, 5), 31) * 8,
			bit.band(bit.rshift(c, 10), 31) * 8)
	end
	local file = io.open(outdir .. '/' .. name .. '.ppm', 'wb')
	file:write(string.format('P6\n%d %d\n255\n', w, h))
	file:write(table.concat(px))
	file:close()
	captured[#captured + 1] = name
	print(string.format('SHOT %s frame=%d hero=(%d,%d) boss=%d stage=%d dlg=%d coffre=%d hp=%d hits=%d',
		name, gameFrame, heroX(), heroY(), bossDown(), stage(), dialogueId(),
		chestLettre(), duelHp(), duelHits()))
end

local function fail(message)
	print('OGRE_PROGRESSION: FAIL ' .. message)
	PCSX.quit(1)
end

local function waitFrames(frames, cb)
	active = { button = nil, mode = 'wait', cb = cb, doneAt = gameFrame + frames }
end

local function tap(button, cb)
	pad.setOverride(button)
	active = { button = button, mode = 'tap', cb = cb,
		releaseAt = gameFrame + 1, doneAt = gameFrame + 2 }
end

local function pulse(button, cb)
	pad.setOverride(button)
	active = { button = button, mode = 'pulse', cb = cb, stage = 1,
		releaseAt = gameFrame + 14, doneAt = gameFrame + 28 }
end

local function waitUntil(pred, label, cb, timeout)
	active = { button = nil, mode = 'until', pred = pred, label = label, cb = cb,
		doneAt = gameFrame + (timeout or 400) }
end

local function finish()
	print(string.format('OGRE_PROGRESSION_CAPTURE: PASS boss=%d stage=%d dlg=%d coffre=%d hp=%d hits=%d steps=%d shots=%s zone=%d gates=%d',
		bossDown(), stage(), dialogueId(), chestLettre(), duelHp(), duelHits(),
		duelSteps(), table.concat(captured, ','), zone(), gates()))
	PCSX.quit(0)
end

-- C) Apres la victoire : CROIX ferme le dialogue, puis ouvre le coffre.
local function openChest()
	teleport(540, 484, 3)
	waitFrames(10, function()
		tap(B.CROSS, function()
			waitFrames(6, function()
				if chestLettre() ~= 1 then
					fail(string.format('coffre lettre non ouvert: coffre=%d dlg=%d boss=%d',
						chestLettre(), dialogueId(), bossDown()))
					return
				end
				if dialogueId() ~= 11 then
					fail('dialogue objet_lettre absent: dlg=' .. dialogueId())
					return
				end
				if bossDown() ~= 1 or stage() ~= 6 then
					fail('progression perdue apres coffre')
					return
				end
				shot('lettre_apres_combat')
				finish()
			end)
		end)
	end)
end

local function closeVictoryDialogue(n)
	if not dlgIsActive() then
		print(string.format('CONFIRM_APRES_VICTOIRE boss=%d stage=%d dlg=%d coffre=%d panel=%d',
			bossDown(), stage(), dialogueId(), chestLettre(), dlgPanel()))
		if bossDown() ~= 1 or stage() ~= 6 or chestLettre() ~= 0 then
			fail('recompense double apres confirmation CROIX')
			return
		end
		openChest()
		return
	end
	if n <= 0 then fail('dialogue victoire non ferme, active=' .. tostring(dlgIsActive()) .. ' panel=' .. dlgPanel()) return end
	-- dlgUpdate() confirme avec START (pas CROIX) : voir dialogue_ps1.cpp:108.
	tap(B.START, function()
		waitFrames(6, function() closeVictoryDialogue(n - 1) end)
	end)
end

-- B) La consequence de quete suit la transition reelle vers DOWN.
local function verifyVictory()
	if duelHp() ~= 0 or duelPhase() ~= 3 or duelHits() ~= 7 then
		fail(string.format('duel non termine hp=%d phase=%d hits=%d',
			duelHp(), duelPhase(), duelHits()))
		return
	end
	if bossDown() ~= 1 or stage() ~= 6 or dialogueId() ~= 10 or chestLettre() ~= 0 then
		fail(string.format('victoire sans consequence boss=%d stage=%d dlg=%d coffre=%d',
			bossDown(), stage(), dialogueId(), chestLettre()))
		return
	end
	print(string.format('VICTOIRE_REELLE hp=%d hits=%d steps=%d boss=%d stage=%d dlg=%d coffre=%d',
		duelHp(), duelHits(), duelSteps(), bossDown(), stage(), dialogueId(), chestLettre()))
	shot('victoire_apres_combat')
	waitUntil(function() return dlgIsActive() end, 'dialogue victoire ouvert',
		function() waitFrames(2, function() closeVictoryDialogue(12) end) end, 120)
end

local function landNext()
	waitUntil(function() return duelPhase() == 1 and duelCooldown() == 0 end,
		'OUVERT', function()
			tap(B.SQUARE, function()
				if duelHp() == 0 then
					waitFrames(6, verifyVictory)
				else
					landNext()
				end
			end)
		end)
end

local function runDuel()
	waitUntil(function() return duelPhase() == 0 end, 'GARDE', function()
		tap(B.SQUARE, function()
			if duelHp() ~= 14 or duelHits() ~= 0 then
				fail('coup en garde a retire des PV')
				return
			end
			landNext()
		end)
	end)
end

-- A) CROIX avant la victoire : aucun contournement, aucun dialogue, aucun lot.
local function ogreAvant()
	write32(questAddr + 56, 13)
	teleport(540, 450, 3)
	waitFrames(20, function()
		if zone() ~= 13 then fail('zone QuestState 013 non posee') return end
		if enemyFrames() ~= 6 then fail('blob P1AN non initialize') return end
		if bossDown() ~= 0 or stage() ~= 0 or chestLettre() ~= 0 then
			fail('etat initial inattendu')
			return
		end
		tap(B.CROSS, function()
			waitFrames(6, function()
				if bossDown() ~= 0 or stage() ~= 0 or chestLettre() ~= 0 or dialogueId() ~= 0xffffffff then
					fail(string.format('CROIX a contourne le combat boss=%d stage=%d coffre=%d dlg=%d',
						bossDown(), stage(), chestLettre(), dialogueId()))
					return
				end
				print(string.format('CROIX_AVANT_VICTOIRE_SANS_EFFET boss=%d stage=%d coffre=%d dlg=%d',
					bossDown(), stage(), chestLettre(), dialogueId()))
				runDuel()
			end)
		end)
	end)
end

-- Doit rester GLOBAL : une table locale est collectee par le GC Lua et les
-- breakpoints sont alors retires sans erreur (defaut deja rencontre).
bps = {}
local function addBp(addr, label, cb)
	if addr == 0 then return end
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, label, cb, label)
end
addBp(menuDrawAddr, 'progression: menuDrawPS1', function() menuDraws = menuDraws + 1 return true end)
addBp(introDrawAddr, 'progression: introDrawPS1', function() introDraws = introDraws + 1 return true end)
addBp(mainAddr, 'progression: main', function()
	started = true
	gameFrame = 0
	local r = PCSX.getRegisters()
	print(string.format('CAPTURE main reached pc=%08x sp=%08x', r.pc, r.GPR.n.sp))
	return false
end, 'progression: main')

listener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not started then return end
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
				fail('condition non observee: ' .. active.label ..
					' hp=' .. duelHp() .. ' phase=' .. duelPhase() ..
					' shown=' .. dlgShown())
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
	elseif phase == 'game' and gameFrame >= phaseFrame + 40 and
	       enemyFrames() == 6 and duelSteps() > 0 then
		phase = 'ogre'
		waitFrames(20, ogreAvant)
	end
	if gameFrame > 20000 then fail('timeout phase=' .. phase) end
end)

print('OGRE_PROGRESSION ready main=0x' .. string.format('%08x', mainAddr) ..
	' quest=0x' .. string.format('%08x', questAddr) ..
	' duel=0x' .. string.format('%08x', duelBase) .. ' outdir=' .. outdir)
