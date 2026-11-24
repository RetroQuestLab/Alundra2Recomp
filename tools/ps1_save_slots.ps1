# Alternance des slots de carte memoire PS1 sur une ISO figee.
# N sessions PCSX-Redux separees d'ecriture (defaut 2), puis une relecture
# apres arret/rerun complet. Avec -SaveCount 3, la troisieme ecriture doit
# revenir au slot 0 avec seq=3 (retournement complet).
# Verdict "PS1_SAVE_SLOT_ALTERNATION: PASS".
#
# Usage :
#   powershell -ExecutionPolicy Bypass -File tools\ps1_save_slots.ps1 -SkipBuild `
#     -Cue "ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ps1.cue" `
#     -Iso "ps1\a_graver\v0.8.22-ps1-spu-peak\alundra2_ntsc.iso" `
#     -ExpectedSha256 26B2228165B70922A121FDF6C2E33F979C1BB94D45B513E0B310B4496960BE54 `
#     -ExpectedElfSha256 2F8BCA0E3B8962CE6AD5F01FBDEA04031ED039322DFD63D696A9F88ADE144CA7 `
#     -ExpectedDigest 46f25a9f `
#     -OutDir "qa\ps1_logs\save_slots_v0822"
#
# Troisieme ecriture (retournement) :
#   ... -SaveCount 3 -ExpectedSeq3 3 -ExpectedSlot3 0 `
#     -OutDir "qa\ps1_logs\save_wraparound_v0822"

