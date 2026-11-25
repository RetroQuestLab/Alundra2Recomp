-- Parcours "ordres de Diaz -> epee de Jeehan -> Vallee de Mithra" (v0.8.26).
-- La sonde ne fait que PLACER le heros (x, y, direction) sur des cases reelles
-- et appuyer sur la manette emulee. Elle n'ecrit AUCUN champ QuestState ni
-- inventaire : zones, cartes, gates et objets viennent du jeu.
-- Etapes :
--   1. Nouvelle partie, passages reels 011 -> 013 -> 019 -> 119.
--   2. Passage 119 -> 026 SANS l'epee : doit etre refuse.
--   3. Coffre des ordres (CROIX), puis coffre deja vide (CROIX) : un seul objet.
--   4. Jeehan (CROIX) : dialogue 22, gate epee_supreme, +1 objet.
--   5. Passage 119 -> 026 : carte lue depuis le CD, narration 23 une fois.
--   6. SELECT (sauvegarde) -> titre -> Continuer : reprise en 026, carte 026.
-- Env : SESSION_OUT, PS1_MAIN_ADDR, PS1_HERO_X/Y, PS1_DIR_ADDR, PS1_QUEST_ADDR,
-- PS1_DLG_ACTIVE_ADDR, PS1_MENU_DRAW_ADDR, PS1_INTRO_DRAW_ADDR, PS1_MENU_ADDR,
-- PS1_INV_ADDR, PS1_ZONE_W_ADDR.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR'))
local heroXAddr = tonumber(os.getenv('PS1_HERO_X_ADDR'))
local heroYAddr = tonumber(os.getenv('PS1_HERO_Y_ADDR'))
local dirAddr = tonumber(os.getenv('PS1_DIR_ADDR'))
local questAddr = tonumber(os.getenv('PS1_QUEST_ADDR'))
local dlgActiveAddr = tonumber(os.getenv('PS1_DLG_ACTIVE_ADDR'))
local menuDrawAddr = tonumber(os.getenv('PS1_MENU_DRAW_ADDR') or '0')
local introDrawAddr = tonumber(os.getenv('PS1_INTRO_DRAW_ADDR') or '0')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR'))
local invAddr = tonumber(os.getenv('PS1_INV_ADDR'))
local zoneWAddr = tonumber(os.getenv('PS1_ZONE_TILES_ADDR'))  -- g_nTiles
local duelBase = tonumber(os.getenv('PS1_DUEL_ADDR'))
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
local function stage() return read32(questAddr) end
local function dialogueId() return read32(questAddr + 8) end
local function zone() return read32(questAddr + 56) end
local function gates() return read32(questAddr + 64) end
local function zoneW() return read32(zoneWAddr) end
local function duelHp() return bit.band(read32(duelBase), 0xffff) end
local function duelCooldown() return bit.band(bit.rshift(read32(duelBase), 16), 0xffff) end
local function duelPhase() return bit.band(bit.rshift(read32(duelBase + 8), 16), 0xffff) end
local function duelHits() return bit.band(read32(duelBase + 12), 0xffff) end
local function dlgIsActive() return read32(dlgActiveAddr) == 1 end
-- PSIItemEntry = { uint8 def ; int16 qty } (4 octets), 64 entrees, 0xFF = vide.
local function invList()
	local n, total, s = 0, 0, {}
	for i = 0, 63 do
		local off = invAddr - 0x80000000 + i * 4
		local def = mem[off]
		if def ~= 255 then
			local q = mem[off + 2] + mem[off + 3] * 256
			n = n + 1
			total = total + q
			s[#s + 1] = def .. 'x' .. q
		end
	end
	return n, total, table.concat(s, ',')
end

local function state(tag)
	local n, total, list = invList()
	print(string.format('%s zone=%d tuiles=%d stage=%d dlg=%d gates=0x%x inv=%d/%d [%s] hero=%d,%d',
		tag, zone(), zoneW(), stage(), dialogueId(), gates(), n, total, list,
		math.floor(read32(heroXAddr) / 65536), math.floor(read32(heroYAddr) / 65536)))
end

local function teleport(x, y, dir)
	write32(heroXAddr, x * 65536)
	write32(heroYAddr, y * 65536)
	write32(dirAddr, dir)
end
local function tileCenter(cx, cy) return cx * 24 + 12, cy * 16 + 8 end

local function shot(name)
	local ss = PCSX.GPU.takeScreenShot()
	local w, h = ss.width, ss.height
	if ss.data.size ~= w * h * 2 then print('shot ' .. name .. ': skipped') return end
	local p = ffi.cast('const uint16_t*', ss.data.data)
	local px = {}
	for i = 0, w * h - 1 do
		local c = p[i]
		px[#px + 1] = string.char(bit.band(c, 31) * 8,
			bit.band(bit.rshift(c, 5), 31) * 8, bit.band(bit.rshift(c, 10), 31) * 8)
	end
	local file = io.open(outdir .. '/' .. name .. '.ppm', 'wb')
	file:write(string.format('P6\n%d %d\n255\n', w, h))
	file:write(table.concat(px))
	file:close()
	captured[#captured + 1] = name
	print(string.format('shot %s frame=%d zone=%d dlg=%d', name, gameFrame, zone(), dialogueId()))
end

local function fail(message)
	state('JEEHAN_VALLEE_ETAT')
	print('JEEHAN_VALLEE: FAIL ' .. message)
	PCSX.quit(1)
end
local function waitUntil(cond, maxFrames, cb)
	active = { mode = 'until', cond = cond, cb = cb, doneAt = gameFrame + maxFrames }
end
local function waitFrames(frames, cb)
	active = { mode = 'wait', cb = cb, doneAt = gameFrame + frames }
end
local function tap(button, cb, holdFrames)
	pad.setOverride(button)
	active = { button = button, mode = 'tap', cb = cb,
		releaseAt = gameFrame + (holdFrames or 1), doneAt = gameFrame + (holdFrames or 1) + 1 }
end
local function pulse(button, cb)
	pad.setOverride(button)
	active = { button = button, mode = 'pulse', cb = cb, stage = 1,
		releaseAt = gameFrame + 14, doneAt = gameFrame + 28 }
end
local function closeDialogue(n, cb)
	if not dlgIsActive() then waitFrames(4, cb) return end
	if n <= 0 then fail('dialogue non ferme') return end
	tap(B.START, function() waitFrames(6, function() closeDialogue(n - 1, cb) end) end)
end
-- Place le heros sur la case de depart d'un passage et attend le changement.
local function warpVia(cx, cy, want, cb)
	local x, y = tileCenter(cx, cy)
	teleport(x, y, 0)
	local t0 = gameFrame
	waitUntil(function() return zone() == want end, 600, function()
		if zone() ~= want then fail(string.format('passage (%d,%d) -> %d non pris', cx, cy, want)) return end
		print('PASSAGE_IMAGES ' .. want .. ' ' .. (gameFrame - t0))
		state('PASSAGE_' .. want)
		cb()
	end)
end

local inv0 = 0
local w119 = 0

local function reload()
	phase = 'reload_title'
	print('RELOAD SELECT frame=' .. gameFrame)
	pulse(B.SELECT, function() end)
end

local wValley = 0

-- Parler a un PNJ (x, y = point de placement, dir) et verifier le dialogue.
local function talk(x, y, dir, want, tag, cb)
	teleport(x, y, dir)
	waitFrames(10, function() tap(B.CROSS, function() waitFrames(20, function()
		if dialogueId() ~= want then fail(tag .. ': dlg=' .. dialogueId() .. ' attendu=' .. want) return end
		state(tag)
		shot(string.lower(tag))
		closeDialogue(30, cb)
	end) end, 4) end)  -- appui de 4 VSync : zones lourdes < 60 i/s
end

-- Combat reel : CARRE uniquement quand le boss ouvre sa garde.
local function fight(want, flag, tag, cb)
	local fx = math.floor(read32(heroXAddr) / 65536)
	local fy = math.floor(read32(heroYAddr) / 65536)
	local fdir = read32(dirAddr)
	local function hit()
		teleport(fx, fy - 70, fdir)  -- hors de portee pendant la garde
		waitUntil(function() return duelPhase() == 1 and duelCooldown() == 0 end, 900, function()
			if duelPhase() ~= 1 then fail(tag .. ' jamais ouvert phase=' .. duelPhase()) return end
			teleport(fx, fy, fdir)
			tap(B.SQUARE, function()
				if duelHp() ~= 0 then hit() return end
				waitFrames(8, function()
					if dialogueId() ~= want or (flag ~= 0 and bit.band(gates(), flag) == 0) then
						fail(tag .. ' sans consequence dlg=' .. dialogueId()) return
					end
					state(tag)
					waitFrames(12, function() shot(string.lower(tag)) closeDialogue(30, cb) end)
				end)
			end)
		end)
	end
	tap(B.CROSS, function() waitFrames(10, function()
		if dialogueId() ~= 4294967295 or bit.band(gates(), flag) ~= 0 then
			fail('CROIX a contourne ' .. tag) return
		end
		hit()
	end) end)
end

local function enter(cx, cy, want, dlg, tag, cb)
	local before = zoneW()
	local x, y = tileCenter(cx, cy)
	teleport(x, y, 0)
	local t0 = gameFrame
	waitUntil(function() return zone() == want and dlgIsActive() end, 600, function()
		print(tag .. '_CHARGEMENT images=' .. (gameFrame - t0))
		if zone() ~= want then fail(tag .. ' : passage refuse') return end
		if zoneW() == before then fail(tag .. ' : carte non chargee') return end
		if dialogueId() ~= dlg then fail(tag .. ' : dlg=' .. dialogueId()) return end
		state(tag)
		waitFrames(20, function() shot(string.lower(tag)) closeDialogue(30, cb) end)
	end)
end

function finale()
	-- Mephisto (20,26) : premiere replique, puis CROIX ne doit plus rien faire.
	talk(492, 424 - 10, 3, 13, 'MEPHISTO_TRONE', function()
		tap(B.CROSS, function() waitFrames(10, function()
			if read32(questAddr + 68) ~= 0 or dialogueId() ~= 4294967295 then
				fail('CROIX a vaincu Mephisto sans combat') return
			end
			print('MEPHISTO_CROIX_SANS_EFFET hp=' .. duelHp())
			if duelHp() ~= 36 then fail('PV de Mephisto inattendus: ' .. duelHp()) return end
			fight(14, 0, 'MEPHISTO_VAINCU', function()
				if read32(questAddr + 68) ~= 1 then fail('mephistoDown absent') return end
				if read32(questAddr + 72) ~= 0 then fail('fin avant Alexia') return end
				talk(324, 840 - 10, 3, 18, 'ALEXIA_FINALE', function()
					waitFrames(30, function()
						if read32(questAddr + 72) ~= 1 then fail('vraie fin non atteinte') return end
						state('VRAIE_FIN')
						shot('vraie_fin')
						print(string.format('JEEHAN_VALLEE_CAPTURE: PASS shots=%s', table.concat(captured, ',')))
						PCSX.quit(0)
					end)
				end)
			end)
		end) end)
	end)
end

function toManoir()
	enter(44, 28, 86, 40, 'MANOIR_ARRIVEE', function()
		talk(40 * 24 + 12, 30 * 16 + 8 + 10, 0, 42, 'DIAZ_CRISTAL', function()
		talk(25 * 24 + 12, 8 * 16 + 8 + 10, 0, 44, 'ALEXIA_MANOIR', function()
		talk(26 * 24 + 12, 8 * 16 + 8 + 10, 0, 43, 'MILENA', function()
			enter(26, 8, 120, 12, 'TRONE_ARRIVEE', function()
				local tx, ty = tileCenter(14, 48)
				teleport(tx, ty, 0)
				waitFrames(40, function()
					if zone() ~= 120 then fail('rebond hors du trone') return end
					state('TRONE_CARTE')
					shot('trone_carte')
					reload()
				end)
			end)
		end)
		end)
		end)
	end)
end

function toMedusa()
	local wEden = zoneW()
	local x, y = tileCenter(26, 13)
	teleport(x, y, 0)
	local t0 = gameFrame
	waitUntil(function() return zone() == 50 and dlgIsActive() end, 600, function()
		print('MEDUSA_CHARGEMENT images=' .. (gameFrame - t0))
		if zone() ~= 50 then fail('passage vers Puerto Medusa refuse avec le prisme') return end
		if zoneW() == wEden then fail('carte 050 non chargee') return end
		if dialogueId() ~= 34 then fail('narration Medusa absente: dlg=' .. dialogueId()) return end
		state('MEDUSA_ARRIVEE')
		waitFrames(20, function() shot('medusa_arrivee')
		closeDialogue(30, function()
			talk(40 * 24 + 12, 26 * 16 + 8 + 10, 0, 35, 'RUBY', function()
			talk(42 * 24 + 12, 26 * 16 + 8 + 10, 0, 36, 'ALBERT', function()
			talk(16 * 24 + 12, 26 * 16 + 8 + 10, 0, 37, 'ZACH', function()
			talk(47 * 24 + 12, 26 * 16 + 8 + 10, 0, 45, 'ZEPPO', function()
				teleport(34 * 24 + 12, 28 * 16 + 8 - 20, 3)  -- au nord du capitaine
				waitFrames(20, function()
				fight(38, 0x8, 'CAPITAINE_VAINCU', function()
					local n0 = invList()
					talk(34 * 24 + 12, 29 * 16 + 8 - 10, 3, 39, 'CRISTAL_OBTENU', function()
						if invList() ~= n0 + 1 or bit.band(gates(), 0x10) == 0 then fail('cristal non recu') return end
						toManoir()
					end)
				end)
				end)
			end)
			end)
			end)
			end)
		end) end)
	end)
end

function edenContent()
	talk(20 * 24 + 12 - 14, 51 * 16 + 8, 2, 29, 'ROSWELL_LAMPE', function()
	talk(40 * 24 + 12, 44 * 16 + 8 + 10, 0, 31, 'OMEGA', function()
		teleport(612, 468, 3)  -- au nord du second golem (25,30)
		waitFrames(20, function()
		fight(32, 0x2000, 'GOLEM_EDEN_VAINCU', function()
			local n0 = invList()
			talk(636, 494, 3, 33, 'PRISME_OBTENU', function()
				if invList() ~= n0 + 1 or bit.band(gates(), 4) == 0 then fail('prisme non recu') return end
				talk(20 * 24 + 12 - 14, 51 * 16 + 8, 2, 30, 'ROSWELL_PRISME', function()
					toMedusa()
				end)
			end)
		end)
		end)
	end)
	end)
end
function toEden()
	wValley = zoneW()
	local x, y = tileCenter(26, 22)
	teleport(x, y, 0)
	local t0 = gameFrame
	waitUntil(function() return zone() == 42 and dlgIsActive() end, 600, function()
		print('EDEN_CHARGEMENT images=' .. (gameFrame - t0))
		if zone() ~= 42 then fail('passage vers Eden refuse avec la lampe') return end
		if zoneW() == wValley then fail('carte 042 non chargee') return end
		if dialogueId() ~= 27 then fail('narration d Eden absente: dlg=' .. dialogueId()) return end
		state('EDEN_ARRIVEE')
		waitFrames(20, function() shot('eden_arrivee')
		closeDialogue(30, function()
			if bit.band(gates(), 0x1000) == 0 then fail('narration Eden non marquee') return end
			local ex, ey = tileCenter(21, 48)
			teleport(ex, ey, 0)
			waitFrames(40, function()
				if zone() ~= 42 then fail('rebond hors d Eden') return end
				state('EDEN_CARTE')
				shot('eden_carte')
				edenContent()
			end)
		end) end)
	end)
end

local function lampChest()
	local n0 = invList()
	teleport(636, 462, 3)  -- PF_DIR_DOWN : point d'interaction = coffre (26,29)
	waitFrames(10, function() tap(B.CROSS, function() waitFrames(20, function()
		if dialogueId() ~= 26 then fail('coffre de la lampe: dlg=' .. dialogueId()) return end
		if bit.band(gates(), 2) == 0 then fail('gate lampe absent') return end
		state('LAMPE_OBTENUE')
		shot('lampe')
		closeDialogue(30, function()
			if invList() ~= n0 + 1 then fail('lampe non ajoutee a l inventaire') return end
			tap(B.CROSS, function() waitFrames(10, function()
				if dialogueId() ~= 4294967295 or invList() ~= n0 + 1 then fail('coffre lampe rouvert') return end
				state('LAMPE_UNIQUE')
				toEden()
			end) end)
		end)
	end) end) end)
end

local function golemHit()
	waitUntil(function() return duelPhase() == 1 and duelCooldown() == 0 end, 600, function()
		if duelPhase() ~= 1 then fail('golem jamais ouvert phase=' .. duelPhase()) return end
		tap(B.SQUARE, function()
			if duelHp() == 0 then
				waitFrames(8, function()
					if dialogueId() ~= 25 or bit.band(gates(), 0x800) == 0 then
						fail('victoire golem sans consequence dlg=' .. dialogueId()) return
					end
					print(string.format('GOLEM_VAINCU hp=%d hits=%d', duelHp(), duelHits()))
					state('GOLEM_VAINCU')
					waitFrames(12, function() shot('golem_vaincu')
						closeDialogue(30, lampChest) end)
				end)
			else
				golemHit()
			end
		end)
	end)
end

function golem()
	teleport(636, 436, 3)  -- au nord du golem (26,28), face vers le bas
	waitFrames(20, function()
		state('GOLEM_FACE')
		shot('golem')
		tap(B.CROSS, function() waitFrames(10, function()
			if dialogueId() ~= 4294967295 or bit.band(gates(), 0x800) ~= 0 then
				fail('CROIX a contourne le golem dlg=' .. dialogueId()) return
			end
			print('GOLEM_CROIX_SANS_EFFET hp=' .. duelHp())
			golemHit()
		end) end)
	end)
end

local function inValley()
	local n = invList()
	local x, y = tileCenter(26, 44)
	teleport(x, y, 0)
	local t0 = gameFrame
	waitUntil(function() return zone() == 26 and dlgIsActive() end, 600, function()
		print('VALLEE_CHARGEMENT images=' .. (gameFrame - t0))
		if zone() ~= 26 then fail('passage vers la vallee refuse avec l epee') return end
		if zoneW() == w119 or zoneW() == 0 then fail('carte 026 non chargee (w=' .. zoneW() .. ')') return end
		if dialogueId() ~= 23 then fail('narration de la vallee absente: dlg=' .. dialogueId()) return end
		state('VALLEE_ARRIVEE')
		waitFrames(20, function() shot('vallee_arrivee')
		closeDialogue(20, function()
			if bit.band(gates(), 0x200) == 0 then fail('narration non marquee lue') return end
			-- Quitter la case d'arrivee (verrou) sans reprendre le passage retour.
			local vx, vy = tileCenter(26, 49)
			teleport(vx, vy, 0)
			waitFrames(30, function()
				if zone() ~= 26 then fail('rebond hors de la vallee') return end
				state('VALLEE_CARTE')
				shot('vallee_carte')
				teleport(612 - 14, 360, 2)  -- guide (25,22), PF_DIR_RIGHT
				waitFrames(10, function() tap(B.CROSS, function() waitFrames(20, function()
					if dialogueId() ~= 24 then fail('guide de la vallee muet: dlg=' .. dialogueId()) return end
					state('GUIDE_VALLEE')
					shot('guide_vallee')
					closeDialogue(30, function()
						if bit.band(gates(), 0x400) == 0 then fail('guide non marque ecoute') return end
						golem()
					end)
				end) end) end)
			end)
		end) end)
	end)
end

local function talkJeehan()
	local n0 = invList()
	teleport(564 - 14, 200, 2)
	waitFrames(10, function()
		tap(B.CROSS, function() waitFrames(6, function()
			if dialogueId() ~= 22 then fail('Jeehan ne remet pas l epee: dlg=' .. dialogueId()) return end
			if bit.band(gates(), 1) ~= 0 then fail('epee avant la fin du dialogue') return end
			state('JEEHAN_ORDRE_VU')
			shot('jeehan_epee')
			closeDialogue(20, function()
				local n1 = invList()
				if bit.band(gates(), 1) == 0 then fail('gate epee_supreme absent') return end
				if n1 ~= n0 + 1 then fail('epee supreme non ajoutee a l inventaire') return end
				state('EPEE_SUPREME_RECUE')
				-- Reparler a Jeehan : plus de remise ni d'objet.
				tap(B.CROSS, function() waitFrames(6, function()
					closeDialogue(20, function()
						local n2 = invList()
						if n2 ~= n1 then fail('epee remise deux fois') return end
						if dialogueId() ~= 4294967295 then fail('dialogue residuel') return end
						state('JEEHAN_SANS_DOUBLE')
						inValley()
					end)
				end) end)
			end)
		end) end)
	end)
end

local function openChest()
	local x, y = tileCenter(25, 1)
	teleport(x, y + 10, 0)  -- PF_DIR_UP : point d'interaction = centre du coffre
	waitFrames(10, function()
		tap(B.CROSS, function() waitFrames(6, function()
			if dialogueId() ~= 21 then fail('coffre des ordres: dlg=' .. dialogueId()) return end
			state('ORDRES_TROUVES')
			shot('ordres_diaz')
			closeDialogue(20, function()
				local n1 = invList()
				if n1 ~= inv0 + 1 then fail('ordres non ajoutes a l inventaire') return end
				if bit.band(gates(), 0x100) == 0 then fail('drapeau ordres absent') return end
				tap(B.CROSS, function() waitFrames(6, function()
					if dialogueId() ~= 4294967295 or invList() ~= n1 then
						fail('coffre des ordres rouvert') return
					end
					state('COFFRE_VIDE')
					talkJeehan()
				end) end)
			end)
		end) end)
	end)
end

local function refusedWithoutSword()
	w119 = zoneW()
	inv0 = invList()
	local x, y = tileCenter(26, 44)
	teleport(x, y, 0)
	waitFrames(40, function()
		if zone() ~= 119 then fail('vallee ouverte SANS l epee') return end
		print('VALLEE_REFUSEE_SANS_EPEE zone=119')
		openChest()
	end)
end

local function route()
	state('DEPART')
	if zone() ~= 11 then fail('nouvelle partie hors de Paco') return end
	warpVia(25, 45, 13, function()
		warpVia(41, 53, 19, function()
			warpVia(14, 52, 119, refusedWithoutSword)
		end)
	end)
end

bps = {}
local function addBp(addr, label, cb)
	if addr == 0 then return end
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, label, cb, label)
end
addBp(menuDrawAddr, 'jv: menuDrawPS1', function() menuDraws = menuDraws + 1 return true end)
addBp(introDrawAddr, 'jv: introDrawPS1', function() introDraws = introDraws + 1 return true end)
addBp(mainAddr, 'jv: main', function()
	started = true
	gameFrame = 0
	return false
end)

listener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not started then return end
	gameFrame = gameFrame + 1
	if active then
		if active.mode == 'until' then
			if active.cond() or gameFrame >= active.doneAt then
				local cb = active.cb
				active = nil
				if cb then cb() end
			end
			return
		end
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
	end
	if phase == 'title' and menuDraws >= 2 then
		phase = 'menu'
		pulse(B.START, function() end)
	elseif phase == 'menu' and read32(menuAddr) == 1 then
		phase = 'intro'
		phaseFrame = gameFrame
		pulse(B.CROSS, function() end)
	elseif phase == 'intro' and introDraws >= 2 and gameFrame >= phaseFrame + 45 then
		phase = 'game'
		phaseFrame = gameFrame
		pulse(B.START, function() end)
	elseif phase == 'game' and gameFrame >= phaseFrame + 60 then
		phase = 'route'
		route()
	elseif phase == 'reload_title' and read32(menuAddr + 20) == 1 and read32(menuAddr) == 0 then
		phase = 'reload_wait'
		phaseFrame = gameFrame
	elseif phase == 'reload_wait' and gameFrame >= phaseFrame + 240 then  -- ecriture carte terminee
		phase = 'reload_menu'
		phaseFrame = gameFrame
		pulse(B.START, function() end)
	elseif phase == 'reload_menu' and read32(menuAddr) == 1 then
		phase = 'reload_go'
		phaseFrame = gameFrame
		pulse(B.CROSS, function() end)
	elseif phase == 'reload_go' and gameFrame >= phaseFrame + 300 then
		phase = 'done'
		state('REPRISE')
		if zone() ~= 120 then fail('reprise hors de la salle du trone') return end
		if zoneW() == w119 or zoneW() == 0 then fail('reprise : carte 042 absente') return end
		if zoneW() == wValley then fail('reprise : carte de la vallee au lieu d Eden') return end
		if bit.band(gates(), 1) == 0 then fail('reprise : epee perdue') return end
		if dialogueId() ~= 4294967295 then fail('reprise : narration rejouee') return end
		shot('reprise_trone')
		finale()
	end
	if gameFrame > 30000 then fail('timeout phase=' .. phase) end
end)

print('JEEHAN_VALLEE ready main=0x' .. string.format('%08x', mainAddr) .. ' outdir=' .. outdir)
