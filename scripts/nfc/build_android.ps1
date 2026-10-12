<# Builds the P1 verifier with existing Android SDK tools; installs nothing. #>
[CmdletBinding()]
param(
    [string] $Sdk = $(if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { Join-Path $env:LOCALAPPDATA 'Android/Sdk' }),
    [string] $BuildTools = '35.0.0',
    [string] $Platform = 'android-35',
    [string] $Jdk = $env:JAVA_HOME
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out = Join-Path $repo 'build/p1-nfc-helper'
$tools = Join-Path $Sdk "build-tools/$BuildTools"
$android = Join-Path $Sdk "platforms/$Platform/android.jar"
if (-not $Jdk) {
    $ErrorActionPreference = 'Continue'
    $properties = & java -XshowSettings:properties -version 2>&1
    $ErrorActionPreference = 'Stop'
    foreach ($line in $properties) {
        if ("$line" -match '^\s*java\.home\s*=\s*(.+)$') { $Jdk = $Matches[1].Trim() }
    }
}
if (-not $Jdk) { throw 'Set -Jdk to an existing JDK home.' }
foreach ($path in @($android,(Join-Path $tools 'aapt2.exe'),(Join-Path $tools 'd8.bat'),(Join-Path $tools 'apksigner.bat'))) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Required existing SDK tool missing: $path" }
}
function Run {
    param([string] $Exe, [string[]] $Arguments)
    $resolved = (Get-Command $Exe -CommandType Application -ErrorAction Stop).Source
    $ErrorActionPreference = 'Continue'
    & $resolved @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Exe failed ($LASTEXITCODE)" }
}
foreach ($directory in @($out,(Join-Path $out classes),(Join-Path $out dex))) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}
$sources = @(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'android/src/dev/badge/nfc') -Filter '*.java' | ForEach-Object FullName)
Run (Join-Path $Jdk 'bin/javac.exe') (@('--release','8','-Xlint:-options','-classpath',$android,'-d',(Join-Path $out classes)) + $sources)
Run (Join-Path $Jdk 'bin/jar.exe') @('--create','--file',(Join-Path $out classes.jar),'-C',(Join-Path $out classes),'dev/badge/nfc')
Run (Join-Path $tools d8.bat) @('--min-api','24','--lib',$android,'--output',(Join-Path $out dex),(Join-Path $out classes.jar))
Run (Join-Path $tools aapt2.exe) @('compile','--dir',(Join-Path $PSScriptRoot android/res),'-o',(Join-Path $out resources.zip))
Run (Join-Path $tools aapt2.exe) @('link','-I',$android,'--manifest',(Join-Path $PSScriptRoot android/AndroidManifest.xml),'-o',(Join-Path $out unsigned.apk),(Join-Path $out resources.zip))
Run (Join-Path $Jdk 'bin/jar.exe') @('--update','--file',(Join-Path $out unsigned.apk),'-C',(Join-Path $out dex),'classes.dex')
Run (Join-Path $tools zipalign.exe) @('-f','4',(Join-Path $out unsigned.apk),(Join-Path $out aligned.apk))
$key = Join-Path $out development.keystore
if (-not (Test-Path -LiteralPath $key)) {
    # Public development password; unrelated to NTAG credentials. Local artifact.
    Run (Join-Path $Jdk 'bin/keytool.exe') @('-genkeypair','-keystore',$key,'-storepass','android','-keypass','android',
        '-alias','badge-dev','-keyalg','RSA','-keysize','2048','-validity','3650','-dname','CN=Badge Development')
}
$apk = Join-Path $out badge-nfc-helper.apk
Run (Join-Path $tools apksigner.bat) @('sign','--ks',$key,'--ks-pass','pass:android','--key-pass','pass:android','--out',$apk,(Join-Path $out aligned.apk))
Run (Join-Path $tools apksigner.bat) @('verify','--verbose',$apk)
Write-Host "Built $apk (no phone installation performed)."
