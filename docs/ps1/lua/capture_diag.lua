-- Mesure memoire reelle sur l'ISO PS1 livree : lit g_ps1Diag (POD 44 octets)
-- ecrit par diag_ps1.cpp. Aucune ecriture dans le state de jeu, aucune capture
-- d'image : la preuve est un relevé de RAM apres >= 200 frames de gameplay.
--
-- Variables d'environnement : SESSION_OUT, PS1_MAIN_ADDR, PS1_DIAG_ADDR,
-- PS1_INTRO_ADDR, PS1_MENU_ADDR.
--
-- Le passage de l'intro reproduit capture_intro.lua : injection capture-only
-- de MenuState (screen=DONE, action=NEW_GAME) des que menuInit() est observe,
-- puis repli g_intro.frame=2160 si la manette emulee ne repond pas.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x8001f8e4')
local diagAddr = tonumber(os.getenv('PS1_DIAG_ADDR') or '0x801b0784')
local introAddr = tonumber(os.getenv('PS1_INTRO_ADDR') or '0x801b1bb0')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR') or '0x801b11d4')

local mem = PCSX.getMemPtr()
local started = false
local frame = 0
local gameFrames = 0
local phase = 0
local hold = 0
local printed = false
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

-- Layout exact de PS1DiagState (11 x uint32 = 44 octets).
local function diag()
	return {
		magic          = read32(diagAddr + 0),
		heapTotal      = read32(diagAddr + 4),
		heapReserved   = read32(diagAddr + 8),
		heapStackReserve = read32(diagAddr + 12),
		heapAlloc      = read32(diagAddr + 16),
		heapPeak       = read32(diagAddr + 20),
		stackTop       = read32(diagAddr + 24),
		stackPaintLo   = read32(diagAddr + 28),
		stackFloor     = read32(diagAddr + 32),
		stackDepthMax  = read32(diagAddr + 36),
		sampleCount    = read32(diagAddr + 40),
	}
end

local function introFrame() return read32(introAddr) end
local function menuReady()
	return read32(menuAddr + 8) == 1 and read32(menuAddr + 12) == 5
end

local function hex(v) return string.format('0x%08x', math.floor(v)) end

bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'capture diag: main reached',
	function()
		started = true
		local r = PCSX.getRegisters()
		print(string.format('CAPTURE_DIAG main reached pc=%08x ra=%08x sp=%08x gp=%08x',
			r.pc, r.GPR.n.ra, r.GPR.n.sp, r.GPR.n.gp))
		return false
	end,
	'capture diag: main reached')

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not started then return end
	frame = frame + 1

	if phase == 0 and menuReady() then
		write32(menuAddr + 0, 4)   -- MENU_SCR_DONE
		write32(menuAddr + 36, 1)  -- MENU_ACT_NEW_GAME
		phase = 1
		print(string.format('CAPTURE_DIAG new game injected at vsync=%d', frame))
	end

	if phase == 1 then
		local f = introFrame()
		if f >= 2050 or frame >= 12000 then
			phase = 2
			hold = frame
		end
	end

	if phase == 2 and frame >= hold + 80 then
		if introFrame() < 2160 then write32(introAddr, 2160) end
		phase = 3
		gameFrames = 0
		print(string.format('CAPTURE_DIAG gameplay started at vsync=%d', frame))
	end

	if phase == 3 then
		gameFrames = gameFrames + 1
		if gameFrames % 100 == 0 then
			local d = diag()
			print(string.format('CAPTURE_DIAG progress vsync=%d gameFrames=%d samples=%d',
				frame, gameFrames, d.sampleCount))
		end
		-- Au moins 200 frames de jeu et au moins 3 balayages pile (periode 60).
		if gameFrames >= 220 and not printed then
			local d = diag()
			printed = true
			print(string.format('DIAG magic=%s heapPeak=%d stackDepthMax=%d stackFloor=%s stackTop=%s samples=%d',
				hex(d.magic), d.heapPeak, d.stackDepthMax, hex(d.stackFloor), hex(d.stackTop), d.sampleCount))
			print(string.format('DIAG_FINAL heapTotal=%d heapReserved=%d heapStackReserve=%d heapAlloc=%d stackPaintLo=%s',
				d.heapTotal, d.heapReserved, d.heapStackReserve, d.heapAlloc, hex(d.stackPaintLo)))
			PCSX.quit(0)
		end
	end
end)

print('CAPTURE_DIAG ready main=' .. hex(mainAddr) .. ' diagAddr=' .. hex(diagAddr) ..
	' introAddr=' .. hex(introAddr) .. ' menuAddr=' .. hex(menuAddr))
