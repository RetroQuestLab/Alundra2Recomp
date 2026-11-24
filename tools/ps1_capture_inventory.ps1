# Capture native du menu armes / objets / equipement PS1 depuis une ISO precise.
# Navigation pilotee par les variables reelles du jeu (g_intro, g_invCapture).
#
# Usage :
#   powershell -ExecutionPolicy Bypass -File tools\ps1_capture_inventory.ps1 -SkipBuild
#   powershell -ExecutionPolicy Bypass -File tools\ps1_capture_inventory.ps1 -SkipBuild `
#     -Cue "C:\...\ps1\a_graver\v0.7.0-inventaire\alundra2_ntsc.iso" -OutDir "qa\ps1_logs\inventory_shots_iso"
#
# Sortie : "PS1_CAPTURE_INVENTORY: PASS" (exit 0).

[CmdletBinding()]
param(
	[int]$Seconds = 480,
	[switch]$SkipBuild,
	[string]$SdkRoot,
	[string]$PcsxExe,
	[string]$BiosFile,
	[string]$Cue,
	[string]$OutDir,
	[switch]$NoFastBoot
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

function Fail([string]$message) {
	Write-Host "PS1_CAPTURE_INVENTORY: FAIL - $message"
	exit 1
}

if (-not $SdkRoot) { $SdkRoot = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32' }
$sdkBin = Join-Path $SdkRoot 'bin'
$nmExe = Join-Path $sdkBin 'mipsel-none-elf-nm.exe'
$ps1Dir = Join-Path $repo 'ps1'
$elf = Join-Path $ps1Dir 'build\alundra2.elf'
if (-not $Cue) { $Cue = Join-Path $ps1Dir 'build\alundra2_ps1.cue' }
$lua = Join-Path $repo 'docs\ps1\lua\capture_inventory.lua'
if (-not $OutDir) { $OutDir = Join-Path $repo 'qa\ps1_logs\inventory_shots' }
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
$env:PS1_INV_ADDR = Get-Symbol '\bg_invCapture$' 'g_invCapture'
$env:PS1_MENU_DRAW_ADDR = Get-Symbol 'menuDrawPS1v$' 'menuDrawPS1'
$env:PS1_INTRO_DRAW_ADDR = Get-Symbol 'introDrawPS1v$' 'introDrawPS1'
$env:PS1_MENU_ADDR = Get-Symbol '6g_menuE$' 'g_menu'

$stdoutFile = Join-Path $OutDir 'capture_stdout.txt'
$stderrFile = Join-Path $OutDir 'capture_stderr.txt'
$argList = @(
	'-testmode', '-stdout', '-lua_stdout',
	'-bios', $BiosFile,
	'-iso', $Cue
)
if (-not $NoFastBoot) { $argList += '-fastboot' }
$argList += @(
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

$stdout -split "`n" | Where-Object { $_ -match 'CAPTURE main|shot |INVENTORY_CAPTURE' } | ForEach-Object { Write-Host $_ }
if ($stdout -notmatch 'INVENTORY_CAPTURE: PASS') { Fail 'la sequence inventaire n est pas allee a son terme' }
foreach ($name in @('inv_weapons', 'inv_items', 'inv_equipment', 'inv_closed')) {
	if ($stdout -notmatch ([regex]::Escape("shot $name "))) { Fail "capture absente: $name" }
}
Write-Host '--- PNG ---'
Get-ChildItem -LiteralPath $OutDir -Filter '*.png' -File | ForEach-Object { Write-Host "$($_.Name) $($_.Length) octets" }
Write-Host 'PS1_CAPTURE_INVENTORY: PASS'
exit 0
