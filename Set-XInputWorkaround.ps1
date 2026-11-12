param(
    [string]$GameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Shrek2Remastered',
    [switch]$Restore
)
# Controller-only experimental workaround for CL 583916. No glyph changes.
# Keyboard/mouse gameplay may be unavailable. Enable Steam Input for DualSense.
$ErrorActionPreference = 'Stop'
$originalHash = '5C2EDA7616B845E4DEAFE1387FE703FC75012E1854E759840E707C1FA5C25A29'
$patchedHash = '5224E4C657B27D51134071686754A48DDA21D3D0787F92B35DB72EC148C961A6'
$gameRoot = (Resolve-Path -LiteralPath $GameDirectory).Path
$exePath = Join-Path $gameRoot 'Shrek.exe'
$backupRoot = Join-Path $PSScriptRoot 'backups\xinput-CL583916'
$backupPath = Join-Path $backupRoot 'Shrek-CL583916-original.exe'
if (Get-Process -Name Shrek -ErrorAction SilentlyContinue) { throw 'Close Shrek before applying or restoring.' }
$currentHash = (Get-FileHash -LiteralPath $exePath).Hash
if ($Restore) {
    if ($currentHash -eq $originalHash) { Write-Output 'Already original; nothing changed.'; return }
    if ($currentHash -ne $patchedHash) { throw 'Game changed or updated; refusing to replace it with an older backup.' }
    if (-not (Test-Path -LiteralPath $backupPath) -or (Get-FileHash -LiteralPath $backupPath).Hash -ne $originalHash) { throw 'Verified original backup missing.' }
    Copy-Item -LiteralPath $backupPath -Destination $exePath -Force
    if ((Get-FileHash -LiteralPath $exePath).Hash -ne $originalHash) { throw 'Restoration verification failed.' }
    Write-Output 'Original CL 583916 executable restored.'
    return
}
if ($currentHash -ne $originalHash) { throw 'Requires the exact original CL 583916 executable. No files changed.' }
[byte[]]$bytes = [IO.File]::ReadAllBytes($exePath)
if ($bytes.Length -ne 6995456 -or $bytes[0x2cc380] -ne 0x75 -or $bytes[0x2cc381] -ne 0x27 -or $bytes[0x2cc49a] -ne 0x74 -or $bytes[0x2cc49b] -ne 0x18) { throw 'Instruction validation failed.' }
# Fall through to LuxXInputInterface construction instead of GameInput.
$bytes[0x2cc380] = 0x90
$bytes[0x2cc381] = 0x90
# Skip the GameInput-only window handler for the XInput object.
$bytes[0x2cc49a] = 0xeb
$sha = [Security.Cryptography.SHA256]::Create()
try { $resultHash = [BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-','') } finally { $sha.Dispose() }
if ($resultHash -ne $patchedHash) { throw 'Patched executable hash verification failed; nothing written.' }
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
if (Test-Path -LiteralPath $backupPath) {
    if ((Get-FileHash -LiteralPath $backupPath).Hash -ne $originalHash) { throw 'Existing backup differs; refusing to overwrite it.' }
} else { Copy-Item -LiteralPath $exePath -Destination $backupPath }
try {
    [IO.File]::WriteAllBytes($exePath,$bytes)
    if ((Get-FileHash -LiteralPath $exePath).Hash -ne $patchedHash) { throw 'Installed patch verification failed.' }
} catch {
    Copy-Item -LiteralPath $backupPath -Destination $exePath -Force
    throw
}
Write-Output 'Experimental XInput workaround applied. Enable Steam Input. Keyboard/mouse gameplay may be unavailable.'
Write-Output 'Run this script with -Restore to undo. Do not reuse after a game update.'

