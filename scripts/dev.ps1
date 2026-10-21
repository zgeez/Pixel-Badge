<#
.SYNOPSIS
    Windows developer commands for smart-led-badge (equivalent to the Makefile).

.DESCRIPTION
    build   Build Prototype (nRF5340 DK) or RevA (E73 nRF52840).
    test    Run the Twister host tests in the CI toolchain container (Docker).
    format  Format application C/C++ sources in place with clang-format.
    check   Check formatting, lint YAML, and check repository hygiene.
    hardware  Run KiCad ERC and the pin/pad check, export schematic PDF/BOM (requires KiCad 10.0.6).
    clean   Remove the build/ directory.
    recover Rev A only: erase the whole nRF52840 (ERASEALL) and keep its debug
            port open. Needed once for a new module, whose AP-Protect blocks
            SWD. Erases settings, bond and artwork; requires -EraseAll.
    flash   Rev A only: program -BuildDir's merged.hex over SWD, erasing only the
            ranges it touches (UICR, settings, bond and artwork are kept), verify
            by readback and reset.
    rtt     Rev A only: log RTT channel 0 to -RttLog (default build/rtt/<time>.log).

    recover, flash and rtt use a J-Link probe (the nRF5340 DK's debug out, see
    docs/bringup.md). They refuse to act unless the probe identifies an
    nRF52840, so the DK's own nRF5340 is never erased or programmed by mistake;
    recover also accepts a target that AP-Protect keeps from being identified.
    recover and flash pass --family nrf52 to nrfutil, so the write itself fails
    if the target changed after the check. -SwdSpeedKhz applies to every probe
    access. Name the probe with -SerialNumber when more than one J-Link is
    connected.

    The nRF Connect SDK at -NcsRoot\-NcsVersion and the toolchain it requires
    are always loaded, overriding any SDK environment already active. The
    command fails if the SDK version differs from -NcsVersion or its required
    toolchain is not installed.

.EXAMPLE
    .\scripts\dev.ps1 build

.EXAMPLE
    .\scripts\dev.ps1 build -Pristine

.EXAMPLE
    .\scripts\dev.ps1 build -Board smart_led_badge/nrf5340/cpuapp -BuildDir build/badge -ExtraConfFile firmware/nrf5340_prototype/configs/rev-a-peripherals.conf

.EXAMPLE
    .\scripts\dev.ps1 build -App firmware/nrf5340_prototype/bench/matrix_timing -BuildDir build/matrix_timing

.EXAMPLE
    .\scripts\dev.ps1 recover -Target RevA -EraseAll

.EXAMPLE
    .\scripts\dev.ps1 flash -Target RevA -BuildDir build/reva-diagnostics

.EXAMPLE
    .\scripts\dev.ps1 rtt -Target RevA
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('build', 'test', 'format', 'check', 'hardware', 'clean', 'recover', 'flash', 'rtt')]
    [string] $Command = 'build',

    [ValidateSet('Prototype', 'RevA')]
    [string] $Target = 'Prototype',
    [string] $Board,
    [string] $BuildDir,
    # Application directory relative to the repository root.
    [string] $App,
    # Optional Kconfig fragment added to the application image.
    [string[]] $ExtraConfFile,
    # Optional sysbuild-only fragment (for example the P2 BLE controller).
    [string] $SysbuildConfFile,
    # Selects the local .env build default; stored settings still take precedence.
    [ValidateSet('Portfolio', 'LinkedIn', 'GitHub', 'Resume', 'Representative')]
    [string] $QrLink = 'Portfolio',
    [string] $NcsVersion = 'v3.4.1',
    [string] $NcsRoot = $(if ($env:NCS_ROOT) { $env:NCS_ROOT } else { 'C:\ncs' }),
    [switch] $Pristine,
    # recover/flash/rtt: serial number of the J-Link probe (the DK's debugger).
    [string] $SerialNumber,
    # recover: confirms that the whole chip, including settings, bond and
    # artwork, is erased.
    [switch] $EraseAll,
    # rtt: output file.
    [string] $RttLog,
    # recover/flash/rtt: SWD clock in kHz (the flat cable to J501 is long).
    [int] $SwdSpeedKhz = 1000
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $App) { $App = if ($Target -eq 'RevA') { 'firmware/rev_a_nrf52840' } else { 'firmware/nrf5340_prototype' } }
if (-not $Board) { $Board = if ($Target -eq 'RevA') { 'pixel_badge_reva/nrf52840' } else { 'nrf5340dk/nrf5340/cpuapp' } }
if (-not $BuildDir) { $BuildDir = if ($Target -eq 'RevA') { 'build/rev_a_nrf52840' } else { 'build/nrf5340dk' } }

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
    @($files | Where-Object { $_ -and (Test-Path -LiteralPath (Join-Path $RepoRoot $_) -PathType Leaf) } | ForEach-Object { Join-Path $RepoRoot $_ })
}

