# Mesure du pic heap reel et de la profondeur de pile MIPS sur l'ISO PS1 livree.
# Le harnais lit g_ps1Diag depuis la RAM emulee apres >= 200 frames de gameplay.
# Aucun fichier du jeu n'est modifie ; aucun screenshot n'est produit.
#
# Usage depuis la racine du depot :
#   powershell -ExecutionPolicy Bypass -File tools\ps1_capture_diag.ps1
#   powershell -ExecutionPolicy Bypass -File tools\ps1_capture_diag.ps1 -Iso "ps1\a_graver\<lot>\alundra2_ntsc.iso"
#
# Sortie : "PS1_CAPTURE_DIAG: PASS" (exit 0). Ecrit sous qa/ps1_logs/harness_v0819/.

[CmdletBinding()]
param(
	[int]$Seconds = 300,
	[string]$SdkRoot,
	[string]$PcsxExe,
	[string]$BiosFile,
	[string]$Iso,
	[string]$Elf,
	[string]$OutDir
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

function Fail([string]$message) {
	Write-Host "PS1_CAPTURE_DIAG: FAIL - $message"
	exit 1
}

# --- 1. Chemins et outils --------------------------------------------------
if (-not $SdkRoot) { $SdkRoot = Join-Path $repo 'external\tools\psn00bsdk\PSn00bSDK-0.24-win32' }
$sdkBin = Join-Path $SdkRoot 'bin'
$nmExe = Join-Path $sdkBin 'mipsel-none-elf-nm.exe'

if (-not $Iso) { $Iso = 'ps1\a_graver\v0.8.19-ps1-heap-stack-diag\alundra2_ntsc.iso' }
if (-not $Elf) { $Elf = 'ps1\a_graver\v0.8.19-ps1-heap-stack-diag\alundra2.elf' }
$isoPath = if ([IO.Path]::IsPathRooted($Iso)) { $Iso } else { Join-Path $repo $Iso }
$elfPath = if ([IO.Path]::IsPathRooted($Elf)) { $Elf } else { Join-Path $repo $Elf }
$lua = Join-Path $repo 'docs\ps1\lua\capture_diag.lua'
if (-not $OutDir) { $OutDir = 'qa\ps1_logs\harness_v0819' }
$outdir = if ([IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $repo $OutDir }
New-Item -ItemType Directory -Force -Path $outdir | Out-Null

if (-not $PcsxExe) { $PcsxExe = Join-Path $repo 'external\tools\pcsx-redux\pcsx-redux.main' }
if (-not $BiosFile) { $BiosFile = Join-Path $env:LOCALAPPDATA 'DuckStation\bios\openbios.bin' }

foreach ($required in @($nmExe, $PcsxExe, $BiosFile, $lua, $elfPath, $isoPath)) {
	if (-not (Test-Path $required)) { Fail "outil/ressource absent: $required" }
}

# --- 2. Adresses via nm sur l'ELF LIVRE ------------------------------------
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
$diagAddr = Get-Addr '\bg_ps1Diag$' 'g_ps1Diag'
$introAddr = Get-Addr 'g_introE$' 'g_intro'
$menuAddr = Get-Addr 'g_menuE$' 'g_menu'

# --- 3. Lancement PCSX-Redux sur l'ISO exacte ------------------------------
$env:SESSION_OUT = $outdir
$env:PS1_MAIN_ADDR = $mainAddr
$env:PS1_DIAG_ADDR = $diagAddr
$env:PS1_INTRO_ADDR = $introAddr
$env:PS1_MENU_ADDR = $menuAddr
$stdoutFile = Join-Path $outdir 'diag_capture_stdout.txt'
$stderrFile = Join-Path $outdir 'diag_capture_stderr.txt'

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
$isoHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $isoPath).Hash
$elfHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $elfPath).Hash

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

# --- 4. Assertions ---------------------------------------------------------
$lines = $stdout -split "`n"
$diagLine = $lines | Where-Object { $_ -match '^DIAG ' } | Select-Object -Last 1
if (-not $diagLine) { Fail 'ligne DIAG absente du stdout Lua' }
$finalLine = $lines | Where-Object { $_ -match '^DIAG_FINAL ' } | Select-Object -Last 1
if (-not $finalLine) { Fail 'ligne DIAG_FINAL absente du stdout Lua' }

