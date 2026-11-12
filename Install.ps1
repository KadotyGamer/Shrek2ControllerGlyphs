param(
    [string]$GameDirectory = 'C:\Program Files (x86)\Steam\steamapps\common\Shrek2Remastered',
    [switch]$Uninstall
)
$ErrorActionPreference = 'Stop'
$gameRoot = (Resolve-Path -LiteralPath $GameDirectory).Path
$gameExe = Join-Path $gameRoot 'Shrek.exe'
if (-not (Test-Path -LiteralPath $gameExe)) { throw 'Shrek.exe was not found.' }
if (Get-Process -Name Shrek -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $gameExe }) {
    throw 'Close Shrek 2 Remastered before installing or uninstalling.'
}
$manifestPath = Join-Path $gameRoot 'ShrekControllerGlyphs.install.json'
$dllPath = Join-Path $gameRoot 'XInput1_4.dll'
$iniPath = Join-Path $gameRoot 'ShrekControllerGlyphs.ini'
if ($Uninstall) {
    if (-not (Test-Path -LiteralPath $manifestPath)) { throw 'No installation manifest found; nothing removed.' }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.Mod -ne 'ShrekControllerGlyphs') { throw 'Unrecognized installation manifest.' }
    if (Test-Path -LiteralPath $dllPath) {
        if ((Get-FileHash -LiteralPath $dllPath).Hash -ne $manifest.DllSHA256) { throw 'DLL changed since installation; nothing removed.' }
        Remove-Item -LiteralPath $dllPath
    }
    if ((Test-Path -LiteralPath $iniPath) -and (Get-FileHash -LiteralPath $iniPath).Hash -eq $manifest.IniSHA256) {
        Remove-Item -LiteralPath $iniPath
    }
    Remove-Item -LiteralPath $manifestPath
    Write-Output 'Glyph mod removed. The game executable, diagnostic log and any edited config were retained. Undo the optional XInput workaround separately.'
    exit
}
$supported = @(
    '00B67356582A4F3D900DB855224BA9379F9980DCCCFF3C89C3558CF0EEFB11CB',
    'AF5DF179E6725985B13A6E54E7B61AB73BF54635ED392A46E238F07BDFB73966',
    '5C2EDA7616B845E4DEAFE1387FE703FC75012E1854E759840E707C1FA5C25A29',
    '5224E4C657B27D51134071686754A48DDA21D3D0787F92B35DB72EC148C961A6' # CL 583916 with optional XInput workaround
)
$gameHash = (Get-FileHash -LiteralPath $gameExe).Hash
if ($gameHash -notin $supported) {
    throw 'This game executable is a different build. Installation stopped to avoid applying incompatible hooks.'
}
$sourceDll = Join-Path $PSScriptRoot 'build\XInput1_4.dll'
$sourceIni = Join-Path $PSScriptRoot 'ShrekControllerGlyphs.ini'
if (-not (Test-Path -LiteralPath $sourceDll)) { throw 'Build the mod with build.cmd first.' }
foreach ($target in @($dllPath,$iniPath,$manifestPath)) {
    if (Test-Path -LiteralPath $target) { throw "Already exists: $target. Uninstall the previous mod first; no files overwritten." }
}
$manifest = [ordered]@{
    Mod = 'ShrekControllerGlyphs'
    Version = '1.0.5'
    GameSHA256 = $gameHash
    DllSHA256 = (Get-FileHash -LiteralPath $sourceDll).Hash
    IniSHA256 = (Get-FileHash -LiteralPath $sourceIni).Hash
}
$created = @()
try {
    Copy-Item -LiteralPath $sourceDll -Destination $dllPath
    $created += $dllPath
    Copy-Item -LiteralPath $sourceIni -Destination $iniPath
    $created += $iniPath
    $manifest | ConvertTo-Json | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    $created += $manifestPath
    if ((Get-FileHash -LiteralPath $dllPath).Hash -ne $manifest.DllSHA256) { throw 'Installed DLL verification failed.' }
} catch {
    foreach ($createdPath in $created) { Remove-Item -LiteralPath $createdPath -ErrorAction SilentlyContinue }
    throw
}
Write-Output "Installed Shrek Controller Glyphs 1.0.5 to $gameRoot. Enable Steam Input, then launch normally through Steam."
