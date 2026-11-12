param([string]$GameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Shrek2Remastered')
$ErrorActionPreference = 'Stop'
$originalHash = '5C2EDA7616B845E4DEAFE1387FE703FC75012E1854E759840E707C1FA5C25A29'
$patchedHash = '5224E4C657B27D51134071686754A48DDA21D3D0787F92B35DB72EC148C961A6'
$source = Join-Path $GameDirectory 'Shrek.exe'
$sourceHash = (Get-FileHash -LiteralPath $source).Hash
if ($sourceHash -notin @($originalHash,$patchedHash)) { throw 'Test requires CL 583916.' }
$fixture = Join-Path $PSScriptRoot ('build\workaround-fixture-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
# Run a copy of the script so all test backups stay inside the fixture.
$script = Join-Path $fixture 'Set-XInputWorkaround.ps1'
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Set-XInputWorkaround.ps1') -Destination $script
$exe = Join-Path $fixture 'Shrek.exe'
[byte[]]$bytes = [IO.File]::ReadAllBytes($source)
if ($sourceHash -eq $patchedHash) {
    $bytes[0x2cc380]=0x75; $bytes[0x2cc381]=0x27; $bytes[0x2cc49a]=0x74
}
[IO.File]::WriteAllBytes($exe,$bytes)
if ((Get-FileHash -LiteralPath $exe).Hash -ne $originalHash) { throw 'Original fixture mismatch.' }
& $script -GameDirectory $fixture
if ((Get-FileHash -LiteralPath $exe).Hash -ne $patchedHash) { throw 'Patch failed.' }
& $script -GameDirectory $fixture -Restore
if ((Get-FileHash -LiteralPath $exe).Hash -ne $originalHash) { throw 'Restore failed.' }
# A new/altered build must be rejected for both apply and restore without writes.
$bytes[0x2cc380]=0x74
[IO.File]::WriteAllBytes($exe,$bytes)
$alteredHash = (Get-FileHash -LiteralPath $exe).Hash
foreach ($restore in @($false,$true)) {
    $refused = $false
    try { & $script -GameDirectory $fixture -Restore:$restore } catch { $refused=$true }
    if (-not $refused -or (Get-FileHash -LiteralPath $exe).Hash -ne $alteredHash) { throw 'Altered-build guard failed.' }
}
if ((Get-FileHash -LiteralPath $source).Hash -ne $sourceHash) { throw 'Actual game changed.' }
Write-Output 'PASS: XInput patch, exact restoration, changed-build rejection, actual game untouched.'
