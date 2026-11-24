-- Parcours Prunewell -> Finrod (A2 bloc 32) sur l'ISO v0.8.24.
-- Placement diagnostic : zone 011 pour Prunewell, zone 013 pour Finrod, et
-- position/direction du heros. Aucun autre champ QuestState n'est ecrit :
-- talkedPrunewell est pose par la conversation reelle (CROIX + START), et le
-- variant de Finrod doit alors apparaitre tout seul (dialogueId 19).
--
-- Env : SESSION_OUT, PS1_MAIN_ADDR, PS1_HERO_X/Y/DIR, PS1_QUEST_ADDR,
-- PS1_INTRO_ADDR, PS1_DLG_ACTIVE_ADDR, PS1_ENEMY_ADDR, PS1_DUEL_ADDR,
-- PS1_MENU_DRAW_ADDR, PS1_INTRO_DRAW_ADDR, PS1_MENU_ADDR.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x80020724')
local heroXAddr = tonumber(os.getenv('PS1_HERO_X_ADDR') or '0x8008ec28')
local heroYAddr = tonumber(os.getenv('PS1_HERO_Y_ADDR') or '0x8008ec24')
local dirAddr = tonumber(os.getenv('PS1_DIR_ADDR') or '0x8008e5f4')
local questAddr = tonumber(os.getenv('PS1_QUEST_ADDR') or '0x801c3d58')
local introAddr = tonumber(os.getenv('PS1_INTRO_ADDR') or '0x800af3fc')
local dlgActiveAddr = tonumber(os.getenv('PS1_DLG_ACTIVE_ADDR') or '0')
local enemyAddr = tonumber(os.getenv('PS1_ENEMY_ADDR') or '0x8003e9e0')
local duelBase = tonumber(os.getenv('PS1_DUEL_ADDR') or '0x801c4188')
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

local function stage() return read32(questAddr) end
local function dialogueId() return read32(questAddr + 8) end
local function talkedPrunewell() return read32(questAddr + 12) end
local function talkedFinrod() return read32(questAddr + 20) end
local function zone() return read32(questAddr + 56) end
local function dlgIsActive() return dlgActiveAddr ~= 0 and read32(dlgActiveAddr) == 1 end
local function enemyFrames() return bit.band(read32(enemyAddr + 4), 0xffff) end
local function duelSteps() return read32(duelBase + 16) end

local function teleport(x, y, dir)
	write32(heroXAddr, x * 65536)
	write32(heroYAddr, y * 65536)
	write32(dirAddr, dir)
end

local function shot(name)
	local ss = PCSX.GPU.takeScreenShot()
	local w, h = ss.width, ss.height
	if ss.data.size ~= w * h * 2 then print('shot ' .. name .. ': skipped') return end
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
	print(string.format('SHOT %s frame=%d zone=%d stage=%d talkedP=%d talkedF=%d dlg=%d',
		name, gameFrame, zone(), stage(), talkedPrunewell(), talkedFinrod(), dialogueId()))
end

local function fail(message)
	print('FINROD_REUNION: FAIL ' .. message)
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

local function closeDialogue(n, cb)
	if not dlgIsActive() then cb() return end
	if n <= 0 then fail('dialogue non ferme, dlg=' .. dialogueId()) return end
	tap(B.START, function() waitFrames(6, function() closeDialogue(n - 1, cb) end) end)
end

local function talkFinrod()
	write32(questAddr + 56, 13)
	teleport(982, 888, 2)  -- PF_DIR_RIGHT : point d'interaction = (996,888)
	waitFrames(16, function()
		if zone() ~= 13 then fail('zone 013 non posee') return end
		if talkedPrunewell() ~= 1 then fail('talkedPrunewell perdu') return end
		tap(B.CROSS, function()
			waitFrames(6, function()
				if dialogueId() ~= 19 then
					fail(string.format('variant retrouvailles absent: dlg=%d attendu=19 talkedP=%d',
						dialogueId(), talkedPrunewell()))
					return
				end
				print(string.format('FINROD_REUNION_VOUE dlg=%d zone=%d talkedP=%d talkedF=%d stage=%d',
					dialogueId(), zone(), talkedPrunewell(), talkedFinrod(), stage()))
				shot('finrod_reunion')
				closeDialogue(16, function()
					if talkedFinrod() ~= 1 then fail('talkedFinrod non pose') return end
					print(string.format('FINROD_REUNION_CAPTURE: PASS dlg_avant=19 talkedF=%d stage=%d shots=%s',
						talkedFinrod(), stage(), table.concat(captured, ',')))
					PCSX.quit(0)
				end)
			end)
		end)
	end)
end

local function talkPrunewell()
	write32(questAddr + 56, 11)
	teleport(742, 760, 2)  -- PF_DIR_RIGHT : point d'interaction = (756,760)
	waitFrames(16, function()
		if zone() ~= 11 then fail('zone 011 non posee') return end
		if talkedPrunewell() ~= 0 then fail('talkedPrunewell deja pose') return end
		tap(B.CROSS, function()
			waitFrames(6, function()
				if dialogueId() ~= 0 then
					fail('dialogue prunewell absent: dlg=' .. dialogueId())
					return
				end
				print(string.format('PRUNWELL_PARLE dlg=%d talkedP=%d', dialogueId(), talkedPrunewell()))
				closeDialogue(20, function()
					if talkedPrunewell() ~= 1 then
						fail('talkedPrunewell non pose par la conversation')
						return
					end
					print(string.format('PRUNWELL_TALKED talkedP=%d stage=%d', talkedPrunewell(), stage()))
					talkFinrod()
				end)
			end)
		end)
	end)
end

-- Le GC Lua collecte une table locale : les breakpoints seraient retires.
bps = {}
local function addBp(addr, label, cb)
	if addr == 0 then return end
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, label, cb, label)
end
addBp(menuDrawAddr, 'reunion: menuDrawPS1', function() menuDraws = menuDraws + 1 return true end)
addBp(introDrawAddr, 'reunion: introDrawPS1', function() introDraws = introDraws + 1 return true end)
addBp(mainAddr, 'reunion: main', function()
	started = true
	gameFrame = 0
	return false
end, 'reunion: main')

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
		phase = 'paco'
		waitFrames(20, talkPrunewell)
	end
	if gameFrame > 20000 then fail('timeout phase=' .. phase) end
end)

print('FINROD_REUNION ready main=0x' .. string.format('%08x', mainAddr) ..
	' quest=0x' .. string.format('%08x', questAddr) .. ' outdir=' .. outdir)
