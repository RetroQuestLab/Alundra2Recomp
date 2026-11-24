-- Round-trip de sauvegarde PS1 : ecriture apres coupure, puis relecture.
-- PS1_SAVE_PHASE = write | load. SESSION_OUT, PS1_MAIN_ADDR.
-- Navigation pilotee par l'etat observe (rendus menu/intro) : le pre-remplissage
-- CD decale le demarrage du titre d'environ 120 images, les anciennes images
-- fixes tiraient a cote depuis l'ajout du streaming musical.

local outdir = os.getenv('SESSION_OUT') or '.'
local phase = os.getenv('PS1_SAVE_PHASE') or 'write'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x80017070')
local menuDrawAddr = tonumber(os.getenv('PS1_MENU_DRAW_ADDR') or '0')
local introDrawAddr = tonumber(os.getenv('PS1_INTRO_DRAW_ADDR') or '0')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR') or '0')
local questAddr = tonumber(os.getenv('PS1_QUEST_ADDR') or '0')
local invStateAddr = tonumber(os.getenv('PS1_INV_STATE_ADDR') or '0')
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local mem = PCSX.getMemPtr()
local gameFrame = 0
local started = false
local active = nil
local step = 0
local menuDraws = 0
local introDraws = 0
local gamePhaseFrame = 0
local debug = os.getenv('PS1_SAVE_DEBUG') == '1'

local function read32(addr)
	local off = addr - 0x80000000
	return mem[off] + mem[off + 1] * 256 + mem[off + 2] * 65536 + mem[off + 3] * 16777216
end

local function read8(addr)
	return mem[addr - 0x80000000]
end

-- Empreinte FNV-1a des champs POD réellement sérialisés par save_ps1.cpp :
-- QuestState hors prevPad/gates (remis à zéro ou pilotés par le harnais),
-- puis les 64 paires def/qty et les 3 emplacements d'équipement de PSIInventory.
-- Aucune écriture du harnais dans ces états : lecture seule.
local function stateDigest()
	if questAddr == 0 or invStateAddr == 0 then return '' end
	local h = 2166136261
	local function byte(b)
		h = math.fmod(h * 16777619, 4294967296)
		h = bit.bxor(h, b)
	end
	for _, o in ipairs({0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44,
	                    48, 52, 56, 68, 72, 76, 80, 84, 88, 92}) do
		for i = 0, 3 do byte(read8(questAddr + o + i)) end
	end
	for i = 0, 63 do
		local e = invStateAddr + 4 * i
		byte(read8(e))          -- def
		byte(read8(e + 2))      -- qty bas
		byte(read8(e + 3))      -- qty haut
	end
	for _, o in ipairs({256, 257, 258}) do byte(read8(invStateAddr + o)) end
	return string.format('%08x', h)
end

-- Marqueur capture-only : une quantité possédée passe à 7 avant la sauvegarde.
-- Sans cette mutation, l'état sauvegardé serait identique au STATE_INIT et la
-- comparaison après relecture ne prouverait rien. Aucun code du jeu modifié.
local function markInventory()
	if invStateAddr == 0 then return -1 end
	for i = 0, 63 do
		local e = invStateAddr + 4 * i
		if read8(e) ~= 0xFF then
			local off = e - 0x80000000
			mem[off + 2] = 7
			mem[off + 3] = 0
			return i
		end
	end
	return -1
end

local function pulse(button, cb)
	pad.setOverride(button)
	active = { button = button, cb = cb, stage = 1,
		releaseAt = gameFrame + 14, doneAt = gameFrame + 28 }
end

local function fail(message)
	print('SAVE_ROUNDTRIP: FAIL ' .. message)
	PCSX.quit(1)
end

-- Le breakpoint doit rester reference par une globale : un objet Lua non
-- reference est collecte silencieusement et ne se declenche jamais (voir
-- capture_inventory.lua et notes.md pour le piege identique du listener VSync).
bps = {}
local function addBp(addr, label, callback)
	if addr == 0 then return end
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, label, callback, label)
end

