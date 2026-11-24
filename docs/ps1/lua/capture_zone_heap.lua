-- Harnais capture-only : mesure du pic heap PS1 par zone sur la sequence
-- 011 -> 013 -> 026 -> 011 avec liberation explicite entre les allocations.
--
-- Ecrit cmd/zone/done dans g_ps1ZoneHeapDiag (POD 16 x u32) et lit les 16 mots
-- apres chaque etape. Aucune entree manette, aucun fichier ecrit : la preuve
-- est un relevé de RAM + stdout Lua signe par tools/ps1_capture_zone_heap.ps1.
--
-- Variables d'environnement : SESSION_OUT, PS1_MAIN_ADDR, PS1_ZONE_DIAG_ADDR,
-- PS1_INTRO_ADDR, PS1_MENU_ADDR, PS1_ZONE_CMD.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x8001f8e4')
local diagAddr = tonumber(os.getenv('PS1_ZONE_DIAG_ADDR') or '0x801b0784')
local introAddr = tonumber(os.getenv('PS1_INTRO_ADDR') or '0x801b1bb0')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR') or '0x801b11d4')
local zoneCmd = tonumber(os.getenv('PS1_ZONE_CMD') or '0x5a5a5a5a')

local mem = PCSX.getMemPtr()
local started = false
local frame = 0
local phase = 0
local hold = 0
local step = 1
local pending = false
local requestedZone = 0
local zones = { 13, 26, 11, 0 }
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

-- Layout exact de PS1ZoneHeapDiag : ne pas reordonner sans changer le C++.
local function diag()
	return {
		magic          = read32(diagAddr + 0),
		cmd            = read32(diagAddr + 4),
		done           = read32(diagAddr + 8),
		step           = read32(diagAddr + 12),
		zone           = read32(diagAddr + 16),
		loadCount      = read32(diagAddr + 20),
		releaseCount   = read32(diagAddr + 24),
		errors         = read32(diagAddr + 28),
		baselineAlloc  = read32(diagAddr + 32),
		allocBefore    = read32(diagAddr + 36),
		allocAfterFree = read32(diagAddr + 40),
		allocAfterLoad = read32(diagAddr + 44),
		peakAfterLoad  = read32(diagAddr + 48),
		bytes          = read32(diagAddr + 52),
		finalAlloc     = read32(diagAddr + 56),
		staticRam      = read32(diagAddr + 60),
	}
end

local function introFrame() return read32(introAddr) end
local function menuReady()
	return read32(menuAddr + 8) == 1 and read32(menuAddr + 12) == 5
end
local function hex(v) return string.format('0x%08x', math.floor(v)) end
local function fail(message)
	print('ZONE_HEAP_FAIL: ' .. message)
	PCSX.quit(1)
end

bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'zone heap: main reached',
	function()
		started = true
		print('ZONE_HEAP_MAIN pc=' .. hex(PCSX.getRegisters().pc))
		return false
	end,
	'zone heap: main reached')

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not started then return end
	frame = frame + 1

	if phase == 0 and menuReady() then
		write32(menuAddr + 0, 4)   -- MENU_SCR_DONE
		write32(menuAddr + 36, 1)  -- MENU_ACT_NEW_GAME
		phase = 1
		print(string.format('ZONE_HEAP_NEW_GAME vsync=%d', frame))
	end

	if phase == 1 and (introFrame() >= 2050 or frame >= 12000) then
		phase = 2
		hold = frame
	end

	if phase == 2 and frame >= hold + 80 then
		if introFrame() < 2160 then write32(introAddr, 2160) end
		phase = 3
		print(string.format('ZONE_HEAP_GAMEPLAY vsync=%d', frame))
	end

	if phase ~= 3 then return end

	if not pending then
		requestedZone = zones[step]
		write32(diagAddr + 4, zoneCmd)
		write32(diagAddr + 16, requestedZone)
		write32(diagAddr + 8, 0)
		pending = true
		print(string.format('ZONE_HEAP_REQ step=%d zone=%d', step, requestedZone))
		return
	end

	local d = diag()
	if d.done == 0 then return end  -- lecture CD / upload VRAM en cours

	if d.magic ~= 0xa25a5a5a then fail('magic inattendu ' .. hex(d.magic)) end
	if d.errors ~= 0 then fail('errors=' .. d.errors) end
	if d.step ~= step then fail('step=' .. d.step .. ' attendu ' .. step) end
	if step <= 3 then
		if d.zone ~= requestedZone then fail('zone=' .. d.zone .. ' attendu ' .. requestedZone) end
		if d.bytes <= 0 then fail('bytes=0 pour la zone ' .. requestedZone) end
		if d.peakAfterLoad < d.allocAfterLoad then fail('pic < alloc courant') end
	end

	print(string.format('ZONE_HEAP step=%d zone=%d alloc=%d peak=%d bytes=%d releases=%d errors=%d magic=%s',
		d.step, step <= 3 and d.zone or 0, d.allocAfterLoad, d.peakAfterLoad,
		d.bytes, d.releaseCount, d.errors, hex(d.magic)))

	if step == 4 then
		if d.loadCount ~= 3 then fail('loadCount=' .. d.loadCount) end
		if d.releaseCount ~= 3 then fail('releaseCount=' .. d.releaseCount) end
		if d.finalAlloc ~= d.baselineAlloc then
			fail('fuite : finalAlloc=' .. d.finalAlloc .. ' baselineAlloc=' .. d.baselineAlloc)
		end
		print(string.format('ZONE_HEAP_RESULT loadCount=%d releaseCount=%d baselineAlloc=%d finalAlloc=%d staticRam=%d',
			d.loadCount, d.releaseCount, d.baselineAlloc, d.finalAlloc, d.staticRam))
		PCSX.quit(0)
		return
	end

	step = step + 1
	pending = false
end)

print('ZONE_HEAP_READY main=' .. hex(mainAddr) .. ' diagAddr=' .. hex(diagAddr) ..
	' introAddr=' .. hex(introAddr) .. ' menuAddr=' .. hex(menuAddr) ..
	' cmd=' .. hex(zoneCmd) .. ' outdir=' .. outdir)
