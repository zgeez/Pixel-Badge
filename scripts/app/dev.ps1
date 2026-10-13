<#
.SYNOPSIS
Build or run the Android-first Flutter app with the four local contact links.
#>
[CmdletBinding()]
param(
    [ValidateSet('build', 'run', 'test', 'check')]
    [string]$Command = 'build',
    [string]$Device
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$defines = Join-Path $root 'build/p2-app-links.json'
function Invoke-AppCommand {
    param([string]$Exe, [string[]]$Arguments)
    $path = (Get-Command $Exe -CommandType Application -ErrorAction Stop |
        Select-Object -First 1).Source
    $ErrorActionPreference = 'Continue'
    & $path @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Exe failed with exit code $LASTEXITCODE" }
}
if ($Command -in @('build', 'run')) {
    Invoke-AppCommand python @((Join-Path $PSScriptRoot 'configure_links.py'),
        '--env-file', (Join-Path $root '.env'), '--out', $defines)
}
Push-Location (Join-Path $root 'app')
try {
    switch ($Command) {
        'build' { Invoke-AppCommand flutter @('build', 'apk', '--debug', "--dart-define-from-file=$defines") }
        'run' {
            $arguments = @('run', "--dart-define-from-file=$defines")
            if ($Device) { $arguments += @('-d', $Device) }
            Invoke-AppCommand flutter $arguments
        }
        'test' { Invoke-AppCommand flutter @('test') }
        'check' {
            Invoke-AppCommand dart @('format', '--output=none', '--set-exit-if-changed', 'lib', 'test')
            Invoke-AppCommand flutter @('analyze')
        }
    }
} finally { Pop-Location }
