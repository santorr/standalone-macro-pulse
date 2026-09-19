#Requires -Version 7.0
param([string]$BuildDirectory = 'build', [string]$OutputDirectory = 'dist', [string]$Tag = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$version = & "$PSScriptRoot/get-version.ps1" -Tag $Tag
$buildDir = Join-Path $projectRoot $BuildDirectory
$outputDir = Join-Path $projectRoot $OutputDirectory
$executable = Join-Path $buildDir 'Release/MacroPulse.exe'
if (!(Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw 'Build and test the Release configuration with scripts/build.ps1 before packaging.'
}
$metadata = (Get-Item -LiteralPath $executable).VersionInfo
if ($metadata.FileVersion -ne $version -or $metadata.ProductVersion -ne $version) {
    throw "The executable version does not match VERSION ($version). Rebuild the project."
}
$baseName = "MacroPulse-$version-windows-x64"
$stage = Join-Path $buildDir ("package-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage -Force | Out-Null
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
Copy-Item -LiteralPath $executable -Destination (Join-Path $stage 'MacroPulse.exe')
Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md'), (Join-Path $projectRoot 'CHANGELOG.md') -Destination $stage
$exePath = Join-Path $outputDir "$baseName.exe"
$zipPath = Join-Path $outputDir "$baseName.zip"
Copy-Item -LiteralPath $executable -Destination $exePath -Force
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zipPath -Force
$checksums = foreach ($file in @($exePath, $zipPath)) {
    $hash = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $([IO.Path]::GetFileName($file))"
}
$checksums | Set-Content -LiteralPath (Join-Path $outputDir 'SHA256SUMS.txt') -Encoding utf8NoBOM
Write-Output "Packages $version created in $outputDir"
