-- Capture native de la suite A→Z PS1 : 011 Paco → 013 Kindra → 019 Epave → 119 Toroledo.
--
-- Le harnais ecrit g_x/g_y/g_dir en RAM emulee pour placer le heros devant un
-- PNJ ou sur une case de passage : c'est un outil de capture, pas une fonction
-- du jeu. Les changements de zone, eux, sont declenches par la vraie boucle
-- (worldTryWarp dans boot.cpp) et verifies en lisant g_quest.zone.
--
-- Variables d'environnement : SESSION_OUT, PS1_MAIN_ADDR, PS1_HERO_X_ADDR,
-- PS1_HERO_Y_ADDR, PS1_DIR_ADDR, PS1_QUEST_ADDR, PS1_INTRO_ADDR,
-- PS1_DLG_SHOWN_ADDR, PS1_DLG_PANEL_ADDR.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x80015600')
local heroXAddr = tonumber(os.getenv('PS1_HERO_X_ADDR') or '0x8008ec28')
local heroYAddr = tonumber(os.getenv('PS1_HERO_Y_ADDR') or '0x8008ec24')
local dirAddr = tonumber(os.getenv('PS1_DIR_ADDR') or '0x8008e5f4')
local questAddr = tonumber(os.getenv('PS1_QUEST_ADDR') or '0x800af3b8')
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
-- QuestState (v0.6.0) : stage 0, dialogueId 8, hasStrangeKey 44,
-- bossDown 48, chestLettre 52, zone 56. L'ancien offset zone=44 etait celui
-- de v0.5.0 ; le patcher ici evite de lire la cle comme une zone.
local function zone() return read32(questAddr + 56) end
local function stage() return read32(questAddr) end
local function dialogueId() return read32(questAddr + 8) end
local function hasKey() return read32(questAddr + 44) end
local function bossDown() return read32(questAddr + 48) end
local function gotLettre() return read32(questAddr + 52) end
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
	print(string.format('shot %s %dx%d frame=%d hero=(%d,%d) zone=%d stage=%d dlg=%d panel=%d shown=%d intro=%d key=%d boss=%d lettre=%d',
		name, w, h, gameFrame, heroX(), heroY(), zone(), stage(), dialogueId(), dlgPanel(),
		dlgShown(), introFrame(), hasKey(), bossDown(), gotLettre()))
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
	print('ZONE_CAPTURE: FAIL ' .. message)
	PCSX.quit(1)
end

-- Le dialogue se termine quand g_quest.dialogueId repasse a -1 (0xffffffff).
-- START complete le panneau puis avance ; on borne le nombre d'appuis pour
-- transformer un blocage en echec explicite.
local function closeDialogue(cb, tries)
	tries = tries or 0
	if dialogueId() == 0xffffffff then cb() return end
	if tries > 60 then fail('dialogue non ferme, id=' .. dialogueId()) return end
	pulse(B.START, function()
		waitFrames(6, function() closeDialogue(cb, tries + 1) end)
	end)
end

local function expectZone(expected, name, cb)
	if zone() ~= expected then
		fail('zone attendue ' .. expected .. ', lue ' .. zone() .. ' apres ' .. name)
		return
	end
	shot(name)
	waitFrames(12, cb)
end

local function talk(x, y, name, minStage, cb)
	teleport(x, y)
	waitFrames(8, function()
		pulse(B.CROSS, function()
			if dialogueId() == 0xffffffff then fail('dialogue non ouvert: ' .. name) return end
			waitFrames(24, function()
				shot(name)
				closeDialogue(function()
					if stage() < minStage then
						fail('stage attendu >= ' .. minStage .. ' apres ' .. name .. ', lu ' .. stage())
						return
					end
					waitFrames(8, cb)
				end)
			end)
		end)
	end)
end

-- Sequence complete : une seule zone active a la fois, memes blobs que l'ISO.
-- Ordre de definition bottom-up : en Lua, une closure ne voit que les locaux
-- deja declares au moment ou elle est creee.
local function toledo()
	expectZone(119, 'toledo_arrivee', function()
		talk(238, 552, 'toledo_natasha', 5, function()
			shot('toledo_quete')
			waitFrames(8, function()
				print(string.format('ZONE_CAPTURE: PASS zones=11,13,19,119 stage=%d hero=(%d,%d)',
					stage(), heroX(), heroY()))
				PCSX.quit(0)
			end)
		end)
	end)
end

local function epave()
	expectZone(19, 'epave_arrivee', function()
		talk(238, 184, 'epave_alexia', 4, function()
			-- Passage 019 (14,52) -> 119 (25,50).
			teleport(348, 840)
			waitFrames(14, function() toledo() end)
		end)
	end)
end

local function kindra()
	-- Passage 011 (25..27,45) -> 013 (8,54), declenche par la boucle de jeu.
	teleport(660, 728)
	waitFrames(14, function()
		expectZone(13, 'kindra_arrivee', function()
			talk(982, 888, 'kindra_finrod', 3, function()
				-- Passage 013 (41,53) -> 019 (10,12).
				teleport(996, 856)
				waitFrames(14, function() epave() end)
			end)
		end)
	end)
end

bps = {}
bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'capture zones: main reached',
	function()
		started = true
		gameFrame = 0
		local r = PCSX.getRegisters()
		print(string.format('CAPTURE main reached pc=%08x ra=%08x sp=%08x gp=%08x',
			r.pc, r.GPR.n.ra, r.GPR.n.sp, r.GPR.n.gp))
		return false
	end,
	'capture zones: main reached')

-- Reference GLOBALE : une locale de chunk peut etre collectee apres le
-- chargement, et le listener VSync disparait alors sans erreur Lua.
listener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not started then return end
	gameFrame = gameFrame + 1

	if gameFrame % 120 == 0 then
		print(string.format('WAIT phase=%s hero=(%d,%d) zone=%d stage=%d dlg=%d',
			phase, heroX(), heroY(), zone(), stage(), dialogueId()))
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
		phase = 'zones'
		pulse(B.START, function()
			-- Le triple buffering peut encore presenter la derniere frame d'intro
			-- juste apres la validation : attendre 30 frames avant la capture.
			waitFrames(30, function()
				shot('paco_depart')
				waitFrames(12, kindra)
			end)
		end)
	end

	if gameFrame > 20000 then
		fail('timeout phase=' .. phase .. ' zone=' .. zone() .. ' hero=(' ..
			heroX() .. ',' .. heroY() .. ') stage=' .. stage())
	end
end)

print('CAPTURE_ZONES ready, main=0x' .. string.format('%08x', mainAddr) ..
	' quest=0x' .. string.format('%08x', questAddr) .. ' outdir=' .. outdir)
