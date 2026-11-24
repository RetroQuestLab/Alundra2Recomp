-- Capture native du menu armes / objets / equipement PS1.
-- Navigation pilote par l'etat REEL du jeu : g_intro (frame) et g_invCapture
-- (bits open/page/curseur). appuis maintenus 14 VSync + 14 VSync de stabilisation.
-- SESSION_OUT, PS1_MAIN_ADDR, PS1_INTRO_ADDR, PS1_INV_ADDR,
-- PS1_MENU_DRAW_ADDR, PS1_INTRO_DRAW_ADDR, PS1_MENU_ADDR.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x80017070')
local introAddr = tonumber(os.getenv('PS1_INTRO_ADDR') or '0x800b0000')
local invAddr = tonumber(os.getenv('PS1_INV_ADDR') or '0x800b0004')
local menuDrawAddr = tonumber(os.getenv('PS1_MENU_DRAW_ADDR') or '0')
local introDrawAddr = tonumber(os.getenv('PS1_INTRO_DRAW_ADDR') or '0')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR') or '0')
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local mem = PCSX.getMemPtr()
local gameFrame = 0
local gameStarted = false
local menuDraws = 0
local introDraws = 0
local gamePhaseFrame = 0
local phase = 'title'
local active = nil
-- Le GC Lua peut collecter une table locale et desactiver silencieusement les
-- breakpoints : la table des points d'arret doit rester globale.
bps = {}

local function read32(addr)
	local off = addr - 0x80000000
	return mem[off] + mem[off + 1] * 256 + mem[off + 2] * 65536 + mem[off + 3] * 16777216
end

local function introFrame() return read32(introAddr) end
local function invOpen() return bit.band(read32(invAddr), 255) ~= 0 end
local function invPage() return bit.band(bit.rshift(read32(invAddr), 8), 255) end
local function invCursor() return bit.band(bit.rshift(read32(invAddr), 16), 255) end

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
	print(string.format('shot %s %dx%d frame=%d inv=%d page=%d cursor=%d',
		name, w, h, gameFrame, invOpen() and 1 or 0, invPage(), invCursor()))
end

local function pulse(button, cb)
	pad.setOverride(button)
	active = { button = button, mode = 'pulse', cb = cb, stage = 1,
		releaseAt = gameFrame + 14, doneAt = gameFrame + 28 }
end

local function waitFrames(frames, cb)
	active = { button = nil, mode = 'wait', cb = cb, doneAt = gameFrame + frames }
end

local function waitOpen(page, cb)
	active = { button = nil, mode = 'until', cb = cb, stage = 1,
		cond = function() return invOpen() and invPage() == page end,
		releaseAt = gameFrame + 2, doneAt = 0 }
end

local function waitClosed(cb)
	active = { button = nil, mode = 'until', cb = cb, stage = 1,
		cond = function() return not invOpen() end,
		releaseAt = gameFrame + 2, doneAt = 0 }
end

local function fail(message)
	print('INVENTORY_CAPTURE: FAIL ' .. message)
	PCSX.quit(1)
end

bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'capture inventory: main reached',
	function()
		gameStarted = true
		gameFrame = 0
		local r = PCSX.getRegisters()
		print(string.format('CAPTURE main reached pc=%08x ra=%08x sp=%08x gp=%08x',
			r.pc, r.GPR.n.ra, r.GPR.n.sp, r.GPR.n.gp))
		return false
	end,
	'capture inventory: main reached')

local function countBp(addr, label, counter)
	if addr == 0 then return end
	-- PCSX-Redux : le callback DOIT retourner true pour que le breakpoint reste
	-- actif. Un retour false le desactive apres le premier hits (menuDraws=1).
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, label,
		function() counter(); return true end, label)
end

countBp(menuDrawAddr, 'capture inventory: menuDraw', function() menuDraws = menuDraws + 1 end)
countBp(introDrawAddr, 'capture inventory: introDraw', function() introDraws = introDraws + 1 end)

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not gameStarted then return end
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
		-- mode 'until' : condition vraie puis 12 frames de stabilisation.
		if active.stage == 1 and gameFrame >= active.releaseAt and active.cond() then
			active.stage = 2
			active.doneAt = gameFrame + 12
		elseif active.stage == 2 and gameFrame >= active.doneAt then
			local cb = active.cb
			active = nil
			if cb then cb() end
		end
		return
	end

	-- Navigation pilote par l'etat observe : le pre-remplissage CD decale le menu
	-- d'environ 120 images. Les ancres sont le premier rendu du menu, son passage
	-- en ecran principal, puis le premier rendu de l'intro.
	if phase == 'title' and menuDraws >= 2 then
		phase = 'menu'
		pulse(B.START, function() end)
	elseif phase == 'menu' and read32(menuAddr) == 1 then
		phase = 'intro'
		pulse(B.CROSS, function() end)
	elseif phase == 'intro' and introDraws >= 2 then
		phase = 'game'
		gamePhaseFrame = 0
		pulse(B.START, function() end)  -- START skips the whole intro
	elseif phase == 'game' and gamePhaseFrame == 0 then
		gamePhaseFrame = gameFrame
	elseif phase == 'game' and gameFrame >= gamePhaseFrame + 90 then
		phase = 'open'
		pulse(B.START, function()
			waitOpen(0, function()
				shot('inv_weapons')
				pulse(B.RIGHT, function()
					waitOpen(1, function()
						shot('inv_items')
						pulse(B.RIGHT, function()
							waitOpen(2, function()
								shot('inv_equipment')
								pulse(B.CIRCLE, function()
									waitClosed(function()
										shot('inv_closed')
										print('INVENTORY_CAPTURE: PASS')
										PCSX.quit(0)
									end)
								end)
							end)
						end)
					end)
				end)
			end)
		end)
	end

	if gameFrame > 8000 then
		fail('timeout phase=' .. phase .. ' inv=' .. tostring(invOpen()) ..
			' page=' .. invPage() .. ' intro=' .. introFrame())
	end
end)

print('CAPTURE_INVENTORY ready, main=0x' .. string.format('%08x', mainAddr) ..
	' intro=0x' .. string.format('%08x', introAddr) ..
	' inv=0x' .. string.format('%08x', invAddr) .. ' outdir=' .. outdir)
