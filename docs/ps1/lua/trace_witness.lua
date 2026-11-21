-- Points d'arret pour le TEMOIN intact (template PSn00bSDK non modifie).
-- Adresses lues dans witness_clean/build/template.map :
--   _start 0x8001214c, main 0x80010400
-- La presence de ces hits prouve que l'environnement et le lancement
-- fonctionnent, independamment de notre programme.

local addresses = {
	{ 0x8001214c, "witness_entry" },
	{ 0x80010400, "witness_main" },
}

local hits = {}
local bps = {}
local frame = 0

for _, item in ipairs(addresses) do
	local addr, label = item[1], item[2]
	hits[label] = 0
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, 'trace ' .. label,
		function()
			hits[label] = hits[label] + 1
			if hits[label] <= 2 then
				local r = PCSX.getRegisters()
				local g = r.GPR.n
				print(string.format('BP %s #%d pc=%08x ra=%08x sp=%08x gp=%08x',
					label, hits[label], r.pc, g.ra, g.sp, g.gp))
			end
			return true
		end,
		'trace ' .. label)
end

local summaryFrame = tonumber(os.getenv('PS1_SUMMARY_FRAME') or '60')
local quitFrame = tonumber(os.getenv('PS1_QUIT_FRAME') or '70')

print('TRACE_WITNESS ready: ' .. #bps .. ' breakpoints, summary@' ..
	summaryFrame .. ' quit@' .. quitFrame)

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	frame = frame + 1
	if frame == summaryFrame then
		print('TRACE_WITNESS summary frame=' .. frame)
		for _, item in ipairs(addresses) do
			print(string.format('TRACE_WITNESS %s hits=%d', item[2], hits[item[2]]))
		end
	end
	if frame == quitFrame then PCSX.quit(0) end
end)
