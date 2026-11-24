-- Mesure passive du pic VRAM pendant le rendu PS1 : lecture de g_ps1VramDiag
-- (16 x u32) depuis la RAM emulee. Aucune ecriture dans le POD, aucune image,
-- aucun fichier disque. La navigation capture-only reproduit capture_diag.lua
-- puis ouvre l'inventaire avec un appui START emule.
--
-- Variables d'environnement : SESSION_OUT, PS1_MAIN_ADDR, PS1_VRAM_ADDR,
-- PS1_INTRO_ADDR, PS1_MENU_ADDR, PS1_INV_ADDR.

local outdir = os.getenv('SESSION_OUT') or '.'
local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x8001fe78')
local vramAddr = tonumber(os.getenv('PS1_VRAM_ADDR') or '0x801b0dc8')
local introAddr = tonumber(os.getenv('PS1_INTRO_ADDR') or '0x801b21f4')
local menuAddr = tonumber(os.getenv('PS1_MENU_ADDR') or '0x801b1818')
local invAddr = tonumber(os.getenv('PS1_INV_ADDR') or '0x801b0ddc')

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

local function vram()
	return {
		magic             = read32(vramAddr + 0),
		ready             = read32(vramAddr + 4),
		frame             = read32(vramAddr + 8),
		uploads           = read32(vramAddr + 12),
		overlapHalfwords  = read32(vramAddr + 16),
		currentBytes      = read32(vramAddr + 20),
		peakBytes         = read32(vramAddr + 24),
		peakFrame         = read32(vramAddr + 28),
		framebufferBytes  = read32(vramAddr + 32),
		texturePeakBytes  = read32(vramAddr + 36),
		rejections        = read32(vramAddr + 40),
		firstFrame        = read32(vramAddr + 44),
		lastFrame         = read32(vramAddr + 48),
		vramBytes         = read32(vramAddr + 52),
		halfwords         = read32(vramAddr + 56),
		integrity         = read32(vramAddr + 60),
	}
end

local function introFrame() return read32(introAddr) end
local function menuReady()
	return read32(menuAddr + 8) == 1 and read32(menuAddr + 12) == 5
end
local function invOpen() return bit.band(read32(invAddr), 255) ~= 0 end
local function hex(v) return string.format('0x%08x', math.floor(v)) end

local function report(label, s)
	print(string.format('VRAM phase=%s magic=%s ready=%d frame=%d uploads=%d overlapHalfwords=%d currentBytes=%d peakBytes=%d peakFrame=%d framebufferBytes=%d texturePeakBytes=%d rejections=%d firstFrame=%d lastFrame=%d vramBytes=%d halfwords=%d integrity=%s',
		label, hex(s.magic), s.ready, s.frame, s.uploads, s.overlapHalfwords,
		s.currentBytes, s.peakBytes, s.peakFrame, s.framebufferBytes,
		s.texturePeakBytes, s.rejections, s.firstFrame, s.lastFrame,
		s.vramBytes, s.halfwords, hex(s.integrity)))
end

bps[#bps + 1] = PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'capture vram: main reached',
	function()
		started = true
		local r = PCSX.getRegisters()
		print(string.format('CAPTURE_VRAM main reached pc=%08x ra=%08x sp=%08x gp=%08x',
			r.pc, r.GPR.n.ra, r.GPR.n.sp, r.GPR.n.gp))
		return false
	end,
	'capture vram: main reached')

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
		report('menu', vram())
		write32(menuAddr + 0, 4)   -- MENU_SCR_DONE
		write32(menuAddr + 36, 1)  -- MENU_ACT_NEW_GAME
		phase = 1
		print(string.format('CAPTURE_VRAM new game injected at vsync=%d', frame))
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
		report('game', vram())
		print(string.format('CAPTURE_VRAM gameplay started at vsync=%d', frame))
	end

	if phase == 3 then
		gameFrames = gameFrames + 1
		if gameFrames == 90 and not pulse then
			pad.setOverride(B.START)
			pulse = { button = B.START, stage = 1, releaseAt = frame + 14, doneAt = frame + 30 }
			phase = 4
		end
	end

	if phase == 4 and pulse == nil and invOpen() then
		phase = 5
		hold = frame
	end

	if phase == 5 and frame >= hold + 20 and not printed then
		printed = true
		report('inventory', vram())
		local s = vram()
		print(string.format('VRAM_RESULT peakBytes=%d peakFrame=%d texturePeakBytes=%d uploads=%d overlapHalfwords=%d rejections=%d',
			s.peakBytes, s.peakFrame, s.texturePeakBytes, s.uploads,
			s.overlapHalfwords, s.rejections))
		PCSX.quit(0)
	end

	if frame > 14000 and not printed then
		print('CAPTURE_VRAM: FAIL timeout phase=' .. phase .. ' inv=' .. tostring(invOpen()) ..
			' intro=' .. introFrame())
		PCSX.quit(1)
	end
end)

print('CAPTURE_VRAM ready main=' .. hex(mainAddr) .. ' vramAddr=' .. hex(vramAddr) ..
	' introAddr=' .. hex(introAddr) .. ' menuAddr=' .. hex(menuAddr) ..
	' invAddr=' .. hex(invAddr) .. ' outdir=' .. outdir)
