#Requires -Version 7.0
param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Release', [switch]$SkipTests, [string]$BuildDirectory = 'build')
$ErrorActionPreference = 'Stop'
# Some launchers supply both Path and PATH. MSBuild's .NET Framework host rejects
# that duplicate; rebuild the child environment with case-insensitive keys.
function Invoke-BuildTool([string]$Tool, [string[]]$ToolArgs) {
    $info = [System.Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Tool
    $info.UseShellExecute = $false
    foreach ($argument in $ToolArgs) { $info.ArgumentList.Add($argument) }
    $clean = [System.Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()) { $clean[$entry.Key] = $entry.Value }
    $info.Environment.Clear()
    foreach ($entry in $clean.GetEnumerator()) { $info.Environment[$entry.Key] = $entry.Value }
    $process = [System.Diagnostics.Process]::Start($info)
    $process.WaitForExit()
    if ($process.ExitCode) { throw "Failed to run $Tool (code $($process.ExitCode))." }
}
$projectRoot = Split-Path -Parent $PSScriptRoot
$null = & "$PSScriptRoot/get-version.ps1"
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmake = $cmakeCommand.Source
} else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio 2022 with Desktop development with C++ and CMake.' }
    $vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $cmake = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
if (!(Test-Path -LiteralPath $cmake)) { throw 'CMake was not found. Add the CMake component in Visual Studio Installer.' }
$buildDir = Join-Path $projectRoot $BuildDirectory
Invoke-BuildTool $cmake @('-S', $projectRoot, '-B', $buildDir, '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DBUILD_TESTING=ON')
Invoke-BuildTool $cmake @('--build', $buildDir, '--config', $Configuration, '--parallel')
if (!$SkipTests) {
    $ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
    Invoke-BuildTool $ctest @('--test-dir', $buildDir, '-C', $Configuration, '--output-on-failure', '--no-tests=error')
}
Write-Output ('Application: ' + (Join-Path $buildDir "$Configuration\MacroPulse.exe"))
