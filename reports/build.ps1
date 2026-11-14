$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $buildDir = Join-Path $PSScriptRoot '_build/compiled'
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
    for ($pass = 1; $pass -le 3; $pass++) {
        & xelatex -interaction=nonstopmode -halt-on-error -file-line-error -jobname=reports_verified "-output-directory=$buildDir" reports.tex *> (Join-Path $buildDir "xelatex-$pass.stdout.log")
        if ($LASTEXITCODE -ne 0) { throw "XeLaTeX pass $pass failed. See _build/xelatex-$pass.stdout.log" }
    }
    Copy-Item -LiteralPath (Join-Path $buildDir 'reports_verified.pdf') -Destination (Join-Path $PSScriptRoot 'reports.pdf') -Force
    Write-Output 'Built reports.pdf successfully.'
} finally { Pop-Location }
