#Requires -Version 7.0
param([string]$Tag = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$version = (Get-Content -LiteralPath (Join-Path $projectRoot 'VERSION') -Raw).Trim()
if ($version -cnotmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') {
    throw 'VERSION must contain a stable X.Y.Z version (for example: 1.5.0).'
}
foreach ($part in $version.Split('.')) {
    if ([long]$part -gt 65535) { throw 'Each Windows version component must be less than 65536.' }
}
if ($Tag -and $Tag -cne "v$version") {
    throw "The tag '$Tag' does not match VERSION (v$version)."
}
Write-Output $version
