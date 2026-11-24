# Mesure du temps d'image reel PS1 (Root Timer RCntCNT1) sur une ISO precise.
#
# Usage :
#   powershell -ExecutionPolicy Bypass -File tools\ps1_frame_timing.ps1 -SkipBuild
#   powershell -ExecutionPolicy Bypass -File tools\ps1_frame_timing.ps1 -SkipBuild `
#     -Cue "ps1\a_graver\v0.8.6-frame-timing\alundra2_ntsc.iso"
#
# Sortie : "PS1_FRAME_TIMING: PASS" (exit 0). Logs sous qa/ps1_logs/.

[CmdletBinding()]
param(
	[int]$Seconds = 240,
	[switch]$SkipBuild,
	[string]$SdkRoot,
	[string]$PcsxExe,
	[string]$BiosFile,
	[string]$Cue,
	[string]$OutDir
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

function Fail([string]$message) {
	Write-Host "PS1_FRAME_TIMING: FAIL - $message"
	exit 1
}

if (-not $SdkRoot) { $SdkRoot = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32' }
$sdkBin = Join-Path $SdkRoot 'bin'
$nmExe = Join-Path $sdkBin 'mipsel-none-elf-nm.exe'
$ps1Dir = Join-Path $repo 'ps1'
$elf = Join-Path $ps1Dir 'build\alundra2.elf'
$bin = Join-Path $ps1Dir 'build\alundra2_ps1.bin'
if (-not $Cue) { $Cue = Join-Path $ps1Dir 'build\alundra2_ps1.cue' }
$lua = Join-Path $repo 'docs\ps1\lua\frame_timing.lua'
if (-not $OutDir) { $OutDir = Join-Path $repo 'qa\ps1_logs\frame_timing' }
if (-not $PcsxExe) { $PcsxExe = Join-Path $repo 'external\tools\pcsx-redux\pcsx-redux.main' }
if (-not $BiosFile) { $BiosFile = Join-Path $env:LOCALAPPDATA 'DuckStation\bios\openbios.bin' }

foreach ($required in @($nmExe, $PcsxExe, $BiosFile, $lua, $elf, $bin, $Cue)) {
	if (-not (Test-Path $required)) { Fail "outil/ressource absent: $required" }
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if (-not $SkipBuild) {
	& powershell -ExecutionPolicy Bypass -File (Join-Path $repo 'tools\build_ps1.ps1')
	if ($LASTEXITCODE -ne 0) { Fail 'build PS1' }
}

if ($Cue -match '\.iso$') {
	$iso = $Cue
} else {
	$iso = Join-Path $OutDir 'alundra2_ntsc.iso'
	& py -3.12 (Join-Path $repo 'tools\ps1_make_iso.py') $bin $iso
	if ($LASTEXITCODE -ne 0) { Fail 'conversion ISO' }
}

$env:Path = $sdkBin + [IO.Path]::PathSeparator + $env:Path
$nm = & $nmExe -n $elf
if ($LASTEXITCODE -ne 0) { Fail "echec de nm sur l'ELF" }
function Get-SymbolAddress([string]$pattern) {
	$line = $nm | Where-Object { $_ -match $pattern } | Select-Object -Last 1
	if (-not $line) { Fail "symbole absent de l ELF: $pattern" }
	$addrMatch = [regex]::Match($line, '^([0-9a-fA-F]{16})\s')
	if (-not $addrMatch.Success) { Fail "adresse illisible: $line" }
	$a = $addrMatch.Groups[1].Value
	if ($a -match '^ffffffff([0-9a-fA-F]{8})$') { $a = $matches[1] }
	return '0x' + $a.ToLowerInvariant()
}
$env:PS1_TIMING_ADDR = Get-SymbolAddress 'g_ps1FrameTiming$'
$env:PS1_MAIN_ADDR = Get-SymbolAddress ' T main$'
$env:PS1_START_ADDR = Get-SymbolAddress ' _start$'

$stdoutFile = Join-Path $OutDir 'timing_stdout.txt'
$stderrFile = Join-Path $OutDir 'timing_stderr.txt'
$argList = @(
	'-testmode', '-stdout', '-lua_stdout',
	'-bios', $BiosFile,
	'-iso', $iso,
	'-fastboot', '-run', '-debugger', '-interpreter',
	'-portable', '-noupdate',
	'-dofile', $lua
)
$isoHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $iso).Hash
@"
CMD=$PcsxExe $($argList -join ' ')
ISO=$iso
IMAGE=$Cue
SHA256=$isoHash
TIMING=$env:PS1_TIMING_ADDR
MAIN=$env:PS1_MAIN_ADDR
START=$env:PS1_START_ADDR
"@ | Set-Content -LiteralPath (Join-Path $OutDir 'timing_meta.txt') -NoNewline

Get-Process pcsx-redux.main -ErrorAction SilentlyContinue | Stop-Process -Force
$p = Start-Process -FilePath $PcsxExe -ArgumentList $argList -PassThru -WindowStyle Hidden `
	-RedirectStandardOutput $stdoutFile -RedirectStandardError $stderrFile
$timeoutMs = [Math]::Max($Seconds, 5) * 1000
if (-not $p.WaitForExit($timeoutMs)) {
	$p.Kill()
	$p.WaitForExit(5000) | Out-Null
	Fail "timeout apres $Seconds secondes"
}

$stdout = Get-Content -LiteralPath $stdoutFile -Raw
$stderr = Get-Content -LiteralPath $stderrFile -Raw
$errors = @($stderr -split "`n" | Where-Object { $_ -match 'E/(System|BIOS|CDROM|CDrom)|Failed to boot|Could not load BIOS|Could not load CD' })
if ($errors.Count -gt 0) { Fail "erreurs PCSX-Redux: $($errors -join ' | ')" }

$stdout -split "`n" | Where-Object { $_ -match 'FRAME_TIMING' } | ForEach-Object { Write-Host $_ }
if ($stdout -notmatch 'FRAME_TIMING: PASS') { Fail 'la sonde temps d image n est pas allee a son terme' }
if ($stdout -notmatch 'FRAME_TIMING samples=1000') { Fail 'echantillonnage incomplet' }
Write-Host 'PS1_FRAME_TIMING: PASS'
exit 0
