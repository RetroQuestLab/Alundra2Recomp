-- Demarrage a FROID : BIOS complet (pas de -fastboot), aucun appui, aucune
-- ecriture memoire. Releve la VSync a laquelle `main` du jeu est atteint et
-- capture l'ecran a des instants fixes (logo BIOS, titre du jeu).
-- Env : SESSION_OUT, PS1_MAIN_ADDR, PS1_MENU_DRAW_ADDR, COLD_SHOTS (ex. "300,900,1800,2700").
local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0')
local menuDrawAddr = tonumber(os.getenv('PS1_MENU_DRAW_ADDR') or '0')
local shots = {}
for n in string.gmatch(os.getenv('COLD_SHOTS') or '300,900,1800,2700', '%d+') do shots[#shots + 1] = tonumber(n) end
local vsync, mainAt, menuAt, menuDraws, idx = 0, -1, -1, 0, 1

local function shot(name)
	local ss = PCSX.GPU.takeScreenShot()
	local w, h = ss.width, ss.height
	if ss.data.size ~= w * h * 2 then print('COLD_SHOT ' .. name .. ' skipped') return end
	local p = ffi.cast('const uint16_t*', ss.data.data)
	local px = {}
	for i = 0, w * h - 1 do
		local c = p[i]
		px[#px + 1] = string.char(bit.band(c, 31) * 8, bit.band(bit.rshift(c, 5), 31) * 8,
			bit.band(bit.rshift(c, 10), 31) * 8)
	end
	local f = io.open(outdir .. '/' .. name .. '.ppm', 'wb')
	f:write(string.format('P6\n%d %d\n255\n', w, h))
	f:write(table.concat(px))
	f:close()
	print(string.format('COLD_SHOT %s vsync=%d size=%dx%d', name, vsync, w, h))
end

bps = {}
if mainAddr ~= 0 then
	bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'cold: main', function()
		if mainAt < 0 then mainAt = vsync print('COLD_MAIN vsync=' .. vsync) end
		return true
	end)
end
if menuDrawAddr ~= 0 then
	bps[#bps + 1] = PCSX.addBreakpoint(menuDrawAddr, 'Exec', 4, 'cold: menu', function()
		menuDraws = menuDraws + 1
		if menuAt < 0 then menuAt = vsync print('COLD_MENU vsync=' .. vsync) end
		return true
	end)
end

listener = PCSX.Events.createEventListener('GPU::Vsync', function()
	vsync = vsync + 1
	if idx <= #shots and vsync >= shots[idx] then
		shot(string.format('cold_%05d', shots[idx]))
		idx = idx + 1
		if idx > #shots then
			print(string.format('COLD_BOOT_RESULT main=%d menu=%d menuDraws=%d', mainAt, menuAt, menuDraws))
			PCSX.quit((mainAt >= 0 and menuAt >= 0) and 0 or 1)
		end
	end
end)
print('COLD_BOOT ready main=' .. string.format('0x%08x', mainAddr))
