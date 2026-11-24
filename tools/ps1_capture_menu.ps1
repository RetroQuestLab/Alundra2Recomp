# Capture native de l'ecran titre et du menu de depart PS1.
# La chronometrie demarre a l'entree dans main ; les captures sont declenchees
# par GPU::Vsync (jamais depuis un breakpoint). Images PPM P6 -> PNG.
#
# Usage depuis la racine du depot :
#   powershell -ExecutionPolicy Bypass -File tools\ps1_capture_menu.ps1
#   powershell -ExecutionPolicy Bypass -File tools\ps1_capture_menu.ps1 -SkipBuild
#
# Sortie : "PS1_CAPTURE_MENU: PASS" (exit 0). Ecrit uniquement sous qa/ps1_logs/.

[CmdletBinding()]
param(
	[int]$Seconds = 420,
	[switch]$SkipBuild,
	[string]$SdkRoot,
	[string]$PcsxExe,
	[string]$BiosFile,
	[string]$Cue,
	[string]$OutDir,
	[string]$ExpectedSha256
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

function Fail([string]$message) {
	Write-Host "PS1_CAPTURE_MENU: FAIL - $message"
	exit 1
}

# --- 1. Chemins et outils --------------------------------------------------
if (-not $SdkRoot) { $SdkRoot = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32' }
$sdkBin = Join-Path $SdkRoot 'bin'
$nmExe = Join-Path $sdkBin 'mipsel-none-elf-nm.exe'

$ps1Dir = Join-Path $repo 'ps1'
$elf = Join-Path $ps1Dir 'build\alundra2.elf'
if (-not $Cue) { $Cue = Join-Path $ps1Dir 'build\alundra2_ps1.cue' }
$cue = $Cue
$lua = Join-Path $repo 'docs\ps1\lua\capture_menu.lua'
if (-not $OutDir) { $OutDir = 'qa\ps1_logs\menu_shots' }
if ([IO.Path]::IsPathRooted($OutDir)) { $outdir = $OutDir } else { $outdir = Join-Path $repo $OutDir }

if (-not $PcsxExe) { $PcsxExe = Join-Path $repo 'external\tools\pcsx-redux\pcsx-redux.main' }
if (-not $BiosFile) { $BiosFile = Join-Path $env:LOCALAPPDATA 'DuckStation\bios\openbios.bin' }

foreach ($required in @($nmExe, $PcsxExe, $BiosFile, $lua, $elf, $cue)) {
	if (-not (Test-Path $required)) { Fail "outil/ressource absent: $required" }
}
New-Item -ItemType Directory -Force -Path $outdir | Out-Null

# --- 2. Build PS1 si demande ----------------------------------------------
if (-not $SkipBuild) {
	& powershell -ExecutionPolicy Bypass -File (Join-Path $repo 'tools\build_ps1.ps1')
	if ($LASTEXITCODE -ne 0) { Fail 'build PS1' }
}

# --- 3. Adresse de main depuis l'ELF (breakpoint du script Lua) ------------
$env:Path = $sdkBin + [IO.Path]::PathSeparator + $env:Path
$nm = & $nmExe -n $elf
$mainLine = $nm | Where-Object { $_ -match '\bmain$' } | Select-Object -Last 1
if (-not $mainLine) { Fail 'symbole main absent de l ELF' }
# nm affiche des adresses etendues (ffffffff800xxxxx) : on garde les 8 derniers hex.
$mainMatch = [regex]::Match($mainLine, '([0-9a-fA-F]{8})\s+\S+\s+main$')
if (-not $mainMatch.Success) { Fail "adresse de main illisible: $mainLine" }
$mainAddr = '0x' + $mainMatch.Groups[1].Value.ToLowerInvariant()
$startLine = $nm | Where-Object { $_ -match '\b_start$' } | Select-Object -Last 1
$startMatch = [regex]::Match($startLine, '([0-9a-fA-F]{8})\s+\S+\s+_start$')
if (-not $startMatch.Success) { Fail "adresse de _start illisible: $startLine" }
$startAddr = '0x' + $startMatch.Groups[1].Value.ToLowerInvariant()
# g_menu est dans un namespace anonyme : retrouver le symbole reel plutot que
# coder son adresse en dur (elle bouge a chaque ajout de code MIPS).
$menuLine = $nm | Where-Object { $_ -match 'g_menuE$' } | Select-Object -Last 1
$menuMatch = [regex]::Match($menuLine, '([0-9a-fA-F]{8})\s+\S+\s+\S*g_menuE$')
if (-not $menuMatch.Success) { Fail "adresse de g_menu illisible: $menuLine" }
$menuAddr = '0x' + $menuMatch.Groups[1].Value.ToLowerInvariant()

# --- 4. Lancement PCSX-Redux ----------------------------------------------
$env:SESSION_OUT = $outdir
$env:PS1_MAIN_ADDR = $mainAddr
$env:PS1_START_ADDR = $startAddr
$env:PS1_MENU_ADDR = $menuAddr
$stdoutFile = Join-Path $outdir 'capture_stdout.txt'
$stderrFile = Join-Path $outdir 'capture_stderr.txt'

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
$isoHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $cue).Hash
$shaMatch = 'N/A'
$want = ''
if ($ExpectedSha256) {
	$want = ($ExpectedSha256 -replace '\s', '').ToUpperInvariant()
	if ($isoHash.ToUpperInvariant() -ne $want) {
		Fail "hash ISO inattendu: $isoHash (attendu $want)"
	}
	$shaMatch = '1'
}
@"
CMD=$cmdString
MAIN_ADDR=$mainAddr
START_ADDR=$startAddr
MENU_ADDR=$menuAddr
OUTDIR=$outdir
CUE=$cue
SHA256=$isoHash
EXPECTED_SHA256=$want
SHA256_MATCH=$shaMatch
"@ | Set-Content -LiteralPath (Join-Path $outdir 'capture_meta.txt') -NoNewline

$p = Start-Process -FilePath $PcsxExe -ArgumentList $argList -PassThru -WindowStyle Hidden `
	-RedirectStandardOutput $stdoutFile -RedirectStandardError $stderrFile

$timeoutMs = [Math]::Max($Seconds, 5) * 1000
if (-not $p.WaitForExit($timeoutMs)) {
	$p.Kill()
	$p.WaitForExit(5000) | Out-Null
	Fail "timeout apres $Seconds secondes : PCSX-Redux non termine"
}

$stdout = Get-Content -LiteralPath $stdoutFile -Raw
$stderr = Get-Content -LiteralPath $stderrFile -Raw
$errors = @($stderr -split "`n" | Where-Object { $_ -match 'E/(System|BIOS|CDROM|CDrom)|Failed to boot|Could not load BIOS|Could not load CD' })
if ($errors.Count -gt 0) { Fail "erreurs PCSX-Redux: $($errors -join ' | ')" }

# --- 5. Conversion PPM -> PNG ---------------------------------------------
$ppmFiles = @(Get-ChildItem -LiteralPath $outdir -Filter '*.ppm' -File)
if ($ppmFiles.Count -eq 0) { Fail 'aucune capture PPM produite' }
foreach ($ppm in $ppmFiles) {
	$png = [IO.Path]::ChangeExtension($ppm.FullName, '.png')
	& py -3.12 (Join-Path $repo 'tools\ppm_to_png.py') $ppm.FullName $png
	if ($LASTEXITCODE -ne 0) { Fail "conversion PNG de $($ppm.Name)" }
}

# --- 6. Verdict ------------------------------------------------------------
$stdout -split "`n" | Where-Object { $_ -match 'CAPTURE_MENU|CAPTURE main|shot menu|shot title' } | ForEach-Object { Write-Host $_ }
Write-Host '--- PNG ---'
Get-ChildItem -LiteralPath $outdir -Filter '*.png' -File | ForEach-Object { Write-Host "$($_.Name) $($_.Length) octets" }
Write-Host "PS1_CAPTURE_MENU: PASS"
exit 0
