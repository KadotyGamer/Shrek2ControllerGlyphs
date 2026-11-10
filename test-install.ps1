$ErrorActionPreference = 'Stop'
$fixture = Join-Path $PSScriptRoot 'build\install-fixture'
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
$fixtureExe = Join-Path $fixture 'Shrek.exe'
Copy-Item -LiteralPath 'C:\Program Files (x86)\Steam\steamapps\common\Shrek2Remastered\Shrek.exe' -Destination $fixtureExe
$before = (Get-FileHash -LiteralPath $fixtureExe).Hash
& (Join-Path $PSScriptRoot 'Install.ps1') -GameDirectory $fixture
$refused = $false
try { & (Join-Path $PSScriptRoot 'Install.ps1') -GameDirectory $fixture } catch { $refused = $true }
if (-not $refused) { throw 'Installer did not refuse an existing DLL.' }
& (Join-Path $PSScriptRoot 'Install.ps1') -GameDirectory $fixture -Uninstall
if (Test-Path -LiteralPath (Join-Path $fixture 'XInput1_4.dll')) { throw 'Uninstall left the DLL.' }
if ((Get-FileHash -LiteralPath $fixtureExe).Hash -ne $before) { throw 'Original executable changed.' }
Remove-Item -LiteralPath $fixtureExe
Remove-Item -LiteralPath $fixture
Write-Output 'PASS: install, overwrite refusal, uninstall and original executable preservation.'
