param(
	[Parameter(Mandatory = $true)][string]$Lot,
	[int]$Seconds = 90
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$lotDir = Join-Path $repo "ps1\a_graver\$Lot"
$lotElf = Join-Path $lotDir 'alundra2.elf'
$lotCue = Join-Path $lotDir 'alundra2_ps1.cue'
$liveElf = Join-Path $repo 'ps1\build\alundra2.elf'
$backup = Join-Path $repo 'ps1\build\alundra2.audit_backup.elf'

foreach ($required in @($lotElf, $lotCue, $liveElf)) {
	if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "fichier absent: $required" }
}
$liveHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $liveElf).Hash
Copy-Item -LiteralPath $liveElf -Destination $backup -Force
try {
	Copy-Item -LiteralPath $lotElf -Destination $liveElf -Force
	powershell -ExecutionPolicy Bypass -File (Join-Path $repo 'tools\ps1_smoke.ps1') -Seconds $Seconds -SkipBuild -Cue $lotCue
	$exitCode = $LASTEXITCODE
} finally {
	Copy-Item -LiteralPath $backup -Destination $liveElf -Force
	Remove-Item -LiteralPath $backup -Force
}
$restored = (Get-FileHash -Algorithm SHA256 -LiteralPath $liveElf).Hash
if ($restored -ne $liveHash) { throw "ELF courant non restaure: $restored != $liveHash" }
Write-Output "LOT=$Lot ELF_RESTORE_SHA256=$restored"
exit $exitCode
