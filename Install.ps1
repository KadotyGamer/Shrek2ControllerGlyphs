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
    Write-Output 'Mod removed. Original game files were never modified. The diagnostic log and any edited config were retained.'
    exit
}
$supported = '00B67356582A4F3D900DB855224BA9379F9980DCCCFF3C89C3558CF0EEFB11CB'
if ((Get-FileHash -LiteralPath $gameExe).Hash -ne $supported) {
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
    Version = '1.0.0'
    GameSHA256 = $supported
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
Write-Output "Installed Shrek Controller Glyphs 1.0.0 to $gameRoot. Enable Steam Input, then launch normally through Steam."
