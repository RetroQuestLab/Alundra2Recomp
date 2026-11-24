-- Capture native de l'animation de l'ogre de Kindra (v0.8.8).
-- Navigation reprise telle quelle de capture_boss.lua (jalon prouve) ; seule
-- la sequence devant l'ogre est remplacée par trois captures de frames du cycle
-- g_ps1EnemyAnim. Aucun combat, dialogue, gate ni etat de quete n'est modifie.
--
-- Variables d'environnement : SESSION_OUT, PS1_MAIN_ADDR, PS1_HERO_X_ADDR,
-- PS1_HERO_Y_ADDR, PS1_DIR_ADDR, PS1_QUEST_ADDR, PS1_INTRO_ADDR,
-- PS1_DLG_SHOWN_ADDR, PS1_DLG_PANEL_ADDR, PS1_ENEMY_ADDR.

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
local menuDrawAddr = tonumber(os.getenv('PS1_MENU_DRAW_ADDR') or '0')
local introDrawAddr = tonumber(os.getenv('PS1_INTRO_DRAW_ADDR') or '0')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR') or '0')
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
local function zone() return read32(questAddr + 56) end
local function stage() return read32(questAddr) end
local function bossDown() return read32(questAddr + 48) end
local function introFrame() return read32(introAddr) end
local function dlgShown() return read32(shownAddr) end
local function dlgPanel() return read32(panelAddr) end
-- PS1EnemyAnimState : frame(u16) tick(u16) frames(u16) frozen(u16) steps(u32)
local function enemyFrame() return bit.band(read32(enemyAddr), 0xffff) end
local function enemyFrames() return bit.band(read32(enemyAddr + 4), 0xffff) end
local function enemyFrozen() return bit.band(bit.rshift(read32(enemyAddr + 4), 16), 0xffff) end
local function enemySteps() return read32(enemyAddr + 8) end

local function teleport(x, y)
	write32(heroXAddr, x * 65536)
	write32(heroYAddr, y * 65536)
	write32(dirAddr, 2)
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
		local r = bit.band(c, 31) * 8
		local g = bit.band(bit.rshift(c, 5), 31) * 8
		local b = bit.band(bit.rshift(c, 10), 31) * 8
		px[#px + 1] = string.char(r, g, b)
	end
	local file = io.open(outdir .. '/' .. name .. '.ppm', 'wb')
	file:write(string.format('P6\n%d %d\n255\n', w, h))
	file:write(table.concat(px))
	file:close()
	print(string.format('shot %s %dx%d frame=%d hero=(%d,%d) zone=%d stage=%d boss=%d enemy=(frame=%d,frames=%d,frozen=%d,steps=%d)',
		name, w, h, gameFrame, heroX(), heroY(), zone(), stage(), bossDown(),
		enemyFrame(), enemyFrames(), enemyFrozen(), enemySteps()))
end

local function fail(message)
	print('ENEMY_CAPTURE: FAIL ' .. message)
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

local function waitEnemyFrame(frame, name, cb)
	active = { button = nil, mode = 'frame', frame = frame, name = name, cb = cb,
		doneAt = gameFrame + 240 }
end

local function finish()
	if #captured < 3 then fail('frames capturees insuffisantes : ' .. #captured) return end
	if enemySteps() < 5 then fail('sequence insuffisante, steps=' .. enemySteps()) return end
	if zone() ~= 13 or bossDown() ~= 0 or stage() ~= 0 or dlgShown() ~= 0 then
		fail('etat de quete modifie : zone=' .. zone() .. ' boss=' .. bossDown()
			.. ' stage=' .. stage() .. ' shown=' .. dlgShown())
		return
	end
	print(string.format('ENEMY_CAPTURE: PASS frames=%s steps=%d zone=%d stage=%d boss=%d shown=%d',
		table.concat(captured, ','), enemySteps(), zone(), stage(), bossDown(), dlgShown()))
	PCSX.quit(0)
end

local function captureThree()
	waitEnemyFrame(1, 'enemy_frame_1', function()
		captured[#captured + 1] = 1
		waitEnemyFrame(3, 'enemy_frame_3', function()
			captured[#captured + 1] = 3
			waitEnemyFrame(5, 'enemy_frame_5', function()
				captured[#captured + 1] = 5
				waitFrames(8, finish)
			end)
		end)
	end)
end

-- Harnais de capture : comme capture_boss.lua ecrit la position du heros en RAM,
-- on ecrit ici la zone de QuestState pour placer l'acteur ogre devant la caméra
-- sans traverser la route A->Z. Aucun fichier de jeu ni etat sauvegarde n'est
-- modifie ; le rendu, lui, est bien celui de la boucle PS1 reelle.
local function ogreAvant()
	write32(questAddr + 56, 13)
	teleport(520, 472)
	waitFrames(20, function()
		if zone() ~= 13 then fail('zone QuestState 013 non posee, lue ' .. zone()) return end
		if enemyFrames() ~= 6 then fail('blob P1AN non initialize, frames=' .. enemyFrames()) return end
		if bossDown() ~= 0 then fail('ogre deja vaincu avant capture') return end
		shot('enemy_zone_entree')
		captureThree()
	end)
end

bps = {}
local function addBp(addr, label, cb)
	if addr == 0 then return end
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, label, cb, label)
end
addBp(menuDrawAddr, 'capture enemy: menuDrawPS1', function() menuDraws = menuDraws + 1 return true end)
addBp(introDrawAddr, 'capture enemy: introDrawPS1', function() introDraws = introDraws + 1 return true end)

bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'capture enemy: main reached',
	function()
		started = true
		gameFrame = 0
		local r = PCSX.getRegisters()
		print(string.format('CAPTURE main reached pc=%08x ra=%08x sp=%08x gp=%08x',
			r.pc, r.GPR.n.ra, r.GPR.n.sp, r.GPR.n.gp))
		return false
	end,
	'capture enemy: main reached')

listener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not started then return end
	gameFrame = gameFrame + 1

	if gameFrame % 120 == 0 then
		print(string.format('WAIT phase=%s hero=(%d,%d) zone=%d stage=%d dlg=%d intro=%d addr=0x%x scene=%d skip=%d frame=%d steps=%d',
			phase, heroX(), heroY(), zone(), stage(), dlgShown(), introFrame(), introAddr,
			read32(introAddr + 4), read32(introAddr + 8), enemyFrame(), enemySteps()))
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
		if active.mode == 'frame' then
			if enemyFrame() == active.frame then
				local name, cb = active.name, active.cb
				active = nil
				shot(name)
				if cb then cb() end
			elseif gameFrame >= active.doneAt then
				fail('frame ' .. active.frame .. ' non observee (lue ' .. enemyFrame() .. ')')
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
		pulse(B.START, function() end)  -- saute l'intro comme mix_probe.lua
	elseif phase == 'game' and gameFrame >= phaseFrame + 40 and enemyFrames() == 6 then
		phase = 'ogre'
		waitFrames(20, ogreAvant)
	end

	if gameFrame > 20000 then
		fail('timeout phase=' .. phase .. ' zone=' .. zone() .. ' frame=' .. enemyFrame())
	end
end)

print('CAPTURE_ENEMY ready, main=0x' .. string.format('%08x', mainAddr) ..
	' enemy=0x' .. string.format('%08x', enemyAddr) .. ' outdir=' .. outdir)
