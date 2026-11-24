# Sonde SPU native sur une ISO precise : execution du pilote + reliture DMA +
# declenchements A2 (curseur, ouverture/fermeture d'inventaire).
#
# Usage :
#   powershell -ExecutionPolicy Bypass -File tools\ps1_audio_probe.ps1 -SkipBuild
#   powershell -ExecutionPolicy Bypass -File tools\ps1_audio_probe.ps1 -SkipBuild `
#     -Cue "C:\...\ps1\a_graver\v0.8.1-audio-spu\alundra2_ntsc.iso"
#
# Sortie : "PS1_AUDIO_PROBE: PASS" (exit 0). Les logs sont sous qa/ps1_logs/.

[CmdletBinding()]
param(
	[int]$Seconds = 480,
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
	Write-Host "PS1_AUDIO_PROBE: FAIL - $message"
	exit 1
}

if (-not $SdkRoot) { $SdkRoot = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32' }
$sdkBin = Join-Path $SdkRoot 'bin'
$nmExe = Join-Path $sdkBin 'mipsel-none-elf-nm.exe'
$ps1Dir = Join-Path $repo 'ps1'
$elf = Join-Path $ps1Dir 'build\alundra2.elf'
$bin = Join-Path $ps1Dir 'build\alundra2_ps1.bin'
if (-not $Cue) { $Cue = Join-Path $ps1Dir 'build\alundra2_ps1.cue' }
$lua = Join-Path $repo 'docs\ps1\lua\audio_probe.lua'
if (-not $OutDir) { $OutDir = Join-Path $repo 'qa\ps1_logs\audio_probe' }
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

# L'ISO gravable est une vraie conversion ISO 2048 du BIN MODE2/2352.
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

function Get-Symbol([string]$symbol) {
	$pattern = '^\s*[0-9a-fA-F]{16}\s+\S+\s+' + [regex]::Escape($symbol) + '\s*$'
	$line = $nm | Where-Object { $_ -match $pattern } | Select-Object -Last 1
	if (-not $line) { Fail "symbole $symbol absent de l ELF" }
	$m = [regex]::Match($line, '^([0-9a-fA-F]{16})')
	if (-not $m.Success) { Fail "adresse $symbol illisible: $line" }
	$a = $m.Groups[1].Value
	if ($a -match '^ffffffff([0-9a-fA-F]{8})$') { $a = $matches[1] }
	return '0x' + $a.ToLowerInvariant()
}

$env:PS1_MAIN_ADDR        = Get-Symbol 'main'
$env:PS1_AUDIO_INIT_ADDR  = Get-Symbol 'audioInitPS1'
$env:PS1_AUDIO_MUSIC_ADDR = Get-Symbol 'audioPlayMusicPS1'
$env:PS1_AUDIO_SFX_ADDR   = Get-Symbol 'audioPlaySfxIdPS1'
$env:PS1_AUDIO_DIAG_ADDR  = Get-Symbol 'g_ps1AudioDiag'

function Get-SymbolSuffix([string]$suffix, [string]$label) {
	$line = $nm | Where-Object { $_ -match $suffix } | Select-Object -Last 1
	if (-not $line) { Fail "$label absent de l ELF" }
	$m = [regex]::Match($line, '([0-9a-fA-F]{8})\s+\S+\s+\S+$')
	if (-not $m.Success) { Fail "adresse $label illisible: $line" }
	return '0x' + $m.Groups[1].Value.ToLowerInvariant()
}

$env:PS1_INTRO_ADDR       = Get-SymbolSuffix '7g_introE$' 'g_intro'
$env:PS1_INV_ADDR         = Get-SymbolSuffix '\bg_invCapture$' 'g_invCapture'
$env:PS1_MENU_DRAW_ADDR   = Get-SymbolSuffix 'menuDrawPS1v$' 'menuDrawPS1'
$env:PS1_INTRO_DRAW_ADDR  = Get-SymbolSuffix 'introDrawPS1v$' 'introDrawPS1'
$env:PS1_MENU_ADDR        = Get-SymbolSuffix '6g_menuE$' 'g_menu'

$stdoutFile = Join-Path $OutDir 'audio_stdout.txt'
$stderrFile = Join-Path $OutDir 'audio_stderr.txt'
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
INIT=$env:PS1_AUDIO_INIT_ADDR
MUSIC=$env:PS1_AUDIO_MUSIC_ADDR
SFX=$env:PS1_AUDIO_SFX_ADDR
DIAG=$env:PS1_AUDIO_DIAG_ADDR
"@ | Set-Content -LiteralPath (Join-Path $OutDir 'audio_meta.txt') -NoNewline

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

$stdout -split "`n" | Where-Object { $_ -match 'AUDIO_' } | ForEach-Object { Write-Host $_ }
if ($stdout -notmatch 'AUDIO_PROBE: PASS') { Fail 'la sonde audio n est pas allee a son terme' }
foreach ($marker in @('AUDIO_BP init', 'AUDIO_BP music', 'AUDIO_DIAG ready=1', 'verify=1')) {
	if ($stdout -notmatch [regex]::Escape($marker)) { Fail "marqueur absent: $marker" }
}
foreach ($id in @('AUDIO_BP sfx id=1', 'AUDIO_BP sfx id=63', 'AUDIO_BP sfx id=3')) {
	if ($stdout -notmatch [regex]::Escape($id)) { Fail "bruitage absent: $id" }
}
Write-Host 'PS1_AUDIO_PROBE: PASS'
exit 0
