-- Capture native de l'ecran titre et du menu de depart PS1 (WIND.TX + FONT3.TIM).
-- Declenche par GPU::Vsync, jamais depuis un callback de breakpoint.
--
-- DEPUIS v0.8.2, le prefill CD decale l'apparition du titre d'environ 120 images.
-- Un sequenceur a frames fixes rate donc l'entree START : la navigation est
-- pilotee par l'etat REEL du jeu (g_menu.screen lu en RAM), comme les autres
-- harnais de capture. Chaque appui est maintenu au moins 14 VSync.
--
-- SESSION_OUT, PS1_MAIN_ADDR, PS1_START_ADDR, PS1_MENU_ADDR.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x8001206c')
local startAddr = tonumber(os.getenv('PS1_START_ADDR') or '0x80022a5c')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR') or '0')
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local mem = PCSX.getMemPtr()
local gameFrame = 0
local gameStarted = false
-- La table des breakpoints doit rester GLOBALE : sinon LuaJIT la collecte et
-- le breakpoint main ne declenche jamais (deja observe sur capture_inventory).
bps = {}

local function read32(addr)
	local off = addr - 0x80000000
	return mem[off] + mem[off + 1] * 256 + mem[off + 2] * 65536 + mem[off + 3] * 16777216
end

-- MenuState::screen est le premier champ (int) de la structure POD.
local function menuScreen()
	if menuAddr == 0 then return -1 end
	return bit.band(read32(menuAddr), 255)
end

local held = nil
local function releasePad()
	if held then pad.clearOverride(held) end
	held = nil
end

local function holdPad(button)
	if held ~= button then
		releasePad()
		pad.setOverride(button)
		held = button
	end
end

-- Le titre peut etre photographie avant le premier pfPresent utile : dans ce
-- cas le framebuffer est uniforme (8,8,32) et le logo n'apparait pas. On sonde
-- quelques pixels de l'affichage courant plutôt que de deviner un nombre de
-- images (le prefill CD a deja fausse la sonde "frame >= 140" en v0.8.15).
local function displayReady()
	local ss = PCSX.GPU.takeScreenShot()
	if ss.data.size ~= ss.width * ss.height * 2 then return false end
	local p = ffi.cast('const uint16_t*', ss.data.data)
	local first = p[0]
	for i = 0, ss.width * ss.height - 1, 97 do
		if p[i] ~= first then return true end
	end
	return false
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
	print(string.format('shot %s %dx%d at frame %d screen=%d', name, w, h, gameFrame, menuScreen()))
end

-- Le breakpoint _start est necessaire sur la version courante de PCSX-Redux :
-- sans lui, le breakpoint main seul n'est pas declenche (sonde 2026-11-24).
bps[#bps + 1] = PCSX.addBreakpoint(startAddr, 'Exec', 4, 'start',
	function()
		print('CAPTURE start reached')
		return false
	end,
	'start')
bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'main',
	function()
		gameStarted = true
		gameFrame = 0
		print('CAPTURE main reached')
		return false
	end,
	'main')

local phase = 'wait_title'
local phaseFrame = 0
local titleShot = false
local function enter(next) phase = next; phaseFrame = 0 end

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not gameStarted then return end
	gameFrame = gameFrame + 1
	phaseFrame = phaseFrame + 1
	local screen = menuScreen()
	if gameFrame == 1 or gameFrame % 50 == 0 then
		print(string.format('CAPTURE state frame=%d screen=%d phase=%s', gameFrame, screen, phase))
	end
	if gameFrame > 900 then
		print('CAPTURE state timeout: menu never reached')
		releasePad()
		PCSX.quit(1)
	end

	if phase == 'wait_title' then
		if screen == 1 then
			releasePad()
			enter('wait_main')
		elseif screen == 0 then
			if not titleShot and phaseFrame >= 20 and phaseFrame % 5 == 0 then
				if displayReady() then
					shot('title')
					titleShot = true
				end
			end
			-- Repli : ne jamais bloquer la navigation si l'affichage reste
			-- uniforme (sonde GPU indisponible) ; le titre sera alors documente
			-- comme non verifiable, sans faire echouer la capture.
			if not titleShot and phaseFrame >= 320 then
				shot('title')
				titleShot = true
			end
			-- Ne quitter le titre qu'apres l'avoir effectivement capture.
			if titleShot then
				local cycle = phaseFrame % 40
				if cycle >= 16 and cycle < 30 then holdPad(B.START) else releasePad() end
			else
				releasePad()
			end
		end
	elseif phase == 'wait_main' then
		if screen == 1 and phaseFrame >= 12 then
			shot('menu_main')
			holdPad(B.DOWN)
			enter('cursor_options')
		end
	elseif phase == 'cursor_options' then
		if phaseFrame >= 14 then releasePad() end
		if phaseFrame >= 28 then
			shot('menu_cursor_options')
			holdPad(B.CROSS)
			enter('wait_options')
		end
	elseif phase == 'wait_options' then
		if screen == 2 then
			if phaseFrame >= 14 then releasePad() end
			if phaseFrame >= 28 then
				shot('menu_options')
				holdPad(B.RIGHT)
				enter('speed')
			end
		end
	elseif phase == 'speed' then
		if phaseFrame >= 14 then releasePad() end
		if phaseFrame >= 28 then
			shot('menu_options_speed')
			holdPad(B.CIRCLE)
			enter('wait_return')
		end
	elseif phase == 'wait_return' then
		if screen == 1 then
			if phaseFrame >= 14 then releasePad() end
			if phaseFrame >= 28 then
				shot('menu_final')
				holdPad(B.DOWN)  -- Options -> Quitter (3e entree sans sauvegarde)
				enter('to_quit')
			end
		end
	elseif phase == 'to_quit' then
		if phaseFrame >= 14 then releasePad() end
		if phaseFrame >= 28 then
			holdPad(B.CROSS)
			enter('wait_quit_title')
		end
	elseif phase == 'wait_quit_title' then
		if screen == 0 then
			if phaseFrame >= 14 then releasePad() end
			if phaseFrame >= 28 then
				shot('after_quit')
				PCSX.quit(0)
			end
		end
	end
end)

print(string.format('CAPTURE_MENU ready main=0x%08x start=0x%08x menu=0x%08x outdir=%s',
	mainAddr, startAddr, menuAddr, outdir))
