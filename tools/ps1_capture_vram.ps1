# Mesure du pic VRAM reel pendant le rendu PS1 sur l'ISO livree.
# Le Lua lit g_ps1VramDiag (table d'occupation instrumentee) sans ecrire dans le
# POD ; ce script prouve la provenance (SHA256) et la coherence des relevs.
#
# Usage depuis la racine du depot :
#   powershell -ExecutionPolicy Bypass -File tools\ps1_capture_vram.ps1 `
#     -Iso ps1\a_graver\v0.8.21-ps1-vram-peak\alundra2_ntsc.iso `
#     -Elf ps1\a_graver\v0.8.21-ps1-vram-peak\alundra2.elf `
#     -OutDir qa\ps1_logs\vram_v0821 `
#     -ExpectedSha256 <SHA256 de l'ISO livree>
#
# Sortie : "PS1_CAPTURE_VRAM: PASS" (exit 0).

[CmdletBinding()]
param(
	[int]$Seconds = 300,
	[string]$SdkRoot,
	[string]$PcsxExe,
	[string]$BiosFile,
	[string]$Iso,
	[string]$Elf,
	[string]$OutDir,
	[string]$ExpectedSha256
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

function Fail([string]$message) {
	Write-Host "PS1_CAPTURE_VRAM: FAIL - $message"
	exit 1
}

if (-not $SdkRoot) { $SdkRoot = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32' }
$sdkBin = Join-Path $SdkRoot 'bin'
$nmExe = Join-Path $sdkBin 'mipsel-none-elf-nm.exe'

if (-not $Iso) { $Iso = 'ps1\a_graver\v0.8.21-ps1-vram-peak\alundra2_ntsc.iso' }
if (-not $Elf) { $Elf = 'ps1\a_graver\v0.8.21-ps1-vram-peak\alundra2.elf' }
$isoPath = if ([IO.Path]::IsPathRooted($Iso)) { $Iso } else { Join-Path $repo $Iso }
$elfPath = if ([IO.Path]::IsPathRooted($Elf)) { $Elf } else { Join-Path $repo $Elf }
$lua = Join-Path $repo 'docs\ps1\lua\capture_vram.lua'
if (-not $OutDir) { $OutDir = 'qa\ps1_logs\vram_v0821' }
$outdir = if ([IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $repo $OutDir }
New-Item -ItemType Directory -Force -Path $outdir | Out-Null

if (-not $PcsxExe) { $PcsxExe = Join-Path $repo 'external\tools\pcsx-redux\pcsx-redux.main' }
if (-not $BiosFile) { $BiosFile = Join-Path $env:LOCALAPPDATA 'DuckStation\bios\openbios.bin' }

foreach ($required in @($nmExe, $PcsxExe, $BiosFile, $lua, $elfPath, $isoPath)) {
	if (-not (Test-Path $required)) { Fail "outil/ressource absent: $required" }
}

$isoHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $isoPath).Hash
$elfHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $elfPath).Hash
if ($ExpectedSha256 -and $ExpectedSha256.ToUpperInvariant() -ne $isoHash) {
	Fail "ISO_SHA256 attendu=$($ExpectedSha256.ToUpperInvariant()) reel=$isoHash"
}

# --- Adresses via nm sur l'ELF LIVRE ----------------------------------------
$env:Path = $sdkBin + [IO.Path]::PathSeparator + $env:Path
$nm = & $nmExe -n $elfPath
if ($LASTEXITCODE -ne 0) { Fail "echec nm sur $elfPath" }

function Get-Addr([string]$pattern, [string]$label) {
	$line = $nm | Where-Object { $_ -match $pattern } | Select-Object -Last 1
	if (-not $line) { Fail "symbole $label introuvable dans l'ELF livre" }
	$m = [regex]::Match($line, '([0-9a-fA-F]{8})\s+\S+\s+\S*$')
	if (-not $m.Success) { Fail "adresse $label illisible: $line" }
	return '0x' + $m.Groups[1].Value.ToLowerInvariant()
}

$mainAddr = Get-Addr '\bmain$' 'main'
$vramAddr = Get-Addr '\bg_ps1VramDiag$' 'g_ps1VramDiag'
$introAddr = Get-Addr 'g_introE$' 'g_intro'
$menuAddr = Get-Addr 'g_menuE$' 'g_menu'
$invAddr = Get-Addr '\bg_invCapture$' 'g_invCapture'

# --- Lancement PCSX-Redux sur l'ISO exacte ----------------------------------
$env:SESSION_OUT = $outdir
$env:PS1_MAIN_ADDR = $mainAddr
$env:PS1_VRAM_ADDR = $vramAddr
$env:PS1_INTRO_ADDR = $introAddr
$env:PS1_MENU_ADDR = $menuAddr
$env:PS1_INV_ADDR = $invAddr
$stdoutFile = Join-Path $outdir 'vram_stdout.txt'
$stderrFile = Join-Path $outdir 'vram_stderr.txt'

$argList = @(
	'-testmode', '-stdout', '-lua_stdout',
	'-bios', $BiosFile,
	'-iso', $isoPath,
	'-fastboot',
	'-run', '-debugger', '-interpreter',
	'-portable', '-noupdate',
	'-dofile', $lua
)
$cmdString = $PcsxExe + ' ' + ($argList -join ' ')

$p = Start-Process -FilePath $PcsxExe -ArgumentList $argList -PassThru -WindowStyle Hidden `
	-RedirectStandardOutput $stdoutFile -RedirectStandardError $stderrFile

$timeoutMs = [Math]::Max($Seconds, 5) * 1000
if (-not $p.WaitForExit($timeoutMs)) {
	$p.Kill(); $p.WaitForExit(5000) | Out-Null
	Fail "timeout apres $Seconds secondes"
}
if ($p.ExitCode -ne $null -and $p.ExitCode -ne 0) { Fail "PCSX-Redux code de sortie $($p.ExitCode)" }

$stdout = Get-Content -LiteralPath $stdoutFile -Raw
$stderr = Get-Content -LiteralPath $stderrFile -Raw
$errors = @($stderr -split "`n" | Where-Object { $_ -match 'E/(System|BIOS|CDROM|CDrom)|Failed to boot|Could not load' })
if ($errors.Count -gt 0) { Fail "erreurs PCSX-Redux: $($errors -join ' | ')" }

function Get-Field([string]$line, [string]$name) {
	$m = [regex]::Match($line, "(?:^|\s)${name}=([^\s]+)")
	if (-not $m.Success) { Fail "champ $name absent: $line" }
	return $m.Groups[1].Value
}

function Get-Hex([string]$line, [string]$name) {
	$v = Get-Field $line $name
	if ($v -notmatch '^0x[0-9a-fA-F]+$') { Fail "champ $name n'est pas hexadécimal: $v" }
	return [Convert]::ToUInt64($v.Substring(2), 16)
}

$allLines = @($stdout -split "`n")
if (@($allLines | Where-Object { $_ -match '^CAPTURE_VRAM: FAIL' }).Count -gt 0) {
	Fail "echec signale par le Lua: $(@($allLines | Where-Object { $_ -match '^CAPTURE_VRAM: FAIL' }) -join ' | ')"
}

$phaseLines = @($allLines | Where-Object { $_ -match '^VRAM phase=' })
$resultLines = @($allLines | Where-Object { $_ -match '^VRAM_RESULT ' })
if ($phaseLines.Count -ne 3) { Fail "lignes VRAM phase: $($phaseLines.Count) attendu 3" }
if ($resultLines.Count -ne 1) { Fail "lignes VRAM_RESULT: $($resultLines.Count) attendu 1" }

$expectedPhases = @('menu', 'game', 'inventory')
$kMagic = [Convert]::ToUInt64('A2565241', 16)
$lastPeak = -1
foreach ($i in 0..2) {
	$line = $phaseLines[$i]
	$phase = Get-Field $line 'phase'
	$magic = Get-Hex $line 'magic'
	$integrity = Get-Hex $line 'integrity'
	$ready = [int](Get-Field $line 'ready')
	$uploads = [int64](Get-Field $line 'uploads')
	$overlap = [int64](Get-Field $line 'overlapHalfwords')
	$current = [int64](Get-Field $line 'currentBytes')
	$peak = [int64](Get-Field $line 'peakBytes')
	$peakFrame = [int64](Get-Field $line 'peakFrame')
	$fb = [int64](Get-Field $line 'framebufferBytes')
	$texture = [int64](Get-Field $line 'texturePeakBytes')
	$rejections = [int](Get-Field $line 'rejections')
	$firstFrame = [int64](Get-Field $line 'firstFrame')
	$lastFrame = [int64](Get-Field $line 'lastFrame')
	$vram = [int64](Get-Field $line 'vramBytes')
	$halfwords = [int64](Get-Field $line 'halfwords')

	if ($phase -ne $expectedPhases[$i]) { Fail "phase=$phase attendu $($expectedPhases[$i])" }
	if ($magic -ne $kMagic) { Fail "magic inattendu: 0x$('{0:x8}' -f $magic)" }
	if ($ready -ne 1) { Fail "ready=$ready a la phase $phase" }
	if ($vram -ne 1048576) { Fail "vramBytes=$vram attendu 1048576" }
	if ($halfwords -ne 524288) { Fail "halfwords=$halfwords attendu 524288" }
	if ($fb -ne 307200) { Fail "framebufferBytes=$fb attendu 307200" }
	if ($rejections -ne 0) { Fail "rejections=$rejections a la phase $phase" }
	if ($uploads -le 0) { Fail "uploads=$uploads a la phase $phase" }
	if ($peak -lt $fb -or $peak -gt $vram) { Fail "peakBytes=$peak hors plage [$fb,$vram]" }
	if ($current -ne $peak) { Fail "currentBytes=$current != peakBytes=$peak (table sans liberation)" }
	if ($texture -ne ($peak - $fb)) { Fail "texturePeakBytes=$texture attendu $($peak - $fb)" }
	if ($firstFrame -le 0) { Fail "firstFrame=$firstFrame incoherent" }
	if ($lastFrame -lt $firstFrame) { Fail "lastFrame=$lastFrame < firstFrame=$firstFrame" }
	if ($peakFrame -le 0 -or $peakFrame -gt $lastFrame) { Fail "peakFrame=$peakFrame hors [1,$lastFrame]" }
	if ($peak -lt $lastPeak) { Fail "pic non monotone: $peak < $lastPeak" }
	$lastPeak = $peak

	$computed = $kMagic -bxor [uint64]$uploads -bxor [uint64]$current -bxor [uint64]$peak -bxor [uint64]$overlap
	if ($integrity -ne $computed) {
		Fail "integrity=0x$('{0:x8}' -f $integrity) attendu 0x$('{0:x8}' -f $computed) a la phase $phase"
	}
}

$result = $resultLines[0]
$resultPeak = [int64](Get-Field $result 'peakBytes')
$inventoryPeak = [int64](Get-Field $phaseLines[2] 'peakBytes')
if ($resultPeak -ne $inventoryPeak) {
	Fail "VRAM_RESULT peakBytes=$resultPeak != phase inventory peakBytes=$inventoryPeak"
}

$vramLog = Join-Path $outdir 'vram_capture.txt'
@"
PS1_CAPTURE_VRAM: PASS
CMD=$cmdString
ISO=$isoPath
ISO_SHA256=$isoHash
ELF=$elfPath
ELF_SHA256=$elfHash
MAIN_ADDR=$mainAddr
VRAM_ADDR=$vramAddr
INTRO_ADDR=$introAddr
MENU_ADDR=$menuAddr
INV_ADDR=$invAddr
LUA=$lua
$($phaseLines | Out-String)
$($resultLines | Out-String)
VERDIT: pic VRAM measure=$inventoryPeak octets, texture=$($inventoryPeak - 307200) octets, uploads=$uploads, overlapHalfwords=$overlap, rejections=0.
"@ | Set-Content -LiteralPath $vramLog -Encoding UTF8

Write-Host '--- extrait stdout ---'
$phaseLines | ForEach-Object { Write-Host $_ }
$resultLines | ForEach-Object { Write-Host $_ }
Write-Host '----------------------'
Write-Host "Adresses : main=$mainAddr g_ps1VramDiag=$vramAddr"
Write-Host "ISO : $isoPath ($isoHash)"
Write-Host "Log : $vramLog"
Write-Host 'PS1_CAPTURE_VRAM: PASS'
exit 0