function Invoke-Build {
    Initialize-NcsEnvironment
    $pristineArg = if ($Pristine) { 'always' } else { 'auto' }
    # Each application owns its chip-specific board root, also visible to MCUboot.
    $boardRoot = (Join-Path $RepoRoot $App).Replace('\', '/')
    $cmakeArgs = @("-DBOARD_ROOT=$boardRoot")
    if ($SysbuildConfFile) {
        $sbConf = (Resolve-Path -LiteralPath $SysbuildConfFile).Path.Replace('\', '/')
        $cmakeArgs += "-DSB_EXTRA_CONF_FILE=$sbConf"
    }
    if ($App -in @('firmware/nrf5340_prototype', 'firmware/rev_a_nrf52840')) {
        $cmakeArgs += "-DBADGE_QR_LINK=$($QrLink.ToUpperInvariant())"
    }
    if ($ExtraConfFile) {
        $conf = (@($ExtraConfFile | ForEach-Object { (Resolve-Path -LiteralPath $_).Path.Replace('\', '/') }) -join ';')
        # Application fragments must not reach MCUboot through sysbuild.
        $confKey = if ($App -in @('firmware/nrf5340_prototype', 'firmware/rev_a_nrf52840')) {
            (Split-Path -Leaf $App) + '_EXTRA_CONF_FILE'
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
        Invoke-Native west (@('build', '--sysbuild', "--pristine=$pristineArg", '-b', $Board,
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

function Get-NrfutilLines {
    # nrfutil from the pinned toolchain (Initialize-NcsEnvironment puts it on
    # PATH and sets NRFUTIL_HOME). Returns the JSON Lines output as text.
    param([string[]] $Arguments)
    $exe = (Get-Command nrfutil -CommandType Application -ErrorAction Stop |
        Select-Object -First 1).Source
    $ErrorActionPreference = 'Continue'
    $out = & $exe @Arguments 2>&1 | ForEach-Object { "$_" }
    [pscustomobject]@{ Code = $LASTEXITCODE; Text = ($out -join "`n") }
}

function Get-JsonValue {
    # A property of a parsed JSON object, or $null when it is missing (dev.ps1
    # runs in strict mode, where reading a missing property throws).
    param($Object, [string] $Name)
    if ($null -eq $Object -or $Object -isnot [Management.Automation.PSCustomObject]) { return $null }
    $property = $Object.PSObject.Properties[$Name]
    if ($property) { $property.Value }
}

function Get-JLinkSerials {
    # Serial numbers of J-Link probes in nrfutil's JSON Lines device list.
    param([string] $Text)
    $found = @()
    foreach ($line in $Text -split "`n") {
        if (-not $line.Trim().StartsWith('{')) { continue }
        try { $obj = $line | ConvertFrom-Json } catch { continue }
        if ((Get-JsonValue $obj 'type') -ne 'task_end') { continue }
        $devices = Get-JsonValue (Get-JsonValue (Get-JsonValue $obj 'data') 'data') 'devices'
        foreach ($device in @($devices)) {
            if ((Get-JsonValue (Get-JsonValue $device 'traits') 'jlink') -eq $true) {
                $found += [string](Get-JsonValue $device 'serialNumber')
            }
        }
    }
    $found | Sort-Object -Unique
}

function Resolve-JLinkProbe {
    $list = Get-NrfutilLines @('device', 'list', '--traits', 'jlink', '--json')
    $serials = @(Get-JLinkSerials $list.Text)
    if ($SerialNumber) {
        if ($serials -notcontains $SerialNumber) {
            throw "J-Link probe $SerialNumber not found (connected: $($serials -join ', '))."
        }
        return $SerialNumber
    }
    if ($serials.Count -eq 0) {
        throw 'No J-Link probe found. Connect the nRF5340 DK (debug out to J501, see docs/bringup.md).'
    }
    if ($serials.Count -gt 1) {
        throw "Several J-Link probes found ($($serials -join ', ')); choose one with -SerialNumber."
    }
    return $serials[0]
}

function Get-SwdArguments {
    @('--swd-clock-frequency', [string]$SwdSpeedKhz)
}

function Get-DeviceInfoNodes {
    # Every deviceInfo.jlink object anywhere under a parsed JSON node.
    param($Node)
    if ($Node -is [array]) {
        foreach ($item in $Node) { Get-DeviceInfoNodes $item }
        return
    }
    if ($Node -isnot [Management.Automation.PSCustomObject]) { return }
    foreach ($property in $Node.PSObject.Properties) {
        $jlink = if ($property.Name -eq 'deviceInfo') { Get-JsonValue $property.Value 'jlink' }
        if ($jlink) { $jlink } else { Get-DeviceInfoNodes $property.Value }
    }
}

function Get-TargetIdentity {
    # The chip behind the probe, from a successful device-info:
    # deviceInfo.jlink.deviceFamily/deviceVersion (for example NRF52_FAMILY,
    # NRF52840_xxAA_REV2; nrfutil-device 2.20.0 puts it at task_end
    # data.data.deviceInfo). Family and Version are empty unless nrfutil exited
    # 0, its task ended in success and it reported exactly one identity.
    param([string] $Probe)
    $info = Get-NrfutilLines (@('device', 'device-info', '--serial-number', $Probe, '--json') + (Get-SwdArguments))
    $identity = [pscustomobject]@{ Family = ''; Version = ''; Text = $info.Text }
    if ($info.Code -ne 0) { return $identity }
    $nodes = @()
    $succeeded = $false
    foreach ($line in $info.Text -split "`n") {
        if (-not $line.Trim().StartsWith('{')) { continue }
        try { $obj = $line | ConvertFrom-Json } catch { continue }
        if ((Get-JsonValue $obj 'type') -ne 'task_end') { continue }
        $data = Get-JsonValue $obj 'data'
        if ((Get-JsonValue $data 'result') -ne 'success') { return $identity }
        $succeeded = $true
        $nodes += @(Get-DeviceInfoNodes $data)
    }
    if (-not $succeeded -or $nodes.Count -ne 1) { return $identity }
    $identity.Family = [string](Get-JsonValue $nodes[0] 'deviceFamily')
    $identity.Version = [string](Get-JsonValue $nodes[0] 'deviceVersion')
    $identity
}

function Assert-RevATarget {
    # With nothing powered on the debug-out connector the DK's debugger targets
    # the DK's own nRF5340. Only an identified nRF52840 (the badge's E73) is
    # accepted. -AllowProtected (recover only) also accepts a target that
    # cannot be identified, since AP-Protect can hide the part; the recover
    # command itself then refuses anything but the nRF52 family.
    param([string] $Probe, [switch] $AllowProtected)
    $id = Get-TargetIdentity $Probe
    if ($id.Family -eq 'NRF52_FAMILY' -and $id.Version -match '^NRF52840_') {
        Write-Host "Probe $Probe sees $($id.Version)."
        return
    }
    if ($id.Family -match '^NRF53' -or $id.Version -match '^NRF53' -or
        (-not $id.Family -and $id.Text -match '(?i)nrf53')) {
        throw "Probe $Probe sees an nRF5340 (the DK itself), not the badge. Check the debug-out setup in docs/bringup.md: badge powered, VTG on the cable, DK SB47 open."
    }
    if (($id.Family -and $id.Family -ne 'NRF52_FAMILY') -or $id.Version -match '^NRF\d') {
        throw "Probe $Probe sees $($id.Family) $($id.Version), not the badge's nRF52840."
    }
    Write-Host $id.Text
    if (-not $AllowProtected) {
        throw "Probe $Probe could not identify an nRF52840 (output above). A new module with AP-Protect needs 'recover' first; otherwise check the cable and badge power."
    }
    Write-Warning "Probe $Probe could not identify the target (AP-Protect on a new module hides it). Recovering only if nrfutil detects the nRF52 family."
}

function Assert-RevAImage {
    # A Rev A sysbuild image with no UICR records: programming it must neither
    # erase UICR (AP-Protect stays open, pin reset and NFC-pin settings stay)
    # nor flash a DK image to the badge.
    param([string] $Hex)
    if (-not (Test-Path -LiteralPath $Hex)) {
        throw "$Hex not found; build first (dev.ps1 build -Target RevA -BuildDir ...)."
    }
    $config = Join-Path (Split-Path -Parent $Hex) 'rev_a_nrf52840\zephyr\.config'
    if (-not (Test-Path -LiteralPath $config) -or
        -not (Select-String -LiteralPath $config -Pattern '^CONFIG_BOARD="pixel_badge_reva"' -Quiet)) {
        throw "$Hex is not a Rev A (pixel_badge_reva) build."
    }
    if (Select-String -LiteralPath $Hex -Pattern '^:020000041000' -Quiet) {
        throw "$Hex contains UICR data; programming it would erase UICR and re-lock the debug port."
    }
}

function Invoke-Recover {
    if ($Target -ne 'RevA') { throw 'recover is for the Rev A badge only (-Target RevA).' }
    if (-not $EraseAll) {
        throw 'recover erases the whole nRF52840, including settings, the BLE bond and artwork. Add -EraseAll to confirm.'
    }
    Initialize-NcsEnvironment
    $probe = Resolve-JLinkProbe
    Assert-RevATarget $probe -AllowProtected
    Write-Host "Recovering the nRF52840 behind J-Link $probe (ERASEALL, debug port kept open)..."
    # --family repeats the target check inside the destructive command itself,
    # so a target that changed since identification (badge power lost, the
    # probe back on the DK's nRF5340) is refused.
    Invoke-Native nrfutil (@('device', 'recover', '--serial-number', $probe, '--family', 'nrf52') + (Get-SwdArguments))
}

function Invoke-Flash {
    if ($Target -ne 'RevA') { throw 'flash is for the Rev A badge only (-Target RevA).' }
    Initialize-NcsEnvironment
    $hex = Join-Path (Join-Path $RepoRoot $BuildDir) 'merged.hex'
    Assert-RevAImage $hex
    $probe = Resolve-JLinkProbe
    Assert-RevATarget $probe
    Write-Host "Programming $hex through J-Link $probe..."
    # --family: as in recover, the target is checked again by the write itself.
    Invoke-Native nrfutil (@('device', 'program', '--serial-number', $probe, '--family', 'nrf52', '--firmware', $hex,
        '--options', 'chip_erase_mode=ERASE_RANGES_TOUCHED_BY_FIRMWARE,verify=VERIFY_READ,reset=RESET_SYSTEM') +
        (Get-SwdArguments))
}

function Get-JLinkTool {
    param([string] $Name)
    $onPath = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue
    if ($onPath) { return ($onPath | Select-Object -First 1).Source }
    $installs = @(Get-ChildItem -Directory -Path "$env:ProgramFiles\SEGGER" -Filter 'JLink*' -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending)
    foreach ($dir in $installs) {
        $candidate = Join-Path $dir.FullName $Name
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw "$Name not found; install the SEGGER J-Link software."
}

function Invoke-Rtt {
    if ($Target -ne 'RevA') { throw 'rtt is for the Rev A badge only (-Target RevA).' }
    Initialize-NcsEnvironment
    $probe = Resolve-JLinkProbe
    Assert-RevATarget $probe
    if (-not $RttLog) {
        $RttLog = Join-Path $RepoRoot ("build/rtt/{0:yyyyMMdd-HHmmss}.log" -f (Get-Date))
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $RttLog) | Out-Null
    $logger = Get-JLinkTool 'JLinkRTTLogger.exe'
    Write-Host "Logging RTT channel 0 from J-Link $probe to $RttLog (Ctrl+C to stop)..."
    & $logger -Device NRF52840_XXAA -If SWD -Speed $SwdSpeedKhz -USB $probe -RTTChannel 0 $RttLog
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
        'recover' { Invoke-Recover }
        'flash'  { Invoke-Flash }
        'rtt'    { Invoke-Rtt }
        'clean'  {
            if (Test-Path (Join-Path $RepoRoot 'build')) {
                Remove-Item -Recurse -Force (Join-Path $RepoRoot 'build')
            }
        }
    }
} finally {
    Pop-Location
}
