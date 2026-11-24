-- Mesure du temps d'image reel PS1 par acces direct au compteur racine 1.
-- PS1_TIMING_ADDR = adresse de g_ps1FrameTiming (symbole ELF).
-- PS1_MAIN_ADDR / PS1_START_ADDR = points d'entree fournis par nm ; ils
-- reproduisent le motif des sondes qui fonctionnent sous PCSX-Redux.
-- Les breakpoints et le listener doivent rester GLOBAUX : le GC Lua les
-- collecte sinon silencieusement (piege deja rencontre sur les autres sondes).

local timingAddr = tonumber(os.getenv('PS1_TIMING_ADDR') or '0')
if timingAddr == 0 then
	print('FRAME_TIMING: FAIL adresse timing absente')
	PCSX.quit(1)
	return
end

local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0')
local startAddr = tonumber(os.getenv('PS1_START_ADDR') or '0')
local target = 1000
local mem = PCSX.getMemPtr()

local function read32(addr)
	local off = addr - 0x80000000
	return mem[off] + mem[off + 1] * 256 + mem[off + 2] * 65536 + mem[off + 3] * 16777216
end

local function read16(addr)
	local off = addr - 0x80000000
	return mem[off] + mem[off + 1] * 256
end

local function finish(message)
	print('FRAME_TIMING: ' .. message)
	PCSX.quit(message == 'PASS' and 0 or 1)
end

-- Points d'entree : simples marqueurs d'observation, jamais d'execution.
bps = {}
local gameStarted = mainAddr == 0
local function addBp(addr, label, cb)
	if addr == 0 then return end
	bps[#bps + 1] = PCSX.addBreakpoint(addr, 'Exec', 4, label, cb, label)
end
addBp(startAddr, 'timing start', function() return false end)
addBp(mainAddr, 'timing main', function()
	gameStarted = true
	print('FRAME_TIMING main reached')
	return false
end)

local done = false
vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if done or not gameStarted then return end
	local count = read32(timingAddr + 4)
	if count < target then return end
	done = true

	if read32(timingAddr) ~= 1 then
		finish('FAIL structure non prete')
		return
	end

	local values = {}
	local maxIndex, outliers = 0, 0
	for i = 0, count - 1 do
		local v = read16(timingAddr + 16 + i * 2)
		values[#values + 1] = v
		if v > 20000 then outliers = outliers + 1 end
		if maxIndex == 0 or v > values[maxIndex] then maxIndex = #values end
	end
	table.sort(values)

	local function pct(p)
		local idx = math.floor((#values - 1) * p) + 1
		return values[idx]
	end

	local p50, p95, mx, mn = pct(0.50), pct(0.95), values[#values], values[1]
	local sum = 0
	for _, v in ipairs(values) do sum = sum + v end
	local mean = sum / #values

	-- RCntCNT1 tourne a 1,053497 MHz sur NTSC : taux NOMINAL, pas une mesure.
	local hz = 1053497.0
	local function ms(ticks) return ticks * 1000.0 / hz end

	print(string.format('FRAME_TIMING samples=%d ticks_min=%d ticks_p50=%d ticks_p95=%d ticks_max=%d ticks_mean=%.1f',
		#values, mn, p50, p95, mx, mean))
	print(string.format('FRAME_TIMING ms_nominal_min=%.3f ms_nominal_p50=%.3f ms_nominal_p95=%.3f ms_nominal_max=%.3f ms_nominal_mean=%.3f',
		ms(mn), ms(p50), ms(p95), ms(mx), ms(mean)))
	print(string.format('FRAME_TIMING outliers_gt_20000=%d max_index=%d', outliers, maxIndex - 1))

	if p50 < 16000 or p50 > 19000 then
		finish('FAIL periode mediane hors plage NTSC (16000..19000 ticks)')
		return
	end
	if p95 > p50 + 2000 then
		finish('FAIL p95 trop eloigne de p50 (instabilite > 1,9 ms)')
		return
	end
	finish('PASS')
end)

print('FRAME_TIMING ready addr=0x' .. string.format('%08x', timingAddr) ..
	' target=' .. target .. ' main=0x' .. string.format('%08x', mainAddr) ..
	' start=0x' .. string.format('%08x', startAddr) ..
	' rcnt_hz_nominal=1053497')
