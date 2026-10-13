<#
.SYNOPSIS
    Windows developer commands for smart-led-badge (equivalent to the Makefile).

.DESCRIPTION
    build   Build the firmware for the nRF5340 DK application core.
    test    Run the Twister host tests in the CI toolchain container (Docker).
    format  Format application C/C++ sources in place with clang-format.
    check   Check formatting, lint YAML, and check repository hygiene.
    hardware  Run KiCad ERC and the pin/pad check, export schematic PDF/BOM (requires KiCad 10.0.6).
    clean   Remove the build/ directory.

    The nRF Connect SDK at -NcsRoot\-NcsVersion and the toolchain it requires
    are always loaded, overriding any SDK environment already active. The
    command fails if the SDK version differs from -NcsVersion or its required
    toolchain is not installed.

.EXAMPLE
    .\scripts\dev.ps1 build

.EXAMPLE
    .\scripts\dev.ps1 build -Pristine

.EXAMPLE
    .\scripts\dev.ps1 build -Board smart_led_badge/nrf5340/cpuapp -BuildDir build/badge -ExtraConfFile firmware/configs/rev-a-peripherals.conf

.EXAMPLE
    .\scripts\dev.ps1 build -App firmware/bench/matrix_timing -BuildDir build/matrix_timing
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('build', 'test', 'format', 'check', 'hardware', 'clean')]
    [string] $Command = 'build',

    [string] $Board = 'nrf5340dk/nrf5340/cpuapp',
    [string] $BuildDir = 'build/nrf5340dk',
    # Application directory relative to the repository root.
    [string] $App = 'firmware',
    # Optional Kconfig fragment added to the application image.
    [string] $ExtraConfFile,
    # Optional sysbuild-only fragment (for example the P2 BLE controller).
    [string] $SysbuildConfFile,
    # Selects the local .env build default; stored settings still take precedence.
    [ValidateSet('Portfolio', 'LinkedIn', 'GitHub', 'Resume', 'Representative')]
    [string] $QrLink = 'Portfolio',
    [string] $NcsVersion = 'v3.4.1',
    [string] $NcsRoot = $(if ($env:NCS_ROOT) { $env:NCS_ROOT } else { 'C:\ncs' }),
    [switch] $Pristine
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot

# Keep in sync with .github/workflows/ci.yml.
$ToolchainImage = 'ghcr.io/nrfconnect/sdk-nrf-toolchain:v3.4.1@sha256:45b97cad97a9967c52d77d1d1a0f7dd8fe027edd17c05c3eda2eeadc23729418'
$NcsVolume = 'smart-led-badge-ncs-v3.4.1'

function Invoke-Native {
    param([string] $Exe, [string[]] $Arguments)
    # Resolve first: a missing executable must fail here, because a lookup
    # failure does not update $LASTEXITCODE.
    $path = (Get-Command $Exe -CommandType Application -ErrorAction Stop |
        Select-Object -First 1).Source
    # Windows PowerShell 5.1 turns redirected native stderr into error records;
    # rely on the exit code instead.
    $ErrorActionPreference = 'Continue'
    & $path @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Exe exited with code $LASTEXITCODE"
    }
}

function Get-NcsVersion {
    param([string] $NrfDir)
    $versionFile = Join-Path $NrfDir 'VERSION'
    if (-not (Test-Path $versionFile)) {
        throw "Not an nRF Connect SDK tree (missing $versionFile)."
    }
    $v = @{}
    Get-Content $versionFile | ForEach-Object {
        if ($_ -match '^\s*(\w+)\s*=\s*(\S*)') { $v[$Matches[1]] = $Matches[2] }
    }
    "v$($v['VERSION_MAJOR']).$($v['VERSION_MINOR']).$($v['PATCHLEVEL'])"
}

