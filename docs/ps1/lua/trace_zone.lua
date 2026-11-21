-- Trace d'execution de la premiere zone jouable (hors TTY du programme).
-- Les adresses sont relues via nm par l'appelant, ou fournies en environnement.

local function addr(name, default)
	return tonumber(os.getenv(name) or default)
end

local addresses = {
	{ addr('PS1_MAIN_ADDR', 0x80011950), 'main' },
	{ addr('PS1_ZONEINIT_ADDR', 0x80010a10), 'zoneInit' },
	{ addr('PS1_ZONEDRAW_ADDR', 0x80010c48), 'zoneDraw' },
	{ addr('PS1_HEROINIT_ADDR', 0x800111b0), 'heroInit' },
	{ addr('PS1_HEROUPDATE_ADDR', 0x80011330), 'heroUpdate' },
	{ addr('PS1_HERODRAW_ADDR', 0x800116c8), 'heroDraw' },
	{ addr('PS1_PFSPRITE_ADDR', 0x80010504), 'pfDrawSprite16' },
	{ addr('PS1_PFPRESENT_ADDR', 0x80010380), 'pfPresent' },
	{ addr('PS1_PFDRAW_ADDR', 0x800106d8), 'pfDrawRect' },
}

local hits = {}
local bps = {}
local frame = 0

for _, item in ipairs(addresses) do
	local a, label = item[1], item[2]
	hits[label] = 0
	bps[#bps + 1] = PCSX.addBreakpoint(a, 'Exec', 4, 'trace ' .. label,
		function()
			hits[label] = hits[label] + 1
			return true
		end, 'trace ' .. label)
end

local step = tonumber(os.getenv('PS1_TRACE_STEP') or '40')
local quitFrame = tonumber(os.getenv('PS1_QUIT_FRAME') or '150')

local listener = PCSX.Events.createEventListener('GPU::Vsync', function()
	frame = frame + 1
	if frame % step == 0 then
		print('TRACE_ZONE summary frame=' .. frame)
		for _, item in ipairs(addresses) do
			print(string.format('TRACE_ZONE %s hits=%d', item[2], hits[item[2]]))
		end
	end
	if frame >= quitFrame then PCSX.quit(0) end
end)

print('TRACE_ZONE ready: ' .. #bps .. ' breakpoints')
