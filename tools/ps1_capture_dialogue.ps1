# Capture native du dialogue A1 (WIND.TX + FONT3.TIM, texte A2) sur la cible PS1.
# La chronometrie demarre a l'entree dans main ; les captures sont declenchees
# par GPU::Vsync (jamais depuis un breakpoint). Images PPM P6 -> PNG.
#
# Usage depuis la racine du depot :
#   powershell -ExecutionPolicy Bypass -File tools\ps1_capture_dialogue.ps1
#   powershell -ExecutionPolicy Bypass -File tools\ps1_capture_dialogue.ps1 -SkipBuild
#
# Sortie : "PS1_CAPTURE_DIALOGUE: PASS" (exit 0). Ecrit uniquement sous qa/ps1_logs/.

[CmdletBinding()]
param(
	[int]$Seconds = 420,
	[switch]$SkipBuild,
	[string]$SdkRoot,
	[string]$PcsxExe,
	[string]$BiosFile,
	[string]$Cue
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

function Fail([string]$message) {
	Write-Host "PS1_CAPTURE_DIALOGUE: FAIL - $message"
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
$lua = Join-Path $repo 'docs\ps1\lua\capture_dialogue.lua'
$outdir = Join-Path $repo 'qa\ps1_logs\dialogue_shots'

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

# Depuis v0.3.0, le dialogue est precede du titre, du menu et de l'intro.
# Le script Lua lit ces variables pour attendre le vrai debut du dialogue.
function Get-Symbol([string]$suffix, [string]$label) {
	$line = $nm | Where-Object { $_ -match $suffix } | Select-Object -Last 1
	if (-not $line) { Fail "$label absent de l ELF" }
	$m = [regex]::Match($line, '([0-9a-fA-F]{8})\s+\S+\s+\S+$')
	if (-not $m.Success) { Fail "adresse $label illisible: $line" }
	return '0x' + $m.Groups[1].Value.ToLowerInvariant()
}
$introAddr = Get-Symbol 'g_introE$' 'g_intro'
$shownAddr = Get-Symbol 'g_shownE$' 'g_shown'
$panelAddr = Get-Symbol 'g_panelE$' 'g_panel'

# --- 4. Lancement PCSX-Redux ----------------------------------------------
$env:SESSION_OUT = $outdir
$env:PS1_MAIN_ADDR = $mainAddr
$env:PS1_INTRO_ADDR = $introAddr
$env:PS1_DLG_SHOWN_ADDR = $shownAddr
$env:PS1_DLG_PANEL_ADDR = $panelAddr
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
@"
CMD=$cmdString
MAIN_ADDR=$mainAddr
INTRO_ADDR=$introAddr
DLG_SHOWN_ADDR=$shownAddr
DLG_PANEL_ADDR=$panelAddr
OUTDIR=$outdir
CUE=$cue
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
$stdout -split "`n" | Where-Object { $_ -match 'CAPTURE_DIALOGUE|CAPTURE main|shot dlg|DLG_CAPTURE' } | ForEach-Object { Write-Host $_ }
if ($stdout -notmatch 'DLG_CAPTURE: PASS') { Fail 'le dialogue n a pas ete atteint apres titre/menu/intro' }
if ($stdout -notmatch 'shot dlg_typing .*panel=0') { Fail 'dlg_typing n est pas un panneau 0' }
if ($stdout -notmatch 'shot dlg_panel1_full .*panel=0') { Fail 'dlg_panel1_full n est pas le panneau 0 complete' }
if ($stdout -notmatch 'shot dlg_panel3_full .*panel=2') { Fail 'dlg_panel3_full n est pas le panneau 2 complete' }
Write-Host '--- PNG ---'
Get-ChildItem -LiteralPath $outdir -Filter '*.png' -File | ForEach-Object { Write-Host "$($_.Name) $($_.Length) octets" }
Write-Host "PS1_CAPTURE_DIALOGUE: PASS"
exit 0
