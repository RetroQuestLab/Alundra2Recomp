-- Points d'arret externes : independants de puts/printf/AddSIO du programme.
-- Usage (adresses a relire dans le .map apres chaque rebuild) :
--   pcsx-redux -testmode -stdout -lua_stdout -bios <openbios.bin> \
--     -exe <alundra2.exe> -run -debugger -interpreter -dofile trace_boot.lua
--
-- Les objets breakpoint sont conserves dans une table globale : s'ils sont
-- ramasses par le GC, le point d'arret disparait (documentation officielle).

-- Les adresses peuvent etre fournies par l'environnement : le smoke les lit
-- dans l'ELF avec nm apres chaque rebuild (voir tools/ps1_smoke.ps1).
local function addr(name, default)
	return tonumber(os.getenv(name) or (string.format('0x%x', default)))
end

local addresses = {
	{ addr('PS1_ENTRY_ADDR',    0x80013944), "entry/_start" },
	{ addr('PS1_MAIN_ADDR',     0x80011980), "main" },
	{ addr('PS1_PFINIT_ADDR',   0x80010098), "pfInit" },
	{ addr('PS1_PFPRESENT_ADDR',0x80010380), "pfPresent" },
	{ addr('PS1_PFDRAW_ADDR',   0x800106d8), "pfDrawRect" },
}

local hits = {}
local bps = {}
local frame = 0

local function regsLine()
	local r = PCSX.getRegisters()
	local g = r.GPR.n
	return string.format("pc=%08x ra=%08x sp=%08x gp=%08x", r.pc, g.ra, g.sp, g.gp)
end

for _, item in ipairs(addresses) do
	local addr, label = item[1], item[2]
	hits[label] = 0
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, 'trace ' .. label,
		function()
			hits[label] = hits[label] + 1
			if hits[label] <= 3 then
				print(string.format('BP %s #%d %s', label, hits[label], regsLine()))
			end
			return true
		end,
		'trace ' .. label)
end

local summaryFrame = tonumber(os.getenv('PS1_SUMMARY_FRAME') or '120')
local quitFrame = tonumber(os.getenv('PS1_QUIT_FRAME') or '130')

print('TRACE_BOOT ready: ' .. #bps .. ' breakpoints, summary@' ..
	summaryFrame .. ' quit@' .. quitFrame .. ' (debugger+interpreter requis)')

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	frame = frame + 1
	if frame == summaryFrame then
		print('TRACE_BOOT summary frame=' .. frame)
		for _, item in ipairs(addresses) do
			local label = item[2]
			print(string.format('TRACE_BOOT %s hits=%d', label, hits[label]))
		end
	end
	if frame == quitFrame then
		print('TRACE_BOOT summary frame=' .. frame .. ' (final)')
		for _, item in ipairs(addresses) do
			local label = item[2]
			print(string.format('TRACE_BOOT %s hits=%d', label, hits[label]))
		end
		PCSX.quit(0)
	end
end)
