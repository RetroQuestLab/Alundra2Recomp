-- Capture native du dialogue A1 (WIND.TX + FONT3.TIM) sur la cible PS1.
-- Depuis v0.3.0 le boot passe par titre -> menu -> introduction avant le jeu :
-- la chronometrie doit donc suivre les VRAIES variables de jeu (g_intro.frame,
-- g_shown, g_panel) et non un simple compteur de VSync.
-- Les appuis sont maintenus 14 VSync : sous -interpreter, les fronts montants
-- doivent survivre a la difference de cadence entre le GPU et le MIPS.
-- SESSION_OUT=... PS1_MAIN_ADDR=... PS1_INTRO_ADDR=...
-- PS1_DLG_SHOWN_ADDR=... PS1_DLG_PANEL_ADDR=... pcsx-redux -dofile capture_dialogue.lua

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x80014c30')
local introAddr = tonumber(os.getenv('PS1_INTRO_ADDR') or '0x800ae404')
local shownAddr = tonumber(os.getenv('PS1_DLG_SHOWN_ADDR') or '0x8008dc7c')
local panelAddr = tonumber(os.getenv('PS1_DLG_PANEL_ADDR') or '0x8008dc80')
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local mem = PCSX.getMemPtr()
local gameFrame = 0
local gameStarted = false
local phase = 'title'
local active = nil
local bps = {}

local function read32(addr)
	local off = addr - 0x80000000
	return mem[off] + mem[off + 1] * 256 + mem[off + 2] * 65536 + mem[off + 3] * 16777216
end

local function introFrame() return read32(introAddr) end
local function dlgShown() return read32(shownAddr) end
local function dlgPanel() return read32(panelAddr) end

-- pulse : appui maintenu 14 VSync puis relachement, 14 VSync de stabilisation.
local function pulse(button, cb)
	pad.setOverride(button)
	active = {
		button = button, cb = cb, stage = 1,
		releaseAt = gameFrame + 14, doneAt = gameFrame + 28,
	}
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
	print(string.format('shot %s %dx%d at frame %d panel=%d shown=%d introFrame=%d',
		name, w, h, gameFrame, dlgPanel(), dlgShown(), introFrame()))
end

local function finish()
	print(string.format('DLG_CAPTURE: PASS panel=%d shown=%d frame=%d',
		dlgPanel(), dlgShown(), gameFrame))
	PCSX.quit(0)
end

-- Plusieurs pulses START successifs : #1 complete le panneau 0, #2 avance au 1,
-- #3 le complete, #4 avance au 2, #5 le complete puis on capture.
local function completePanel2()
	pulse(B.START, function()
		pulse(B.START, function()
			pulse(B.START, function()
				pulse(B.START, function()
					if dlgPanel() ~= 2 then
						print('DLG_CAPTURE: FAIL panneau inattendu ' .. dlgPanel())
						PCSX.quit(1)
						return
					end
					shot('dlg_panel3_full')
					pulse(B.START, function() finish() end)
				end)
			end)
		end)
	end)
end

bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'capture dialogue: main reached',
	function()
		gameStarted = true
		gameFrame = 0
		local r = PCSX.getRegisters()
		print(string.format('CAPTURE main reached pc=%08x ra=%08x sp=%08x gp=%08x',
			r.pc, r.GPR.n.ra, r.GPR.n.sp, r.GPR.n.gp))
		return false
	end,
	'capture dialogue: main reached')

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not gameStarted then return end
	gameFrame = gameFrame + 1

	if active then
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

	if phase == 'title' and gameFrame >= 60 then
		phase = 'menu'
		pulse(B.START, function() end)
	elseif phase == 'menu' and gameFrame >= 110 then
		phase = 'intro'
		pulse(B.CROSS, function() end)
	elseif phase == 'intro' and introFrame() >= 30 then
		phase = 'game'
		pulse(B.START, function() end)
	elseif phase == 'game' and dlgShown() >= 8 then
		phase = 'dialogue'
		shot('dlg_typing')
		pulse(B.START, function()
			if dlgPanel() ~= 0 then
				print('DLG_CAPTURE: FAIL dialogue non demarre, panel=' .. dlgPanel())
				PCSX.quit(1)
				return
			end
			shot('dlg_panel1_full')
			completePanel2()
		end)
	end

	if gameFrame > 12000 then
		print('DLG_CAPTURE: FAIL timeout phase=' .. phase ..
			' introFrame=' .. introFrame() .. ' panel=' .. dlgPanel())
		PCSX.quit(1)
	end
end)

print('CAPTURE_DIALOGUE ready, main=0x' .. string.format('%08x', mainAddr) ..
	' intro=0x' .. string.format('%08x', introAddr) ..
	' shown=0x' .. string.format('%08x', shownAddr) ..
	' panel=0x' .. string.format('%08x', panelAddr) .. ' outdir=' .. outdir)