addBp(mainAddr, 'save roundtrip: main reached', function()
	started = true
	gameFrame = 0
	print('SAVE_ROUNDTRIP main reached')
	return false
end)

-- Les callbacks de comptage DOIVENT retourner true, sinon PCSX desactive le
-- breakpoint apres le premier hit (cf. capture_inventory.lua).
addBp(menuDrawAddr, 'save roundtrip: menuDraw', function()
	menuDraws = menuDraws + 1
	return true
end)
addBp(introDrawAddr, 'save roundtrip: introDraw', function()
	introDraws = introDraws + 1
	return true
end)

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not started then return end
	gameFrame = gameFrame + 1

	if active then
		if active.stage == 1 and gameFrame >= active.releaseAt then
			pad.clearOverride(active.button)
			active.stage = 2
		elseif active.stage == 2 and gameFrame >= active.doneAt then
			local cb = active.cb
			active = nil
			step = step + 1
			gamePhaseFrame = gameFrame
			if cb then cb() end
		end
		return
	end

	if phase == 'write' then
		if step == 0 and menuDraws >= 2 then
			pulse(B.START)
		elseif step == 1 and read32(menuAddr) == 1 then
			pulse(B.CROSS)
		elseif step == 2 and introDraws >= 2 then
			pulse(B.START)  -- saute l'intro
		elseif step == 3 and gameFrame >= gamePhaseFrame + 90 then
			if debug then print('SAVE_DEBUG pre-mark frame=' .. gameFrame ..
				' quest=' .. string.format('%08x', questAddr) ..
				' inv=' .. string.format('%08x', invStateAddr)) end
			local slot = markInventory()
			if debug then print('SAVE_DEBUG post-mark slot=' .. slot) end
			print(string.format('SAVE_STATE phase=write digest=%s mark=%d stage=%d zone=%d',
				stateDigest(), slot,
				questAddr == 0 and -1 or read32(questAddr),
				questAddr == 0 and -1 or read32(questAddr + 56)))
			pulse(B.SELECT)
		elseif step == 4 and gameFrame >= gamePhaseFrame + 180 then
			print('SAVE_ROUNDTRIP: WRITE SENT frame=' .. gameFrame)
			PCSX.quit(0)
		end
	elseif phase == 'load' then
		-- La detection carte est asynchrone (src/ps1/boot.cpp:139) : il faut
		-- attendre hasSave==1 avant d'ouvrir le menu, sinon "Continuer"
		-- n'existe pas encore et CROIX lance "Nouvelle partie".
		-- Depuis le menu avec sauvegarde, "Continuer" est l'entree 0
		-- (src/ps1/menu_ps1_logic.h::menuMainLabel) : CROIX directe.
		if step == 0 and menuDraws >= 2 and read32(menuAddr + 20) == 1 then
			pulse(B.START)
		elseif step == 1 and read32(menuAddr) == 1 then
			if debug then
				print(string.format('SAVE_DEBUG menu screen=%d cursor=%d hasSave=%d frame=%d',
					read32(menuAddr), read32(menuAddr + 4), read32(menuAddr + 20), gameFrame))
			end
			pulse(B.CROSS)
		elseif step == 2 and gameFrame >= gamePhaseFrame + 180 then
			print(string.format('SAVE_STATE phase=load digest=%s stage=%d zone=%d',
				stateDigest(),
				questAddr == 0 and -1 or read32(questAddr),
				questAddr == 0 and -1 or read32(questAddr + 56)))
			if debug then
				print(string.format('SAVE_DEBUG action=%d screen=%d frame=%d',
					read32(menuAddr + 36), read32(menuAddr), gameFrame))
			end
			print('SAVE_ROUNDTRIP: LOAD SENT frame=' .. gameFrame)
			PCSX.quit(0)
		end
	else
		fail('phase inconnue ' .. phase)
	end

	if gameFrame > 2400 then fail('timeout phase=' .. phase .. ' step=' .. step) end
end)

print('SAVE_ROUNDTRIP ready phase=' .. phase .. ' main=0x' ..
	string.format('%08x', mainAddr) .. ' outdir=' .. outdir)
