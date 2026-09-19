#Requires -Version 7.0
param([string]$Tag = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$version = (Get-Content -LiteralPath (Join-Path $projectRoot 'VERSION') -Raw).Trim()
if ($version -cnotmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') {
    throw 'VERSION doit contenir une version stable X.Y.Z (exemple : 1.4.0).'
}
foreach ($part in $version.Split('.')) {
    if ([long]$part -gt 65535) { throw 'Chaque composant de version Windows doit être inférieur à 65536.' }
}
if ($Tag -and $Tag -cne "v$version") {
    throw "Le tag '$Tag' ne correspond pas à VERSION (v$version)."
}
Write-Output $version
