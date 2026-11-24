-- Sonde temporaire : etat du pilote carte OpenBIOS et du slot SIO0.
-- Usage : pcsx-redux -dofile card_probe.lua (avec PS1_MAIN_ADDR).

local mainAddr = tonumber(os.getenv('PS1_MAIN_ADDR') or '0x80017070')
local mem = PCSX.getMemPtr()
local function rd8(a) return mem[a - 0x80000000] end
local started = false
local frame = 0

print('CARD_PROBE SIO0 keys:')
for k, v in pairs(PCSX.SIO0) do print('CARD_PROBE SIO0.' .. tostring(k) .. '=' .. tostring(v)) end
local slot1 = PCSX.SIO0.slots[1]
print('CARD_PROBE slot1 keys:')
for k, v in pairs(slot1) do print('CARD_PROBE slot1.' .. tostring(k) .. '=' .. tostring(v)) end

PCSX.addBreakpoint(mainAddr, 'Exec', 4, 'card probe main reached', function()
	started = true
	print('CARD_PROBE main reached')
	return false
end, 'card probe main reached')

vsyncListener = PCSX.Events.createEventListener('GPU::Vsync', function()
	frame = frame + 1
	if frame <= 3 then print('CARD_PROBE vsync f=' .. frame .. ' started=' .. tostring(started)) end
	if not started then return end
	if frame <= 12 or frame % 20 == 0 then
		print(string.format('CARD_PROBE f=%d st0=%02x st1=%02x busy=%02x last=%02x',
			frame, rd8(0x8004b2c), rd8(0x8004b2d), rd8(0x8004b3c), rd8(0x8004b34)))
	end
	if frame > 120 then PCSX.quit(0) end
end)

print('CARD_PROBE ready main=0x' .. string.format('%08x', mainAddr))
