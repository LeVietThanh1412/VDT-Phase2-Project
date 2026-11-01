param(
    [string]$EngineRoot = 'F:/Epic Games/UE_5.6',
    [string]$TerrainRoot = 'E:/UnrealTerrainSet/QuangNinh',
    [switch]$ValidateOnly
)
$ErrorActionPreference = 'Stop'
$projectFile = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../VDTPhase2Project.uproject')).Path
$editorFile = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
foreach ($required in @($editorFile, (Join-Path $TerrainRoot 'Terrain/layer.json'), (Join-Path $TerrainRoot 'Imagery/tilemapresource.xml'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required demo file missing: $required" }
}
if ($ValidateOnly) { Write-Output 'Demo engine, project, terrain and imagery paths are valid.'; return }
$running = Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" | Where-Object { $_.CommandLine -like '*VDTPhase2Project.uproject*' }
if ($running) { Write-Output 'This project is already open. Use its MainMapLevel window.'; return }
Start-Process -FilePath $editorFile -ArgumentList @(('"' + $projectFile + '"'), '/Game/Levels/MainMapLevel', '-NoSplash', '-DisablePlugins=NNERuntimeORT', '-DDC=InstalledNoZenLocalFallback')
