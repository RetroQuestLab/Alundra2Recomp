# Capture native du jalon PS1 v0.6.0b : salle du trone 120, victoire de
# Mephisto et vraie fin. Boot de l'ISO exacte passee en -Cue.
#
# Usage :
#   powershell -ExecutionPolicy Bypass -File tools\ps1_capture_ending.ps1 -SkipBuild `
#     -Cue "C:\...\v0.6.0b-ending\alundra2_ntsc.iso" -OutDir "qa\ps1_logs\ending_shots_iso"
#
# Sortie : "ENDING_CAPTURE: PASS" (exit 0).

[CmdletBinding()]
param(
	[int]$Seconds = 480,
	[switch]$SkipBuild,
	[string]$SdkRoot,
	[string]$PcsxExe,
	[string]$BiosFile,
	[string]$Cue,
	[string]$OutDir,
	[string]$Lua,
	[string]$Marker = 'ENDING_CAPTURE',
	[string]$Label = 'ENDING_CAPTURE',
	[string[]]$Shots = @('ending_arrivee_trone', 'ending_salle_trone',
	                     'ending_mephisto_trone', 'ending_mephisto_vaincu',
	                     'ending_roi_vaincu', 'ending_alexia_vaincu',
	                     'ending_screen')
)

$ErrorActionPreference = 'Stop'
if ($Shots.Count -eq 1 -and $Shots[0] -like '*,*') { $Shots = $Shots[0] -split ',' }
$repo = Split-Path -Parent $PSScriptRoot

function Fail([string]$message) {
	Write-Host "$Label`: FAIL - $message"
	exit 1
}

if (-not $SdkRoot) { $SdkRoot = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32' }
$sdkBin = Join-Path $SdkRoot 'bin'
$nmExe = Join-Path $sdkBin 'mipsel-none-elf-nm.exe'
$ps1Dir = Join-Path $repo 'ps1'
$elf = Join-Path $ps1Dir 'build\alundra2.elf'
if (-not $Cue) { $Cue = Join-Path $ps1Dir 'build\alundra2_ps1.cue' }
if (-not $Lua) { $Lua = Join-Path $repo 'docs\ps1\lua\capture_ending.lua' }
$lua = $Lua
if (-not $OutDir) { $OutDir = Join-Path $repo 'qa\ps1_logs\ending_shots' }
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
$env:PS1_DIR_ADDR = Get-Symbol '5g_dirE$' 'g_dir'
$env:PS1_QUEST_ADDR = Get-Symbol '7g_questE$' 'g_quest'
$env:PS1_DLG_SHOWN_ADDR = Get-Symbol '7g_shownE$' 'g_shown'
$env:PS1_DLG_PANEL_ADDR = Get-Symbol '7g_panelE$' 'g_panel'
$captureZoneAddr = Get-Symbol '16g_ps1CaptureZoneE$' 'g_ps1CaptureZone'
$env:PS1_CAPTURE_ZONE_ADDR = $captureZoneAddr

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
CAPTURE_ZONE_ADDR=$captureZoneAddr
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

$stdout -split "`n" | Where-Object { $_ -match 'CAPTURE main|shot |CAPTURE: PASS|CAPTURE: FAIL' } | ForEach-Object { Write-Host $_ }
if ($stdout -notmatch ([regex]::Escape($Marker + ': PASS'))) { Fail 'la sequence de capture n est pas allee a son terme' }
foreach ($name in $Shots) {
	if ($stdout -notmatch ([regex]::Escape("shot $name "))) { Fail "capture absente: $name" }
}
Write-Host '--- PNG ---'
Get-ChildItem -LiteralPath $OutDir -Filter '*.png' -File | ForEach-Object { Write-Host "$($_.Name) $($_.Length) octets" }
Write-Host "$Label`: PASS"
exit 0