function Get-Field([string]$line, [string]$name) {
	$m = [regex]::Match($line, "(?:^|\s)${name}=([^\s]+)")
	if (-not $m.Success) { Fail "champ $name absent: $line" }
	return $m.Groups[1].Value
}

$magic = Get-Field $diagLine 'magic'
$heapPeak = [int](Get-Field $diagLine 'heapPeak')
$stackDepthMax = [int](Get-Field $diagLine 'stackDepthMax')
$stackFloor = Get-Field $diagLine 'stackFloor'
$stackTop = Get-Field $diagLine 'stackTop'
$samples = [int](Get-Field $diagLine 'samples')
$gameFramesLine = $lines | Where-Object { $_ -match 'CAPTURE_DIAG gameplay started' } | Select-Object -Last 1
if (-not $gameFramesLine) { Fail 'le jeu na pas demarre (phase gameplay absente)' }

if ($magic -ne '0xa2503144') { Fail "magic inattendu: $magic" }
if ($samples -lt 200) { Fail "sampleCount=$samples < 200" }

# Le haut de pile reel est fourni par InitHeap du SDK. Ne pas le figer sur une
# valeur supposee : verifier la plage RAM KUSEG et la coherence interne.
$topVal = [Convert]::ToUInt32($stackTop.Substring(2), 16)
$floorVal = [Convert]::ToUInt32($stackFloor.Substring(2), 16)
$paintLoVal = [Convert]::ToUInt32((Get-Field $finalLine 'stackPaintLo').Substring(2), 16)
$heapTotal = [int](Get-Field $finalLine 'heapTotal')
$kuseLo = [Convert]::ToUInt32('80100000', 16)  # evite le literal 0x... signe en Int32
$kuseHi = [Convert]::ToUInt32('80200000', 16)
if ($topVal -lt $kuseLo -or $topVal -gt $kuseHi) { Fail "stackTop hors RAM KUSEG: $stackTop" }
if ($heapPeak -lt 0 -or $heapPeak -gt $heapTotal) { Fail "heapPeak=$heapPeak incoherent avec heapTotal=$heapTotal" }
if ($floorVal -lt $paintLoVal) { Fail "stackFloor $stackFloor sous stackPaintLo" }
if (($topVal - $floorVal) -ne $stackDepthMax) {
	Fail "stackDepthMax=$stackDepthMax != stackTop-stackFloor=$($topVal - $floorVal)"
}
if ($stackDepthMax -le 0) { Fail "stackDepthMax=$stackDepthMax : aucune pile touchée" }

$diagLog = Join-Path $outdir 'diag_capture.txt'
@"
PS1_CAPTURE_DIAG: PASS
CMD=$cmdString
ISO=$isoPath
ISO_SHA256=$isoHash
ELF=$elfPath
ELF_SHA256=$elfHash
MAIN_ADDR=$mainAddr
DIAG_ADDR=$diagAddr
INTRO_ADDR=$introAddr
MENU_ADDR=$menuAddr
LUA=$lua
$($lines | Where-Object { $_ -match '^CAPTURE_DIAG|^DIAG' } | Out-String)
VERDIT: magic=$magic, $samples echantillons, heapPeak=$heapPeak octets, stackDepthMax=$stackDepthMax octets, stackFloor=$stackFloor, stackTop=$stackTop.
"@ | Set-Content -LiteralPath $diagLog -Encoding UTF8

Write-Host '--- extrait stdout ---'
$lines | Where-Object { $_ -match '^CAPTURE_DIAG|^DIAG' } | ForEach-Object { Write-Host $_ }
Write-Host '----------------------'
Write-Host "Adresses : main=$mainAddr g_ps1Diag=$diagAddr g_intro=$introAddr g_menu=$menuAddr"
Write-Host "ISO : $isoPath ($isoHash)"
Write-Host "Log : $diagLog"
Write-Host 'PS1_CAPTURE_DIAG: PASS'
exit 0
