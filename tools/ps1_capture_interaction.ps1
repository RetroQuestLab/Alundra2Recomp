# Capture native de la premiere interaction PS1 (Paco / Prunewell) depuis une ISO precise.
# La navigation est pilote par les variables reelles du jeu dans le script Lua.
#
# Usage :
#   powershell -ExecutionPolicy Bypass -File tools\ps1_capture_interaction.ps1 -SkipBuild
#   powershell -ExecutionPolicy Bypass -File tools\ps1_capture_interaction.ps1 -SkipBuild `
#     -Cue "C:\...\ps1\a_graver\v0.4.0-interaction\alundra2_ntsc.iso" -OutDir "qa\ps1_logs\interaction_shots_iso_v040"
#
# Sortie : "PS1_CAPTURE_INTERACTION: PASS" (exit 0).

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
	Write-Host "PS1_CAPTURE_INTERACTION: FAIL - $message"
	exit 1
}

if (-not $SdkRoot) { $SdkRoot = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32' }
$sdkBin = Join-Path $SdkRoot 'bin'
$nmExe = Join-Path $sdkBin 'mipsel-none-elf-nm.exe'
$ps1Dir = Join-Path $repo 'ps1'
$elf = Join-Path $ps1Dir 'build\alundra2.elf'
if (-not $Cue) { $Cue = Join-Path $ps1Dir 'build\alundra2_ps1.cue' }
$lua = Join-Path $repo 'docs\ps1\lua\capture_interaction.lua'
if (-not $OutDir) { $OutDir = Join-Path $repo 'qa\ps1_logs\interaction_shots' }
if (-not $PcsxExe) { $PcsxExe = Join-Path $repo 'external\tools\pcsx-redux\pcsx-redux.main' }
if (-not $BiosFile) { $BiosFile = Join-Path $env:LOCALAPPDATA 'DuckStation\bios\openbios.bin' }

foreach ($required in @($nmExe, $PcsxExe, $BiosFile, $lua, $elf, $Cue)) {
	if (-not (Test-Path $required)) { Fail "outil/ressource absent: $required" }
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if (-not $SkipBuild) {
	& powershell -ExecutionPolicy Bypass -File (Join-Path $repo 'tools\build_ps1.ps1')
	if ($LASTEXITCODE -ne 0) { Fail 'build PS1' }
}

$env:Path = $sdkBin + [IO.Path]::PathSeparator + $env:Path
$nm = & $nmExe -n $elf

function Get-Symbol([string]$suffix, [string]$label) {
	$line = $nm | Where-Object { $_ -match $suffix } | Select-Object -Last 1
	if (-not $line) { Fail "$label absent de l ELF" }
	$m = [regex]::Match($line, '([0-9a-fA-F]{8})\s+\S+\s+\S+$')
	if (-not $m.Success) { Fail "adresse $label illisible: $line" }
	return '0x' + $m.Groups[1].Value.ToLowerInvariant()
}

$mainAddr = Get-Symbol '\bmain$' 'main'
$env:SESSION_OUT = $OutDir
$env:PS1_MAIN_ADDR = $mainAddr
$env:PS1_INTRO_ADDR = Get-Symbol '7g_introE$' 'g_intro'
$env:PS1_HERO_X_ADDR = Get-Symbol '3g_xE$' 'g_x'
$env:PS1_HERO_Y_ADDR = Get-Symbol '3g_yE$' 'g_y'
$env:PS1_QUEST_ADDR = Get-Symbol '7g_questE$' 'g_quest'
$env:PS1_DLG_SHOWN_ADDR = Get-Symbol '7g_shownE$' 'g_shown'
$env:PS1_DLG_PANEL_ADDR = Get-Symbol '7g_panelE$' 'g_panel'

$stdoutFile = Join-Path $OutDir 'capture_stdout.txt'
$stderrFile = Join-Path $OutDir 'capture_stderr.txt'
$argList = @(
	'-testmode', '-stdout', '-lua_stdout',
	'-bios', $BiosFile,
	'-iso', $Cue,
	'-fastboot',
	'-run', '-debugger', '-interpreter',
	'-portable', '-noupdate',
	'-dofile', $lua
)
@"
CMD=$PcsxExe $($argList -join ' ')
MAIN_ADDR=$mainAddr
CUE=$Cue
OUTDIR=$OutDir
"@ | Set-Content -LiteralPath (Join-Path $OutDir 'capture_meta.txt') -NoNewline

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

$ppmFiles = @(Get-ChildItem -LiteralPath $OutDir -Filter '*.ppm' -File)
if ($ppmFiles.Count -eq 0) { Fail 'aucune capture PPM produite' }
foreach ($ppm in $ppmFiles) {
	$png = [IO.Path]::ChangeExtension($ppm.FullName, '.png')
	& py -3.12 (Join-Path $repo 'tools\ppm_to_png.py') $ppm.FullName $png
	if ($LASTEXITCODE -ne 0) { Fail "conversion PNG de $($ppm.Name)" }
}

$stdout -split "`n" | Where-Object { $_ -match 'CAPTURE main|shot |INTERACT_CAPTURE' } | ForEach-Object { Write-Host $_ }
if ($stdout -notmatch 'INTERACT_CAPTURE: PASS') { Fail 'la sequence interaction n est pas allee a son terme' }
foreach ($name in @('paco_avant_parole', 'prunewell_ouverture', 'paco_apres_prunewell',
                    'paco_garde_revele', 'garde_ouverture', 'garde_accents',
                    'paco_quete_terminee')) {
	if ($stdout -notmatch ([regex]::Escape("shot $name "))) { Fail "capture absente: $name" }
}
Write-Host '--- PNG ---'
Get-ChildItem -LiteralPath $OutDir -Filter '*.png' -File | ForEach-Object { Write-Host "$($_.Name) $($_.Length) octets" }
Write-Host 'PS1_CAPTURE_INTERACTION: PASS'
exit 0