function Get-RequiredToolchainId {
    # Windows equivalent of nrf/scripts/print_toolchain_checksum.sh: the first
    # 10 hex digits of SHA-256 over the SDK's pinned Python requirements and
    # tool versions, with CR line endings stripped.
    param([string] $NrfDir)
    $scripts = Join-Path $NrfDir 'scripts'
    $bytes = [IO.File]::ReadAllBytes((Join-Path $scripts 'requirements-fixed.txt')) +
        [IO.File]::ReadAllBytes((Join-Path $scripts 'tools-versions-win10.yml'))
    $lf = New-Object System.Collections.Generic.List[byte]
    for ($i = 0; $i -lt $bytes.Length; $i++) {
        $atLineEnd = ($i + 1 -eq $bytes.Length) -or ($bytes[$i + 1] -eq 10)
        if ($bytes[$i] -eq 13 -and $atLineEnd) { continue }
        $lf.Add($bytes[$i])
    }
    $hash = [Security.Cryptography.SHA256]::Create().ComputeHash($lf.ToArray())
    (-join ($hash | ForEach-Object { $_.ToString('x2') })).Substring(0, 10)
}

function Initialize-NcsEnvironment {
    # Always load the SDK and toolchain from -NcsRoot/-NcsVersion, even if
    # another SDK environment is active, so local builds match CI.
    $sdkDir = Join-Path $NcsRoot $NcsVersion
    $nrfDir = Join-Path $sdkDir 'nrf'
    $zephyrDir = Join-Path $sdkDir 'zephyr'

    $found = Get-NcsVersion $nrfDir
    if ($found -ne $NcsVersion) {
        throw "$sdkDir contains nRF Connect SDK $found, expected $NcsVersion."
    }
    if (-not (Test-Path $zephyrDir)) {
        throw "Zephyr not found at $zephyrDir."
    }

    $toolchainId = Get-RequiredToolchainId $nrfDir
    $toolchainDir = Join-Path $NcsRoot "toolchains\$toolchainId"
    $envFile = Join-Path $toolchainDir 'environment.json'
    if (-not (Test-Path $envFile)) {
        throw "nRF Connect SDK $NcsVersion requires toolchain $toolchainId, which is not installed under $NcsRoot\toolchains."
    }

    $envSpec = Get-Content -Raw $envFile | ConvertFrom-Json
    foreach ($var in $envSpec.env_vars) {
        if ($var.type -eq 'string') {
            $value = $var.value
        } else {
            $value = ($var.values | ForEach-Object { Join-Path $toolchainDir $_ }) -join ';'
        }

        $existing = [Environment]::GetEnvironmentVariable($var.key, 'Process')
        if ($var.PSObject.Properties['existing_value_treatment'] -and
            $var.existing_value_treatment -eq 'prepend_to' -and $existing) {
            $value = "$value;$existing"
        }
        [Environment]::SetEnvironmentVariable($var.key, $value, 'Process')
    }
    $env:ZEPHYR_BASE = $zephyrDir

    $west = (Get-Command west -CommandType Application -ErrorAction Stop |
        Select-Object -First 1).Source
    if (-not $west.StartsWith($toolchainDir, [StringComparison]::OrdinalIgnoreCase)) {
        throw "west resolves to $west, not to toolchain $toolchainDir."
    }

    Write-Host "Using nRF Connect SDK $NcsVersion at $sdkDir with toolchain $toolchainId"
}

