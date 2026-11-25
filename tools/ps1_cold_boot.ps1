# Demarrage a froid d'une image PS1 livree : BIOS complet, SANS -fastboot,
# sans chargement direct de l'EXE ni etat sauvegarde, aucune entree.
# Limite : seul OpenBIOS (libre) est disponible localement ; il ne reproduit
# pas tous les controles d'un BIOS Sony (ecran de licence notamment).
#
# Usage : powershell -ExecutionPolicy Bypass -File tools\ps1_cold_boot.ps1 -Lot v0.8.41-licence
# Sortie : "PS1_COLD_BOOT: PASS" si `main` puis le menu titre sont atteints.
[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)][string]$Lot,
	[string]$Shots = '300,900,1800,2700',
	[int]$Seconds = 400
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
function Fail([string]$m) { Write-Host "PS1_COLD_BOOT: FAIL - $m"; exit 1 }
$lotDir = Join-Path $repo "ps1\a_graver\$Lot"
$cue = Join-Path $lotDir 'alundra2_ps1.cue'
$elf = Join-Path $lotDir 'alundra2.elf'
$pcsx = Join-Path $repo 'external\tools\pcsx-redux\pcsx-redux.main'
$bios = Join-Path $env:LOCALAPPDATA 'DuckStation\bios\openbios.bin'
$nmExe = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32\bin\mipsel-none-elf-nm.exe'
$lua = Join-Path $repo 'docs\ps1\lua\cold_boot.lua'
foreach ($r in @($cue, $elf, $pcsx, $bios, $nmExe, $lua)) { if (-not (Test-Path $r)) { Fail "absent: $r" } }
$out = Join-Path $repo "qa\ps1_logs\cold_boot_$Lot"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$nm = & $nmExe -n $elf
function Sym([string]$pat) {
	$l = $nm | Where-Object { $_ -match $pat } | Select-Object -Last 1
	if (-not $l) { return '0' }
	return '0x' + [regex]::Match($l, '([0-9a-fA-F]{8})\s+\S+\s+\S+$').Groups[1].Value
}
$env:SESSION_OUT = $out
$env:PS1_MAIN_ADDR = Sym '\bmain$'
$env:PS1_MENU_DRAW_ADDR = Sym 'menuDrawPS1v$'
$env:COLD_SHOTS = $Shots
$args2 = @('-testmode', '-stdout', '-lua_stdout', '-bios', $bios, '-iso', $cue,
	'-run', '-debugger', '-interpreter', '-portable', '-noupdate', '-dofile', $lua)  # PAS de -fastboot
$so = Join-Path $out 'stdout.txt'; $se = Join-Path $out 'stderr.txt'
$p = Start-Process -FilePath $pcsx -ArgumentList $args2 -PassThru -WindowStyle Hidden -RedirectStandardOutput $so -RedirectStandardError $se
if (-not $p.WaitForExit($Seconds * 1000)) { $p.Kill(); Fail "timeout $Seconds s" }
$txt = Get-Content -LiteralPath $so -Raw
Get-ChildItem -LiteralPath $out -Filter '*.ppm' | ForEach-Object {
	& py -3.12 (Join-Path $repo 'tools\ppm_to_png.py') $_.FullName ([IO.Path]::ChangeExtension($_.FullName, '.png')) | Out-Null
}
$txt -split "`n" | Where-Object { $_ -match 'COLD_' } | ForEach-Object { Write-Host $_.Trim() }
if ($txt -notmatch 'COLD_MAIN vsync=') { Fail 'main du jeu jamais atteint' }
if ($txt -notmatch 'COLD_MENU vsync=') { Fail 'menu titre jamais dessine' }
Write-Host "Captures : $out"
Write-Host 'PS1_COLD_BOOT: PASS'
exit 0
