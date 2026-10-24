param(
    [Parameter(Mandatory=$true)][string]$DatasetRoot,
    [string]$CompatibilityPath = 'E:\UnrealTerrainSet\QuangNinh'
)
$ErrorActionPreference = 'Stop'
$sourcePath = (Resolve-Path -LiteralPath $DatasetRoot).Path.TrimEnd('\')
$linkPath = [IO.Path]::GetFullPath($CompatibilityPath).TrimEnd('\')
if (-not (Test-Path -LiteralPath (Join-Path $sourcePath 'Terrain/layer.json') -PathType Leaf)) { throw 'Missing Terrain/layer.json' }
if (-not (Test-Path -LiteralPath (Join-Path $sourcePath 'Imagery/tilemapresource.xml') -PathType Leaf)) { throw 'Missing Imagery/tilemapresource.xml' }
if ($sourcePath.Equals($linkPath,[StringComparison]::OrdinalIgnoreCase)) { Write-Output 'Dataset already occupies the expected path.'; return }
if ($sourcePath.StartsWith($linkPath+'\',[StringComparison]::OrdinalIgnoreCase) -or $linkPath.StartsWith($sourcePath+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Source and compatibility path must not contain each other.' }
if (Test-Path -LiteralPath $linkPath) {
    $existing = Get-Item -LiteralPath $linkPath -Force
    if ($existing.LinkType -eq 'Junction' -and [IO.Path]::GetFullPath($existing.Target).TrimEnd('\').Equals($sourcePath,[StringComparison]::OrdinalIgnoreCase)) { Write-Output 'Existing junction already targets this dataset.'; return }
    throw 'Compatibility path already exists. It was preserved; no data was overwritten.'
}
$parentPath = Split-Path -Parent $linkPath
if (-not (Test-Path -LiteralPath $parentPath)) { New-Item -ItemType Directory -Path $parentPath | Out-Null }
New-Item -ItemType Junction -Path $linkPath -Target $sourcePath | Out-Null
Write-Output "Mapped $linkPath -> $sourcePath"
Write-Output ('Terrain URL: ' + ([Uri](Join-Path $linkPath 'Terrain/layer.json')).AbsoluteUri)
Write-Output ('Imagery URL: ' + ([Uri](Join-Path $linkPath 'Imagery/tilemapresource.xml')).AbsoluteUri)
