-- Capture native du jalon PS1 v0.6.0b : salle du trone 120, chaine
-- mephisto_arrivee -> mephisto_trone -> mephisto_vaincu -> vraie fin.
--
-- Le harnais ecrit les gates dans QuestState et simule la zone 086 le temps de
-- franchir worldTryWarp120(). Aucun gate PC n'est modifie : la valeur 0x1F est
-- posee uniquement en RAM emulee, exactement comme deplacement du heros.
--
-- Variables d'environnement fournies par tools/ps1_capture_zones.ps1 :
-- SESSION_OUT, PS1_MAIN_ADDR, PS1_HERO_X_ADDR, PS1_HERO_Y_ADDR, PS1_DIR_ADDR,
-- PS1_QUEST_ADDR, PS1_CAPTURE_ZONE_ADDR, PS1_INTRO_ADDR,
-- PS1_DLG_SHOWN_ADDR, PS1_DLG_PANEL_ADDR.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x80015600')
local heroXAddr = tonumber(os.getenv('PS1_HERO_X_ADDR') or '0x8008ec28')
local heroYAddr = tonumber(os.getenv('PS1_HERO_Y_ADDR') or '0x8008ec24')
local dirAddr = tonumber(os.getenv('PS1_DIR_ADDR') or '0x8008e5f4')
local questAddr = tonumber(os.getenv('PS1_QUEST_ADDR') or '0x800af3b8')
local captureZoneAddr = tonumber(os.getenv('PS1_CAPTURE_ZONE_ADDR') or '0x0')
local introAddr = tonumber(os.getenv('PS1_INTRO_ADDR') or '0x800af3fc')
local shownAddr = tonumber(os.getenv('PS1_DLG_SHOWN_ADDR') or '0x8008ec58')
local panelAddr = tonumber(os.getenv('PS1_DLG_PANEL_ADDR') or '0x8008ec5c')
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local mem = PCSX.getMemPtr()
local gameFrame = 0
local started = false
local phase = 'title'
local active = nil

-- QuestState : zone=56, gates=64, mephistoDown=68, ending=72.
local QST_ZONE = 56
local QST_GATES = 64
local QST_MEPHISTO_DOWN = 68
local QST_ENDING = 72
local QST_GATE_TRONE_MASK = 0x1F

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
local function zone() return read32(questAddr + QST_ZONE) end
local function stage() return read32(questAddr) end
local function dialogueId() return read32(questAddr + 8) end
local function gates() return read32(questAddr + QST_GATES) end
local function mephistoDown() return read32(questAddr + QST_MEPHISTO_DOWN) end
local function ending() return read32(questAddr + QST_ENDING) end
local function introFrame() return read32(introAddr) end
local function dlgShown() return read32(shownAddr) end
local function dlgPanel() return read32(panelAddr) end

local function teleport(x, y)
	write32(heroXAddr, x * 65536)
	write32(heroYAddr, y * 65536)
	write32(dirAddr, 2)  -- PF_DIR_RIGHT : le point d'interaction est devant le heros
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
	print(string.format('shot %s %dx%d frame=%d hero=(%d,%d) zone=%d stage=%d dlg=%d panel=%d shown=%d gates=0x%x mephisto=%d ending=%d',
		name, w, h, gameFrame, heroX(), heroY(), zone(), stage(), dialogueId(),
		dlgPanel(), dlgShown(), gates(), mephistoDown(), ending()))
end

local function waitFrames(frames, cb)
	active = { button = nil, mode = 'wait', cb = cb, doneAt = gameFrame + frames }
end

local function pulse(button, cb)
	pad.setOverride(button)
	active = { button = button, mode = 'pulse', cb = cb, stage = 1,
		releaseAt = gameFrame + 14, doneAt = gameFrame + 28 }
end

local function fail(message)
	print('ENDING_CAPTURE: FAIL ' .. message)
	PCSX.quit(1)
end

-- Une page entiere met au plus 55 frames a se taper (2 glyphes/frame).
local PANEL_FRAMES = 70

local function closeDialogue(cb, tries)
	tries = tries or 0
	if dialogueId() == 0xffffffff then cb() return end
	if tries > 60 then fail('dialogue non ferme, id=' .. dialogueId()) return end
	pulse(B.START, function()
		waitFrames(6, function() closeDialogue(cb, tries + 1) end)
	end)
end

