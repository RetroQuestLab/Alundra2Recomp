-- Sonde minimale : verifie quels breakpoints PCSX-Redux atteint reellement.
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x8001dae8')
local startAddr = tonumber(os.getenv('PS1_START_ADDR') or '0x80022a5c')
print(string.format('PROBE ready main=%08x start=%08x', mainAddr, startAddr))

local function add(name, addr)
	PCSX.addBreakpoint(addr, 'Exec', 4, name,
		function()
			print(string.format('PROBE HIT %s pc=%08x', name, PCSX.getRegisters().pc))
			return false
		end, name)
end
add('main', mainAddr)
add('capture menu: main reached', mainAddr)
add('start', startAddr)

local frames = 0
PCSX.Events.createEventListener('GPU::Vsync', function()
	frames = frames + 1
	if frames % 100 == 0 then print('PROBE frame ' .. frames) end
	if frames >= 2000 then PCSX.quit(0) end
end)
