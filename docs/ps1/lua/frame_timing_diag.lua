-- Diagnostic minimal de la sonde temps d'image : imprime l'etat de
-- g_ps1FrameTiming sans jamais ecrire en RAM ni modifier le rendu.
-- PS1_TIMING_ADDR = adresse du symbole ELF (POD PS1FrameTiming).

local timingAddr = tonumber(os.getenv('PS1_TIMING_ADDR') or '0')
if timingAddr == 0 then
	print('FRAME_TIMING_DIAG: FAIL adresse absente')
	PCSX.quit(1)
	return
end

local mem = PCSX.getMemPtr()
local function read32(addr)
	local off = addr - 0x80000000
	return mem[off] + mem[off + 1] * 256 + mem[off + 2] * 65536 + mem[off + 3] * 16777216
end
local function read16(addr)
	local off = addr - 0x80000000
	return mem[off] + mem[off + 1] * 256
end

local gameFrameAddr = tonumber(os.getenv('PS1_GAME_FRAME_ADDR') or '0')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR') or '0')
local timerModeAddr = tonumber(os.getenv('PS1_TIMER_MODE_ADDR') or '0')
local rawCntAddr = tonumber(os.getenv('PS1_RAWCNT_ADDR') or '0')
local cnt0Addr = tonumber(os.getenv('PS1_CNT0_ADDR') or '0')
local cnt2Addr = tonumber(os.getenv('PS1_CNT2_ADDR') or '0')
local mode0Addr = tonumber(os.getenv('PS1_MODE0_ADDR') or '0')
local mode2Addr = tonumber(os.getenv('PS1_MODE2_ADDR') or '0')
local heapAddr = tonumber(os.getenv('PS1_HEAP_ADDR') or '0')
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0')
local startAddr = tonumber(os.getenv('PS1_START_ADDR') or '0')
bps = {}
local gameStarted = false

local function addBp(addr, label, cb)
	if addr == 0 then return end
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, label, cb, label)
end
addBp(startAddr, 'diag start', function() return false end)
addBp(mainAddr, 'diag main', function()
	gameStarted = true
	print('FRAME_TIMING_DIAG main reached')
	return false
end)

local rcntBpAddr = tonumber(os.getenv('PS1_GETRCNT_BP_ADDR') or '0')
local rcntHits = 0
addBp(rcntBpAddr, 'diag getrcnt return', function()
	if rcntHits < 24 then
		local r = PCSX.getRegisters()
		rcntHits = rcntHits + 1
		print(string.format('FRAME_TIMING_DIAG GetRCnt hit=%d v0=%d mode=%d', rcntHits, r.GPR.n.v0,
			timerModeAddr == 0 and -1 or read32(timerModeAddr)))
	end
	return true
end)

local frame = 0
local startTime = os.clock()
vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not gameStarted then return end
	frame = frame + 1
	if frame <= 5 or frame % 30 == 0 then
		print(string.format('FRAME_TIMING_DIAG frame=%d cpu=%.3f gameFrame=%d menu=%d mode=%d modes=%d/%d raw=%d/%d/%d heap=0x%08x ready=%d count=%d head=%d last=%d s0=%d s1=%d',
			frame,
			os.clock() - startTime,
			gameFrameAddr == 0 and -1 or read32(gameFrameAddr),
			menuAddr == 0 and -1 or read32(menuAddr),
			timerModeAddr == 0 and -1 or read32(timerModeAddr),
			mode0Addr == 0 and -1 or read16(mode0Addr),
			mode2Addr == 0 and -1 or read16(mode2Addr),
			cnt0Addr == 0 and -1 or read16(cnt0Addr),
			rawCntAddr == 0 and -1 or read16(rawCntAddr),
			cnt2Addr == 0 and -1 or read16(cnt2Addr),
			heapAddr == 0 and -1 or read32(heapAddr),
			read32(timingAddr),
			read32(timingAddr + 4),
			read32(timingAddr + 8),
			read32(timingAddr + 12),
			read16(timingAddr + 16),
			read16(timingAddr + 18)))
	end
	if frame >= 300 then
		print('FRAME_TIMING_DIAG: DONE')
		PCSX.quit(0)
	end
end)

print(string.format('FRAME_TIMING_DIAG ready addr=0x%08x', timingAddr))
