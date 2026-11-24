# Smoke test reproductible du jalon 2 (chaine PS1 native - preuve d'execution invitee).
# Remplace le smoke jalon 1 ("boot CD accepté" uniquement) par une preuve que notre
# code invite s'execute depuis le BIN/CUE via PCSX-Redux + points d'arret externes.
#
# Usage depuis la racine du dépôt :
#   powershell -ExecutionPolicy Bypass -File tools\ps1_smoke.ps1
#   powershell -ExecutionPolicy Bypass -File tools\ps1_smoke.ps1 -Seconds 120 -SkipBuild
#
# Sortie : "PS1_SMOKE: PASS" (exit 0) ou "PS1_SMOKE: FAIL ..." (exit 1).
# Aucun fichier du depot n'est modifie : seul ce fichier est ecrase ; les logs sont
# ajoutes uniquement sous qa/ps1_logs/.

[CmdletBinding()]
param(
	[int]$Seconds = 300,
	[switch]$SkipBuild,
	[string]$SdkRoot,
	[string]$PcsxExe,
	[string]$BiosFile,
	[string]$Cue
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

function Fail([string]$message) {
	Write-Host "PS1_SMOKE: FAIL - $message"
	exit 1
}

# --- 1. Chemins et outils --------------------------------------------------
if (-not $SdkRoot) { $SdkRoot = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32' }
$sdkBin = Join-Path $SdkRoot 'bin'
$nmExe = Join-Path $sdkBin 'mipsel-none-elf-nm.exe'

$ps1Dir = Join-Path $repo 'ps1'
$elf = Join-Path $ps1Dir 'build\alundra2.elf'
$exe = Join-Path $ps1Dir 'build\alundra2.exe'
if (-not $Cue) { $Cue = Join-Path $ps1Dir 'build\alundra2_ps1.cue' }
$cue = $Cue
$lua = Join-Path $repo 'docs\ps1\lua\trace_boot.lua'

if (-not $PcsxExe) { $PcsxExe = Join-Path $repo 'external\tools\pcsx-redux\pcsx-redux.main' }
if (-not $BiosFile) { $BiosFile = Join-Path $env:LOCALAPPDATA 'DuckStation\bios\openbios.bin' }

$env:PSN00BSDK_LIBS = Join-Path $SdkRoot 'lib\libpsn00b'
$env:Path = $sdkBin + [IO.Path]::PathSeparator + $env:Path

foreach ($required in @($nmExe, $PcsxExe, $BiosFile, $lua, $ps1Dir)) {
	if (-not (Test-Path $required)) { Fail "outil/ressource absent: $required" }
}

# --- 2. Build cible PS1 si demande ----------------------------------------
if (-not $SkipBuild) {
	Push-Location $ps1Dir
	try {
		cmake --preset default
		if ($LASTEXITCODE -ne 0) { Fail 'configuration CMake PS1' }
		cmake --build build
		if ($LASTEXITCODE -ne 0) { Fail 'build PS-X EXE / BIN-CUE' }
	}
	finally { Pop-Location }
}

foreach ($required in @($elf, $exe, $cue)) {
	if (-not (Test-Path $required)) { Fail "artefact PS1 introuvable: $required" }
}

# --- 3. Adresses depuis l'ELF via nm (normalisation ffffffff -> 0x800xxxxx)-
$nmOutput = & $nmExe -n $elf
if ($LASTEXITCODE -ne 0) { Fail "echec de nm sur l'ELF" }

function Get-SymbolAddr([string]$symbol) {
	# Le symbole doit etre le dernier jeton EXACT : '_start' ne doit pas
	# correspondre a '_heap_start' (faux negatif observe le 2026-11-21).
	$pattern = '^\s*[0-9a-fA-F]{16}\s+\S+\s+' + [regex]::Escape($symbol) + '\s*$'
	$line = $nmOutput | Where-Object { $_ -match $pattern } | Select-Object -Last 1
	if (-not $line) { Fail "symbole $symbol introuvable dans l'ELF" }
	$m = [regex]::Match($line, '^([0-9a-fA-F]{16})')
	if (-not $m.Success) { Fail "pas d'adresse pour le symbole $symbol" }
	$a = $m.Groups[1].Value
	if ($a -match '^ffffffff([0-9a-fA-F]{8})$') { $a = '0x' + $matches[1] }
	else { $a = '0x' + $a }
	return $a
}

$entryAddr    = Get-SymbolAddr '_start'
$mainAddr     = Get-SymbolAddr 'main'
$pfInitAddr   = Get-SymbolAddr 'pfInit'
$pfPresentAddr= Get-SymbolAddr 'pfPresent'
$pfDrawRectAddr= Get-SymbolAddr 'pfDrawRect'

$entryAddrHex = $entryAddr -replace '^0x',''

$env:PS1_ENTRY_ADDR      = $entryAddr
$env:PS1_MAIN_ADDR       = $mainAddr
$env:PS1_PFINIT_ADDR     = $pfInitAddr
$env:PS1_PFPRESENT_ADDR  = $pfPresentAddr
$env:PS1_PFDRAW_ADDR     = $pfDrawRectAddr
$env:PS1_SUMMARY_FRAME   = '400'
$env:PS1_QUIT_FRAME      = '2500'

# --- 4. Fichiers de log horodatés + .meta ---------------------------------
$ts = Get-Date -Format 'yyyyMMdd_HHmmss'
$logDir = Join-Path $repo 'qa\ps1_logs'
if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Force -Path $logDir | Out-Null }
$stdoutFile = Join-Path $logDir ("redux_ps1_smoke_" + $ts + ".stdout.txt")
$stderrFile = Join-Path $logDir ("redux_ps1_smoke_" + $ts + ".stderr.txt")
$metaFile   = Join-Path $logDir ("redux_ps1_smoke_" + $ts + ".meta.txt")

$exeHash   = (Get-FileHash -Algorithm SHA256 -LiteralPath $exe).Hash
$elfHash   = (Get-FileHash -Algorithm SHA256 -LiteralPath $elf).Hash
$isoHash   = (Get-FileHash -Algorithm SHA256 -LiteralPath $cue).Hash

$emuVersion = 'inconnue'
$versionJson = Join-Path (Split-Path -Parent $PcsxExe) 'version.json'
if (Test-Path $versionJson) {
	$vj = Get-Content -LiteralPath $versionJson -Raw | ConvertFrom-Json
	$emuVersion = $vj.version
}

$argList = @(
	'-testmode', '-stdout', '-lua_stdout',
	'-bios', $BiosFile,
	'-iso', $cue,
	'-fastboot',
	'-run', '-debugger', '-interpreter',
	'-portable', '-noupdate',
	'-dofile', $lua
)
$cmdString = $PcsxExe + ' ' + ($argList -join ' ')

@"
TIME=$ts
CMD=$cmdString
EXE=$exe
EXE_SHA256=$exeHash
ELF=$elf
ELF_SHA256=$elfHash
BIOS=$BiosFile
EMULATOR=$PcsxExe ($emuVersion)
LUA=$lua
CUE=$cue
ISO_SHA256=$isoHash
PS1_SUMMARY_FRAME=400
PS1_QUIT_FRAME=2500
"@ | Set-Content -LiteralPath $metaFile -NoNewline

# --- 5. Lancement PCSX-Redux sur notre BIN/CUE (froid, instance unique) ---
$timeoutMs = [Math]::Max($Seconds, 5) * 1000
$p = Start-Process -FilePath $PcsxExe -ArgumentList $argList -PassThru -WindowStyle Hidden `
	-RedirectStandardOutput $stdoutFile -RedirectStandardError $stderrFile

if (-not $p.WaitForExit($timeoutMs)) {
	$p.Kill()
	$p.WaitForExit(5000) | Out-Null
	Fail "timeout appres $Seconds secondes : processus PCSX-Redux non termine (pas de PCSX.quit)"
}

# PCSX.quit(0) ne renseigne pas toujours un ExitCode exploitable ; on accepte
# une sortie propre (WaitForExit OK) quel que soit le code renseigne.
if ($p.ExitCode -ne $null -and $p.ExitCode -ne 0) { Fail "PCSX-Redux code de sortie non nul: $($p.ExitCode)" }

$stdout = Get-Content -LiteralPath $stdoutFile -Raw
$stderr = Get-Content -LiteralPath $stderrFile -Raw

$errors = @($stderr -split "`n" | Where-Object { $_ -match 'E/(System|BIOS|CDROM|CDrom)|Failed to boot|Could not load BIOS|Could not load CD' })
if ($errors.Count -gt 0) { Fail "erreurs PCSX-Redux: $($errors -join ' | ')" }

# --- 6. Assertions strictes -----------------------------------------------
# $stdout est une seule chaine multi-lignes : Select-String la renverrait
# entiere. On split explicitement pour ne garder que la ligne du BIOS.
$bootLine = ($stdout -split "`n") | Where-Object { $_ -match '^\s*boot file\s*:' } | Select-Object -Last 1
if (-not $bootLine) { Fail 'ligne "boot file" absente : le CD na pas charge notre EXE depuis le BIN/CUE' }

$hits = @{}
foreach ($line in ($stdout -split "`n")) {
	if ($line -match 'TRACE_BOOT (\S+) hits=(\d+)') {
		$hits[$matches[1]] = [int]$matches[2]
	}
}

$checks = @{
	'entry/_start' = [int]($hits['entry/_start'] - 0)
	'main'         = [int]($hits['main'] - 0)
	'pfInit'       = [int]($hits['pfInit'] - 0)
	'pfPresent'    = [int]($hits['pfPresent'] - 0)
	'pfDrawRect'   = [int]($hits['pfDrawRect'] - 0)
}

# entry/_start : le premier breakpoint peut etre manqué au démarrage a froid
# (débogueur pas encore arme quand le CPU saute a l'adresse d'entree).
# Preuve alternative : le BIOS a charge et execute a l'adresse symbole (EXEC:PC0).
$entryOk = $false
if ($checks['entry/_start'] -ge 1) { $entryOk = $true }
else {
	$execLine = $stdout | Select-String ('EXEC:PC0(' + $entryAddrHex + ')') -SimpleMatch | Select-Object -Last 1
	if ($execLine) { $entryOk = $true; Write-Host "Avertissement : entry/_start hits=0, mais EXEC:PC0($entryAddrHex) confirme l entree a l adresse symbole" }
}
if (-not $entryOk) { Fail 'entry/_start hits=0 et aucune EXEC:PC0 a l adresse d entree : notre code ne sest pas execute a l adresse symbole' }

foreach ($label in @('main','pfInit','pfPresent','pfDrawRect')) {
	if ($checks[$label] -le 0) { Fail "hit $label = 0 : notre code ne sest pas execute a cette adresse" }
}
if ($checks['main'] -lt 1)   { Fail 'main hits < 1' }
if ($checks['pfInit'] -lt 1) { Fail 'pfInit hits < 1' }
if ($checks['pfPresent'] -lt 1) { Fail 'pfPresent hits < 1' }
if ($checks['pfDrawRect'] -lt 1) { Fail 'pfDrawRect hits < 1' }

# --- 7. Verdict -----------------------------------------------------------
Write-Host "--- extrait stdout ---"
$stdout -split "`n" | Select-Object -Last 20 | ForEach-Object { Write-Host $_ }
Write-Host "----------------------"

Write-Host "Adresses ELF : _start=$entryAddr main=$mainAddr pfInit=$pfInitAddr pfPresent=$pfPresentAddr pfDrawRect=$pfDrawRectAddr"
Write-Host "Hits         : entry/_start=$($checks['entry/_start']) main=$($checks['main']) pfInit=$($checks['pfInit']) pfPresent=$($checks['pfPresent']) pfDrawRect=$($checks['pfDrawRect'])"
Write-Host "Boot file    : $bootLine"
Write-Host "PS1_SMOKE: PASS"
Write-Host "Preuves : notre code PS1 invite execute depuis le BIN/CUE sur CD (boot file present, _start/main/pfInit/pfPresent/pfDrawRect hits > 0, PCSX-Redux interpreter)."
Write-Host "Non prouve par ce smoke : entree manette visuelle, execution sur console physique PS1."
exit 0