[CmdletBinding()]
param(
	[int]$Seconds = 180,
	[switch]$SkipBuild,
	[string]$SdkRoot,
	[string]$PcsxExe,
	[string]$BiosFile,
	[string]$Cue,
	[string]$Iso,
	[string]$ExpectedSha256,
	[string]$ExpectedElfSha256,
	[string]$ExpectedDigest = '46f25a9f',
	[ValidateRange(1, 3)]
	[int]$SaveCount = 2,
	[int]$ExpectedSeq1 = 1,
	[int]$ExpectedSlot1 = 0,
	[int]$ExpectedSeq2 = 2,
	[int]$ExpectedSlot2 = 1,
	[int]$ExpectedSeq3 = 3,
	[int]$ExpectedSlot3 = 0,
	[string]$OutDir
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

function Fail([string]$message) {
	Write-Host "PS1_SAVE_SLOT_ALTERNATION: FAIL - $message"
	exit 1
}

if (-not $SdkRoot) { $SdkRoot = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32' }
if (-not $PcsxExe) { $PcsxExe = Join-Path $repo 'external\tools\pcsx-redux\pcsx-redux.main' }
if (-not $BiosFile) { $BiosFile = Join-Path $env:LOCALAPPDATA 'DuckStation\bios\openbios.bin' }
if (-not $Cue) { $Cue = Join-Path $repo 'ps1\build\alundra2_ps1.cue' }
if (-not $Iso) { $Iso = Join-Path (Split-Path -Parent $Cue) 'alundra2_ntsc.iso' }
if (-not $OutDir) { $OutDir = Join-Path $repo 'qa\ps1_logs\save_slots_v0822' }
$elf = Join-Path $repo 'ps1\build\alundra2.elf'
$roundtrip = Join-Path $PSScriptRoot 'ps1_save_roundtrip.ps1'
$card = Join-Path $repo 'memcard1.mcd'

foreach ($required in @($PcsxExe, $BiosFile, $Cue, $Iso, $elf, $roundtrip, $card)) {
	if (-not (Test-Path $required)) { Fail "ressource absente: $required" }
}
$isoHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $Iso).Hash
$elfHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $elf).Hash
Write-Host "ISO_SHA256=$isoHash"
Write-Host "ELF_SHA256=$elfHash"
if ($ExpectedSha256 -and $ExpectedSha256.ToUpperInvariant() -ne $isoHash) {
	Fail "ISO_SHA256 attendu=$($ExpectedSha256.ToUpperInvariant()) reel=$isoHash"
}
if ($ExpectedElfSha256 -and $ExpectedElfSha256.ToUpperInvariant() -ne $elfHash) {
	Fail "ELF_SHA256 attendu=$($ExpectedElfSha256.ToUpperInvariant()) reel=$elfHash"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

# Attentes versionnees : seq et slot par ecriture. Le slot alterne 0,1,0,...
$expected = @(
	[pscustomobject]@{ Seq = $ExpectedSeq1; Slot = $ExpectedSlot1 },
	[pscustomobject]@{ Seq = $ExpectedSeq2; Slot = $ExpectedSlot2 },
	[pscustomobject]@{ Seq = $ExpectedSeq3; Slot = $ExpectedSlot3 }
)
# Le retournement n'a de sens que si une ecriture d'index >= 3 revient au slot 0.
if ($SaveCount -ge 3 -and $expected[2].Slot -ne $expected[0].Slot) {
	Fail "attente invalide: slot3=$($expected[2].Slot) different de slot1=$($expected[0].Slot)"
}

function Get-CardHash([string]$dir) {
	$stateFile = Join-Path $dir 'write_state.txt'
	if (-not (Test-Path $stateFile)) { Fail "etat d ecriture absent: $stateFile" }
	$state = Get-Content -LiteralPath $stateFile -Raw
	$hit = [regex]::Match($state, 'CARD_SHA256=([0-9A-Fa-f]{64})')
	if (-not $hit.Success) { Fail "CARD_SHA256 absent de $stateFile" }
	return $hit.Groups[1].Value.ToUpperInvariant()
}

function Invoke-Roundtrip([string]$name, [string[]]$extra) {
	$dir = Join-Path $OutDir $name
	New-Item -ItemType Directory -Force -Path $dir | Out-Null
	$argsList = @('-Seconds', "$Seconds", '-Cue', $Cue, '-Iso', $Iso,
		'-ExpectedSha256', $isoHash, '-ExpectedElfSha256', $elfHash,
		'-ExpectedDigest', $ExpectedDigest, '-ExpectedZone', '11',
		'-ExpectedX', '660', '-ExpectedY', '760',
		'-SdkRoot', $SdkRoot, '-PcsxExe', $PcsxExe, '-BiosFile', $BiosFile,
		'-OutDir', $dir)
	if ($SkipBuild) { $argsList += '-SkipBuild' }
	$argsList += $extra
	& powershell -ExecutionPolicy Bypass -File $roundtrip @argsList 2>&1 |
		Tee-Object -FilePath (Join-Path $dir 'run.txt') | Out-Null
	if ($LASTEXITCODE -ne 0) {
		Get-Content -LiteralPath (Join-Path $dir 'run.txt') -Tail 8 |
			ForEach-Object { Write-Host $_ }
		Fail "etape $name a echoue"
	}
	return $dir
}

$previousCard = $null
$lastSlot = -1
$lastSeq = -1
for ($i = 1; $i -le $SaveCount; $i++) {
	$exp = $expected[$i - 1]
	$extra = @('-WriteOnly', '-ExpectedSeq', "$($exp.Seq)", '-ExpectedSlot', "$($exp.Slot)")
	if ($i -eq 1) {
		$extra += '-ResetCard'
	} else {
		$extra += @('-ExpectedCardSha256', $previousCard)
	}
	$dir = Invoke-Roundtrip ('{0:d2}_write_slot{1}' -f $i, $exp.Slot) $extra
	$cardHash = Get-CardHash $dir
	if ($previousCard -and $cardHash -eq $previousCard) {
		Fail "ecriture $i n a pas modifie la carte"
	}
	Write-Host "ECRITURE_$i seq=$($exp.Seq) slot=$($exp.Slot) carte=$cardHash"
	$previousCard = $cardHash
	$lastSlot = $exp.Slot
	$lastSeq = $exp.Seq
}

$loadName = '{0:d2}_load_last' -f ($SaveCount + 1)
$dirLoad = Invoke-Roundtrip $loadName @('-LoadOnly',
	'-ExpectedCardSha256', $previousCard, '-ExpectedSeq', "$lastSeq")
# Le verdict PASS est emis par le roundtrip (run.txt de $dirLoad) ; la ligne
# d'etat demarrant (hasGame=1 seq=N) est lue dans le log de la session enfant.
$loadOut = Get-Content -LiteralPath (Join-Path $dirLoad 'run.txt') -Raw
$loadLog = Get-Content -LiteralPath (Join-Path $dirLoad (Join-Path '02_load_after_poweroff' 'stdout.txt')) -Raw
if ($loadOut -notmatch 'PS1_SAVE_ROUNDTRIP: PASS') { Fail 'relecture finale non confirmee' }
if ($loadLog -notmatch "PS1_SAVE_INIT READY hasGame=1 seq=$lastSeq") {
	Fail "sequence $lastSeq non detectee au demarrage final"
}

if ($SaveCount -ge 3) {
	if ($lastSlot -ne $expected[0].Slot) {
		Fail "slot final=$lastSlot different de slot1=$($expected[0].Slot)"
	}
	Write-Host "RETOURNEMENT slot=$lastSlot apres $SaveCount ecritures (slot initial=$($expected[0].Slot))"
}
Write-Host "SEQUENCE_FINALE seq=$lastSeq (superieur a $($expected[0].Seq))"
Write-Host "ETAT_RESTAURE digest=$ExpectedDigest"
Write-Host 'PS1_SAVE_SLOT_ALTERNATION: PASS'
exit 0
