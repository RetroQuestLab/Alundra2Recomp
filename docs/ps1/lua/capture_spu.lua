-- Mesure passive du pic SPU pendant le rendu et le streaming CD : lecture de
-- g_ps1SpuDiag (16 x u32) depuis la RAM emulee. Aucune ecriture dans le POD,
-- aucune image, aucun fichier disque. La navigation capture-only reproduit
-- capture_vram.lua avec un maintien plus long de la phase gameplay pour que le
-- double buffer musical soit ecrit deux fois (preuve de streaming).
--
-- Variables d'environnement : SESSION_OUT, PS1_MAIN_ADDR, PS1_SPU_ADDR,
-- PS1_INTRO_ADDR, PS1_MENU_ADDR, PS1_INV_ADDR.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x8002055c')
local spuAddr = tonumber(os.getenv('PS1_SPU_ADDR') or '0x801c1578')
local introAddr = tonumber(os.getenv('PS1_INTRO_ADDR') or '0x801c49b8')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR') or '0x801c3fdc')
local invAddr = tonumber(os.getenv('PS1_INV_ADDR') or '0x80178780')

local mem = PCSX.getMemPtr()
local pad = PCSX.SIO0.slots[1].pads[1]
local B = PCSX.CONSTS.PAD.BUTTON
local started = false
local frame = 0
local gameFrames = 0
local phase = 0
local hold = 0
local printed = false
local pulse = nil
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

local function spu()
	return {
		magic        = read32(spuAddr + 0),
		ready        = read32(spuAddr + 4),
		frame        = read32(spuAddr + 8),
		transfers    = read32(spuAddr + 12),
		currentBytes = read32(spuAddr + 16),
		peakBytes    = read32(spuAddr + 20),
		peakFrame    = read32(spuAddr + 24),
		overlapBytes = read32(spuAddr + 28),
		rejections   = read32(spuAddr + 32),
		spuBytes     = read32(spuAddr + 36),
		reservedBytes= read32(spuAddr + 40),
		bankBytes    = read32(spuAddr + 44),
		streamBytes  = read32(spuAddr + 48),
		chunks       = read32(spuAddr + 52),
		underruns    = read32(spuAddr + 56),
		integrity    = read32(spuAddr + 60),
	}
end

local function introFrame() return read32(introAddr) end
local function menuReady()
	return read32(menuAddr + 8) == 1 and read32(menuAddr + 12) == 5
end
local function invOpen() return bit.band(read32(invAddr), 255) ~= 0 end
local function hex(v) return string.format('0x%08x', math.floor(v)) end

local function report(label, s)
	print(string.format('SPU phase=%s magic=%s ready=%d frame=%d transfers=%d currentBytes=%d peakBytes=%d peakFrame=%d overlapBytes=%d rejections=%d spuBytes=%d reservedBytes=%d bankBytes=%d streamBytes=%d chunks=%d underruns=%d integrity=%s',
		label, hex(s.magic), s.ready, s.frame, s.transfers, s.currentBytes,
		s.peakBytes, s.peakFrame, s.overlapBytes, s.rejections, s.spuBytes,
		s.reservedBytes, s.bankBytes, s.streamBytes, s.chunks, s.underruns,
		hex(s.integrity)))
end

bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'capture spu: main reached',
	function()
		started = true
		local r = PCSX.getRegisters()
		print(string.format('CAPTURE_SPU main reached pc=%08x ra=%08x sp=%08x gp=%08x',
			r.pc, r.GPR.n.ra, r.GPR.n.sp, r.GPR.n.gp))
		return false
	end,
	'capture spu: main reached')

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	if not started then return end
	frame = frame + 1

	if pulse then
		if pulse.stage == 1 and frame >= pulse.releaseAt then
			pad.clearOverride(pulse.button)
			pulse.stage = 2
		elseif pulse.stage == 2 and frame >= pulse.doneAt then
			pulse = nil
		end
	end

	if phase == 0 and menuReady() then
		report('menu', spu())
		write32(menuAddr + 0, 4)   -- MENU_SCR_DONE
		write32(menuAddr + 36, 1)  -- MENU_ACT_NEW_GAME
		phase = 1
		print(string.format('CAPTURE_SPU new game injected at vsync=%d', frame))
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
		print(string.format('CAPTURE_SPU gameplay started at vsync=%d', frame))
	end

	-- 180 images de gameplay (3 s a 60 Hz) : largement plus que les deux IRQ
	-- SPU necessaires (~0,65 s par chunk) pour ecrire les deux demi-buffers.
	if phase == 3 then
		gameFrames = gameFrames + 1
		if gameFrames == 180 then
			report('game', spu())
			phase = 4
			gameFrames = 0
		end
	end

	if phase == 4 then
		gameFrames = gameFrames + 1
		if gameFrames == 60 and not pulse then
			pad.setOverride(B.START)
			pulse = { button = B.START, stage = 1, releaseAt = frame + 14, doneAt = frame + 30 }
			phase = 5
		end
	end

	if phase == 5 and pulse == nil and invOpen() then
		phase = 6
		hold = frame
	end

	if phase == 6 and frame >= hold + 40 and not printed then
		printed = true
		report('inventory', spu())
		local s = spu()
		print(string.format('SPU_RESULT peakBytes=%d peakFrame=%d currentBytes=%d bankBytes=%d streamBytes=%d transfers=%d overlapBytes=%d rejections=%d chunks=%d underruns=%d frame=%d integrity=%s',
			s.peakBytes, s.peakFrame, s.currentBytes, s.bankBytes, s.streamBytes,
			s.transfers, s.overlapBytes, s.rejections, s.chunks, s.underruns,
			s.frame, hex(s.integrity)))
		PCSX.quit(0)
	end

	if frame > 16000 and not printed then
		print('CAPTURE_SPU: FAIL timeout phase=' .. phase .. ' inv=' .. tostring(invOpen()) ..
			' intro=' .. introFrame())
		PCSX.quit(1)
	end
end)

print('CAPTURE_SPU ready main=' .. hex(mainAddr) .. ' spuAddr=' .. hex(spuAddr) ..
	' introAddr=' .. hex(introAddr) .. ' menuAddr=' .. hex(menuAddr) ..
	' invAddr=' .. hex(invAddr) .. ' outdir=' .. outdir)