-- Capture un dialogue deja ouvert (l'arrivee est declenchee par interactionSetZonePS1).
local function captureOpenDialogue(name, pages, cb)
	local function pageStep(page)
		waitFrames(PANEL_FRAMES, function()
			shot(page == 1 and name or (name .. '_p' .. page))
			if page >= pages then
				closeDialogue(cb)
				return
			end
			pulse(B.START, function()
				waitFrames(8, function()
					if dlgPanel() ~= page then
						fail('page ' .. page .. ' non avancee pour ' .. name
							.. ', lue ' .. dlgPanel())
						return
					end
					pageStep(page + 1)
				end)
			end)
		end)
	end
	pageStep(1)
end

-- Approche d'un acteur, CROIX, puis capture de toutes ses pages.
local function interactPages(x, y, name, pages, cb)
	teleport(x, y)
	waitFrames(8, function()
		pulse(B.CROSS, function()
			if dialogueId() == 0xffffffff then fail('interaction non ouverte: ' .. name) return end
			captureOpenDialogue(name, pages, cb)
		end)
	end)
end

local function finish()
	if ending() ~= 1 then fail('mode ending non atteint') return end
	if stage() < 9 then fail('stage fin attendu, lu ' .. stage()) return end
	if mephistoDown() ~= 1 then fail('mephisto_vaincu absent') return end
	if gates() ~= QST_GATE_TRONE_MASK then
		fail('gates modifies par la quete : 0x' .. string.format('%x', gates()))
		return
	end
	print(string.format('ENDING_CAPTURE: PASS stage=%d gates=0x%x mephisto=%d ending=%d hero=(%d,%d)',
		stage(), gates(), mephistoDown(), ending(), heroX(), heroY()))
	PCSX.quit(0)
end

-- Derniere replique : la fermeture de [alexia_finale_mephisto_vaincu] allume
-- QuestState::ending et la boucle affiche la fenetre A1 six lignes.
local function alexiaFinale()
	interactPages(310, 840, 'ending_alexia_vaincu', 2, function()
		waitFrames(40, function()
			shot('ending_screen')
			finish()
		end)
	end)
end

local function roiVaincu()
	interactPages(310, 680, 'ending_roi_vaincu', 2, function()
		if ending() ~= 0 then fail('fin declenchee par le roi') return end
		waitFrames(8, alexiaFinale)
	end)
end

local function mephistoVaincu()
	interactPages(478, 424, 'ending_mephisto_vaincu', 2, function()
		if mephistoDown() ~= 1 then fail('mephisto_vaincu non pose') return end
		if ending() ~= 0 then fail('fin declenchee avant Alexia') return end
		waitFrames(8, roiVaincu)
	end)
end

local function mephistoTrone()
	interactPages(478, 424, 'ending_mephisto_trone', 3, function()
		waitFrames(8, mephistoVaincu)
	end)
end

local function salleTrone()
	shot('ending_salle_trone')
	waitFrames(12, mephistoTrone)
end

-- Le dialogue d'arrivee est ouvert des l'entree en zone 120, avant tout CROIX.
local function arriveeTrone()
	if zone() ~= 120 then
		fail('zone 120 attendue apres warp, lue ' .. zone())
		return
	end
	if dialogueId() == 0xffffffff then
		fail('dialogue d arrivee non ouvert')
		return
	end
	captureOpenDialogue('ending_arrivee_trone', 4, function()
		if mephistoDown() ~= 0 then fail('mephisto deja vaincu a l arrivee') return end
		waitFrames(12, salleTrone)
	end)
end

local function warpTrone()
	if captureZoneAddr == 0 then
		fail('PS1_CAPTURE_ZONE_ADDR absent de l ELF')
		return
	end
	write32(questAddr + QST_GATES, QST_GATE_TRONE_MASK)
	write32(captureZoneAddr, 86)
	-- 086 (26,8) : case de depart du passage conditionne vers 120 (14,52).
	teleport(26 * 24 + 12, 8 * 16 + 8)
	waitFrames(20, arriveeTrone)
end

bps = {}
bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'capture ending: main reached',
	function()
		started = true
		gameFrame = 0
		local r = PCSX.getRegisters()
		print(string.format('CAPTURE main reached pc=%08x ra=%08x sp=%08x gp=%08x',
			r.pc, r.GPR.n.ra, r.GPR.n.sp, r.GPR.n.gp))
		return false
	end,
	'capture ending: main reached')

-- Reference GLOBALE : une locale de chunk peut etre collectee apres chargement.
listener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not started then return end
	gameFrame = gameFrame + 1

	if gameFrame % 120 == 0 then
		print(string.format('WAIT phase=%s hero=(%d,%d) zone=%d stage=%d dlg=%d gates=0x%x ending=%d',
			phase, heroX(), heroY(), zone(), stage(), dialogueId(), gates(), ending()))
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
		phase = 'ending'
		pulse(B.START, function()
			waitFrames(30, warpTrone)
		end)
	end

	if gameFrame > 20000 then
		fail('timeout phase=' .. phase .. ' zone=' .. zone() .. ' hero=(' ..
			heroX() .. ',' .. heroY() .. ') stage=' .. stage())
	end
end)

print('CAPTURE_ENDING ready, main=0x' .. string.format('%08x', mainAddr) ..
	' quest=0x' .. string.format('%08x', questAddr) ..
	' captureZone=0x' .. string.format('%08x', captureZoneAddr) ..
	' outdir=' .. outdir)
