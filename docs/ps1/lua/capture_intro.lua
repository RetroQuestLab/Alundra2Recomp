-- Capture native de l'introduction PS1 (decors A1, texte A2 [prologue]).
-- La progression est lue dans g_intro.frame (RAM emulee) : les shots sont
-- declenches sur la VRAIE frame de jeu, pas sur le compteur VSync (l'inter-
-- preteur MIPS avance plus lentement que le GPU).
--
-- Le menu est engage par une ecriture capture-only dans MenuState (screen=DONE,
-- action=NEW_GAME) des que menuInit() a ete observe : le prefill CD rend le
-- moment du titre variable, un appui START a frame fixe n'est plus fiable.
-- Aucun fichier du jeu ni aucun comportement gameplay n'est modifie.
--
-- SESSION_OUT, PS1_MAIN_ADDR, PS1_INTRO_ADDR, PS1_MENU_ADDR.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x80014d28')
local introAddr = tonumber(os.getenv('PS1_INTRO_ADDR') or '0x800ae504')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR') or '0x801b0da0')
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local mem = PCSX.getMemPtr()
local started = false
local frame = 0
local phase = 0
local hold = 0
local shots = {}
-- Comme dans trace_boot.lua : la table doit rester GLOBALE, sinon le GC Lua
-- la collecte apres le chunk et le breakpoint main disparait silencieusement.
bps = {}

local function read32(addr)
	local off = addr - 0x80000000
	return mem[off] + mem[off + 1] * 256 + mem[off + 2] * 65536 + mem[off + 3] * 16777216
end

local function write32(addr, value)
	local off = addr - 0x80000000
	mem[off] = bit.band(value, 255)
	mem[off + 1] = bit.band(bit.rshift(value, 8), 255)
	mem[off + 2] = bit.band(bit.rshift(value, 16), 255)
	mem[off + 3] = bit.band(bit.rshift(value, 24), 255)
end

local function introFrame()
	return read32(introAddr)
end

local function menuReady()
	-- menuInit() ecrit textSpeed=1 et audio=5 ; avant cela, la zone est a zero.
	return read32(menuAddr + 8) == 1 and read32(menuAddr + 12) == 5
end

-- PCSX-Redux : boutons actifs a l'etat bas, overrides initialises a 0xffff.
-- setOverride(b) = overrides &= ~(1<<b) = APPUI ; clearOverride(b) = RELACHE.
-- Les constantes B.* sont des INDEX de bit (START=3, CROSS=14), pas des masques.
local function press(...)
	local buttons = { ... }
	for _, b in ipairs(buttons) do pad.setOverride(b) end
end

local function release(...)
	local buttons = { ... }
	for _, b in ipairs(buttons) do pad.clearOverride(b) end
end

local function shot(name)
	if shots[name] then return end
	shots[name] = true
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
	print(string.format('shot %s %dx%d at vsync=%d introFrame=%d', name, w, h, frame, introFrame()))
end

bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'capture intro: main reached',
	function()
		started = true
		local r = PCSX.getRegisters()
		print(string.format('CAPTURE main reached pc=%08x ra=%08x sp=%08x gp=%08x',
			r.pc, r.GPR.n.ra, r.GPR.n.sp, r.GPR.n.gp))
		return false
	end,
	'capture intro: main reached')

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not started then return end
	frame = frame + 1
	local f = introFrame()

	-- Phase 0 : menu init observe -> injection capture-only Nouvelle partie.
	if phase == 0 and menuReady() then
		write32(menuAddr + 0, 4)   -- MENU_SCR_DONE
		write32(menuAddr + 36, 1)  -- MENU_ACT_NEW_GAME
		phase = 1
		print(string.format('CAPTURE new game injected at vsync=%d', frame))
	end

	-- v0.8.18 : les scenes changent aux frontieres des panneaux (648/1728/1944).
	-- Les seuils ci-dessous photographient chaque decor avec son panneau de texte.
	if f >= 60 then shot('intro_sky') end
	if f >= 700 then shot('intro_ship') end
	if f >= 1300 then shot('intro_ship_text_ship_scene') end
	if f >= 1800 then shot('intro_fall') end
	if f >= 2000 then shot('intro_beach') end

	if frame % 600 == 0 then
		print(string.format('CAPTURE progress vsync=%d introFrame=%d phase=%d', frame, f, phase))
	end

	-- Une fois les quatre decors captures, START passe l'intro et on capture
	-- l'arrivee en jeu. Si la manette emulee ne repond pas, on force la fin
	-- de l'intro en ecrivant directement g_intro.frame (capture-only).
	if (f >= 2050 or frame >= 12000) and phase == 1 then
		phase = 2
		hold = frame
		press(B.START)
	end
	if phase == 2 and frame >= hold + 14 then
		release(B.START)
		phase = 3
	end
	if phase == 3 and frame >= hold + 80 then
		-- Repli : g_intro.frame = INTRO_TOTAL_FRAMES force la sortie sans start.
		if introFrame() < 2160 then write32(introAddr, 2160) end
		shot('game_after_skip')
		phase = 4
	end
	if phase == 4 and frame >= hold + 110 then
		PCSX.quit(0)
	end
end)

print('CAPTURE_INTRO ready, main=0x' .. string.format('%08x', mainAddr) ..
	' introAddr=0x' .. string.format('%08x', introAddr) ..
	' menuAddr=0x' .. string.format('%08x', menuAddr) .. ' outdir=' .. outdir)