function Get-SourceFiles {
    $files = & git -C $RepoRoot ls-files --cached --others --exclude-standard -- `
        '*.c' '*.h' '*.cpp' '*.hpp'
    if ($LASTEXITCODE -ne 0) { throw 'git ls-files failed' }
    @($files | Where-Object { $_ } | ForEach-Object { Join-Path $RepoRoot $_ })
}

function Invoke-Build {
    Initialize-NcsEnvironment
    $pristineArg = if ($Pristine) { 'always' } else { 'auto' }
    # BOARD_ROOT makes the Rev A board (firmware/boards) visible to sysbuild too.
    $boardRoot = (Join-Path $RepoRoot 'firmware').Replace('\', '/')
    $cmakeArgs = @("-DBOARD_ROOT=$boardRoot")
    if ($SysbuildConfFile) {
        $sbConf = (Resolve-Path -LiteralPath $SysbuildConfFile).Path.Replace('\', '/')
        $cmakeArgs += "-DSB_EXTRA_CONF_FILE=$sbConf"
    }
    if ($App -eq 'firmware') {
        $cmakeArgs += "-DBADGE_QR_LINK=$($QrLink.ToUpperInvariant())"
    }
    if ($ExtraConfFile) {
        $conf = (Resolve-Path -LiteralPath $ExtraConfFile).Path.Replace('\', '/')
        # Application fragments must not reach MCUboot through sysbuild.
        $confKey = if ($App -eq 'firmware' -and $Board -like '*/cpuapp') {
            'firmware_EXTRA_CONF_FILE'
        } else {
            'EXTRA_CONF_FILE'
        }
        $cmakeArgs += "-D${confKey}=$conf"
    }
    $previousExtraConf = $env:EXTRA_CONF_FILE
    $previousSbExtraConf = $env:SB_EXTRA_CONF_FILE
    try {
        Remove-Item Env:EXTRA_CONF_FILE -ErrorAction SilentlyContinue
        Remove-Item Env:SB_EXTRA_CONF_FILE -ErrorAction SilentlyContinue
        Invoke-Native west (@('build', "--pristine=$pristineArg", '-b', $Board,
            '-d', (Join-Path $RepoRoot $BuildDir), (Join-Path $RepoRoot $App),
            '--') + $cmakeArgs)
    } finally {
        $env:EXTRA_CONF_FILE = $previousExtraConf
        $env:SB_EXTRA_CONF_FILE = $previousSbExtraConf
    }
}

function Invoke-Test {
    # Zephyr's native_sim test target runs on Linux hosts only, so tests run in
    # the same toolchain container as CI. The SDK is cached in a Docker volume.
    if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
        throw 'Docker is required to run host tests on Windows (native_sim is Linux-only).'
    }
    Invoke-Native docker @('run', '--rm',
        '-v', "${RepoRoot}:/work/smart-led-badge",
        '-v', "${NcsVolume}:/work/ncs",
        '-w', '/work/smart-led-badge',
        $ToolchainImage,
        'bash scripts/ci/install-host-deps.sh && bash scripts/ci/setup-ncs-workspace.sh /work/ncs && ZEPHYR_BASE=/work/ncs/zephyr bash scripts/ci/run-tests.sh')
}

function Invoke-Format {
    Initialize-NcsEnvironment
    $files = Get-SourceFiles
    if ($files.Count -gt 0) {
        Invoke-Native clang-format (@('-i', '--style=file') + $files)
    }
}

function Invoke-Check {
    Initialize-NcsEnvironment
    $files = Get-SourceFiles
    if ($files.Count -gt 0) {
        Invoke-Native clang-format (@('--dry-run', '--Werror', '--style=file') + $files)
    }
    Invoke-Native yamllint @('--strict', '.')
    Invoke-Native python @((Join-Path $RepoRoot 'scripts/ci/check_hygiene.py'))
    Write-Host 'All checks passed.'
}

Push-Location $RepoRoot
try {
    switch ($Command) {
        'build'  { Invoke-Build }
        'test'   { Invoke-Test }
        'format' { Invoke-Format }
        'check'  { Invoke-Check }
        'hardware' {
            Invoke-Native python @((Join-Path $RepoRoot 'scripts/ci/check_hardware.py'))
        }
        'clean'  {
            if (Test-Path (Join-Path $RepoRoot 'build')) {
                Remove-Item -Recurse -Force (Join-Path $RepoRoot 'build')
            }
        }
    }
} finally {
    Pop-Location
}
