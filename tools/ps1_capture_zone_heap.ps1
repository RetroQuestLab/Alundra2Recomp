# Mesure du pic heap PS1 par zone sur l'ISO livree : sequence 011 -> 013 -> 026 -> 011.
# Le Lua ecrit dans g_ps1ZoneHeapDiag ; ce script prouve la provenance (SHA256),
# verifie les quatre etapes, la liberation explicite et le retour au baseline.
#
# Usage depuis la racine du depot :
#   powershell -ExecutionPolicy Bypass -File tools\ps1_capture_zone_heap.ps1 `
#     -Iso ps1\a_graver\v0.8.20-ps1-zone-heap\alundra2_ntsc.iso `
#     -Elf ps1\a_graver\v0.8.20-ps1-zone-heap\alundra2.elf `
#     -OutDir qa\ps1_logs\zone_heap_v0820
#
# Sortie : "PS1_CAPTURE_ZONE_HEAP: PASS" (exit 0).

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
	Write-Host "PS1_CAPTURE_ZONE_HEAP: FAIL - $message"
	exit 1
}

if (-not $SdkRoot) { $SdkRoot = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32' }
$sdkBin = Join-Path $SdkRoot 'bin'
$nmExe = Join-Path $sdkBin 'mipsel-none-elf-nm.exe'

if (-not $Iso) { $Iso = 'ps1\a_graver\v0.8.20-ps1-zone-heap\alundra2_ntsc.iso' }
if (-not $Elf) { $Elf = 'ps1\a_graver\v0.8.20-ps1-zone-heap\alundra2.elf' }
$isoPath = if ([IO.Path]::IsPathRooted($Iso)) { $Iso } else { Join-Path $repo $Iso }
$elfPath = if ([IO.Path]::IsPathRooted($Elf)) { $Elf } else { Join-Path $repo $Elf }
$lua = Join-Path $repo 'docs\ps1\lua\capture_zone_heap.lua'
if (-not $OutDir) { $OutDir = 'qa\ps1_logs\zone_heap_v0820' }
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
$zoneDiagAddr = Get-Addr '\bg_ps1ZoneHeapDiag$' 'g_ps1ZoneHeapDiag'
$introAddr = Get-Addr 'g_introE$' 'g_intro'
$menuAddr = Get-Addr 'g_menuE$' 'g_menu'
$zoneCmd = '0x5a5a5a5a'

# --- Lancement PCSX-Redux sur l'ISO exacte ----------------------------------
$env:SESSION_OUT = $outdir
$env:PS1_MAIN_ADDR = $mainAddr
$env:PS1_ZONE_DIAG_ADDR = $zoneDiagAddr
$env:PS1_INTRO_ADDR = $introAddr
$env:PS1_MENU_ADDR = $menuAddr
$env:PS1_ZONE_CMD = $zoneCmd
$stdoutFile = Join-Path $outdir 'zone_heap_stdout.txt'
$stderrFile = Join-Path $outdir 'zone_heap_stderr.txt'

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

$lines = @($stdout -split "`n" | Where-Object { $_ -match '^ZONE_HEAP' })
if (@($lines | Where-Object { $_ -match '^ZONE_HEAP_FAIL:' }).Count -gt 0) {
	Fail "echec signale par le Lua: $(@($lines | Where-Object { $_ -match '^ZONE_HEAP_FAIL:' }) -join ' | ')"
}

function Get-Field([string]$line, [string]$name) {
	$m = [regex]::Match($line, "(?:^|\s)${name}=([^\s]+)")
	if (-not $m.Success) { Fail "champ $name absent: $line" }
	return $m.Groups[1].Value
}

$steps = @($lines | Where-Object { $_ -match '^ZONE_HEAP step=' })
$results = @($lines | Where-Object { $_ -match '^ZONE_HEAP_RESULT ' })
if ($steps.Count -ne 4) { Fail "lignes ZONE_HEAP step: $($steps.Count) attendu 4" }
if ($results.Count -ne 1) { Fail "lignes ZONE_HEAP_RESULT: $($results.Count) attendu 1" }

$expectedZones = @(13, 26, 11, 0)
for ($i = 0; $i -lt 4; ++$i) {
	$line = $steps[$i]
	$step = [int](Get-Field $line 'step')
	$zone = [int](Get-Field $line 'zone')
	$alloc = [int64](Get-Field $line 'alloc')
	$peak = [int64](Get-Field $line 'peak')
	$bytes = [int64](Get-Field $line 'bytes')
	$releases = [int](Get-Field $line 'releases')
	$errCount = [int](Get-Field $line 'errors')
	$magic = Get-Field $line 'magic'
	if ($step -ne ($i + 1)) { Fail "step=$step attendu $($i + 1)" }
	if ($zone -ne $expectedZones[$i]) { Fail "zone=$zone attendu $($expectedZones[$i])" }
	if ($magic -ne '0xa25a5a5a') { Fail "magic inattendu: $magic" }
	if ($errCount -ne 0) { Fail "errors=$errCount a l'etape $step" }
	if ($peak -lt $alloc) { Fail "peak=$peak < alloc=$alloc a l'etape $step" }
	if ($i -lt 3) {
		if ($bytes -le 0) { Fail "bytes=$bytes a l'etape $step" }
	} else {
		if ($releases -ne 3) { Fail "releaseCount final=$releases attendu 3" }
	}
}

$result = $results[0]
$loadCount = [int](Get-Field $result 'loadCount')
$releaseCount = [int](Get-Field $result 'releaseCount')
$baselineAlloc = [int64](Get-Field $result 'baselineAlloc')
$finalAlloc = [int64](Get-Field $result 'finalAlloc')
$staticRam = [int64](Get-Field $result 'staticRam')
if ($loadCount -ne 3) { Fail "loadCount=$loadCount attendu 3" }
if ($releaseCount -ne 3) { Fail "releaseCount=$releaseCount attendu 3" }
if ($finalAlloc -ne $baselineAlloc) { Fail "fuite: finalAlloc=$finalAlloc baselineAlloc=$baselineAlloc" }
if ($staticRam -le 0) { Fail "staticRam=$staticRam incoherent" }

$zoneLog = Join-Path $outdir 'zone_heap_capture.txt'
@"
PS1_CAPTURE_ZONE_HEAP: PASS
CMD=$cmdString
ISO=$isoPath
ISO_SHA256=$isoHash
ELF=$elfPath
ELF_SHA256=$elfHash
MAIN_ADDR=$mainAddr
ZONE_DIAG_ADDR=$zoneDiagAddr
INTRO_ADDR=$introAddr
MENU_ADDR=$menuAddr
ZONE_CMD=$zoneCmd
LUA=$lua
$($lines | Out-String)
VERDIT: 4 etapes, 3 chargements, 3 liberations explicites, baselineAlloc=$baselineAlloc, finalAlloc=$finalAlloc, staticRam=$staticRam.
"@ | Set-Content -LiteralPath $zoneLog -Encoding UTF8

Write-Host '--- extrait stdout ---'
$lines | ForEach-Object { Write-Host $_ }
Write-Host '----------------------'
Write-Host "Adresses : main=$mainAddr g_ps1ZoneHeapDiag=$zoneDiagAddr"
Write-Host "ISO : $isoPath ($isoHash)"
Write-Host "Log : $zoneLog"
Write-Host 'PS1_CAPTURE_ZONE_HEAP: PASS'
exit 0
