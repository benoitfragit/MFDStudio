[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$RemainingArguments
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-RequiredOptionValue {
    param(
        [string[]]$Arguments,
        [int]$CurrentIndex,
        [string]$OptionName
    )

    if ($CurrentIndex + 1 -ge $Arguments.Count) {
        throw "Missing value after $OptionName."
    }

    return $Arguments[$CurrentIndex + 1]
}

function Set-SingleOptionValue {
    param(
        [string]$OptionName,
        [string]$CurrentValue,
        [string]$NewValue
    )

    if (-not [string]::IsNullOrWhiteSpace($CurrentValue)) {
        throw "$OptionName was provided more than once."
    }

    if ([string]::IsNullOrWhiteSpace($NewValue)) {
        throw "$OptionName requires a non-empty value."
    }

    return $NewValue
}

function Get-DeliveryOptions {
    param(
        [string[]]$Arguments,
        [object[]]$AvailableProfiles
    )

    $packageVersion = $null
    $selectedPreset = $null
    $showHelp = $false

    for ($index = 0; $index -lt $Arguments.Count; ++$index) {
        $argument = $Arguments[$index]

        if ($argument -eq "--help" -or $argument -eq "-h" -or $argument -eq "-?") {
            $showHelp = $true
            continue
        }

        if ($argument -eq "--version" -or $argument -eq "-Version" -or $argument -eq "-v") {
            $packageVersion = Set-SingleOptionValue `
                -OptionName $argument `
                -CurrentValue $packageVersion `
                -NewValue (Get-RequiredOptionValue -Arguments $Arguments -CurrentIndex $index -OptionName $argument)
            ++$index
            continue
        }

        if ($argument.StartsWith("--version=")) {
            $packageVersion = Set-SingleOptionValue `
                -OptionName "--version" `
                -CurrentValue $packageVersion `
                -NewValue $argument.Substring("--version=".Length)
            continue
        }

        if ($argument.StartsWith("-Version=")) {
            $packageVersion = Set-SingleOptionValue `
                -OptionName "-Version" `
                -CurrentValue $packageVersion `
                -NewValue $argument.Substring("-Version=".Length)
            continue
        }

        if ($argument -eq "--preset" -or $argument -eq "-p") {
            $selectedPreset = Set-SingleOptionValue `
                -OptionName $argument `
                -CurrentValue $selectedPreset `
                -NewValue (Get-RequiredOptionValue -Arguments $Arguments -CurrentIndex $index -OptionName $argument)
            ++$index
            continue
        }

        if ($argument.StartsWith("--preset=")) {
            $selectedPreset = Set-SingleOptionValue `
                -OptionName "--preset" `
                -CurrentValue $selectedPreset `
                -NewValue $argument.Substring("--preset=".Length)
            continue
        }

        if ($argument.StartsWith("-p=")) {
            $selectedPreset = Set-SingleOptionValue `
                -OptionName "-p" `
                -CurrentValue $selectedPreset `
                -NewValue $argument.Substring("-p=".Length)
            continue
        }

        throw "Unknown argument '$argument'. Use --help to see supported options."
    }

    if (-not $showHelp -and [string]::IsNullOrWhiteSpace($packageVersion)) {
        throw "BuildDeliveries.ps1 requires --version <value>."
    }

    if (-not [string]::IsNullOrWhiteSpace($selectedPreset)) {
        $availablePresetNames = New-Object System.Collections.Generic.List[string]
        foreach ($profile in $AvailableProfiles) {
            if (-not $availablePresetNames.Contains($profile.ConfigurePreset)) {
                [void]$availablePresetNames.Add($profile.ConfigurePreset)
            }
            foreach ($build in $profile.Builds) {
                if (-not $availablePresetNames.Contains($build.PresetName)) {
                    [void]$availablePresetNames.Add($build.PresetName)
                }
            }
        }
        if ($selectedPreset -notin $availablePresetNames) {
            throw "Unknown preset '$selectedPreset'. Use --help to see the available presets."
        }
    }

    return [pscustomobject]@{
        PackageVersion = $packageVersion
        SelectedPreset = $selectedPreset
        ShowHelp = $showHelp
    }
}

function Show-Usage {
    param([object[]]$AvailableProfiles)

    Write-Host "Usage:"
    Write-Host "  Scripts\BuildDeliveries.bat --version <value> [--preset <preset-name>]"
    Write-Host "  Scripts\BuildDeliveries.ps1 --version <value> [--preset <preset-name>]"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  --version, -v <value>   Package version written into manifests and ConfigVersion.cmake files."
    Write-Host "  --preset, -p <name>    Restrict the delivery run to one configure preset or one build preset."
    Write-Host "  --help, -h             Show this help and exit."
    Write-Host ""
    Write-Host "Available configure presets:"
    foreach ($profile in $AvailableProfiles) {
        $buildPresetNames = @($profile.Builds | ForEach-Object { $_.PresetName }) -join ", "
        Write-Host ("  {0} [{1}] -> {2}" -f `
            $profile.ConfigurePreset, `
            $profile.Platform, `
            $buildPresetNames)
    }
    Write-Host ""
    Write-Host "Available build presets:"
    foreach ($profile in $AvailableProfiles) {
        foreach ($build in $profile.Builds) {
            Write-Host ("  {0} [{1}/{2}]" -f `
                $build.PresetName, `
                $profile.Platform, `
                $build.Configuration)
        }
    }
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  Scripts\BuildDeliveries.bat --version 1.8.5"
    Write-Host "  Scripts\BuildDeliveries.bat --version 1.8.5 --preset vs2022-win32-no-tests"
    Write-Host "  Scripts\BuildDeliveries.bat --version 1.8.5 --preset debug-win32-no-tests"
    Write-Host "  Scripts\BuildDeliveries.bat --version 1.8.5 --preset release-x64-no-tests"
    Write-Host ""
    Write-Host "Default behavior:"
    Write-Host "  Without --preset, the script processes every preset listed above."
}

function New-DeliveryProfileSelection {
    param(
        [object]$Profile,
        [object[]]$Builds
    )

    return [pscustomobject]@{
        ConfigurePreset = $Profile.ConfigurePreset
        BuildDirectory = $Profile.BuildDirectory
        Platform = $Profile.Platform
        Builds = @($Builds)
    }
}

function Select-DeliveryProfiles {
    param(
        [object[]]$AvailableProfiles,
        [string]$SelectedPreset
    )

    if ([string]::IsNullOrWhiteSpace($SelectedPreset)) {
        return @(
            $AvailableProfiles | ForEach-Object {
                New-DeliveryProfileSelection -Profile $_ -Builds $_.Builds
            }
        )
    }

    $configureMatch = $AvailableProfiles | Where-Object { $_.ConfigurePreset -eq $SelectedPreset } | Select-Object -First 1
    if ($null -ne $configureMatch) {
        return @(New-DeliveryProfileSelection -Profile $configureMatch -Builds $configureMatch.Builds)
    }

    foreach ($profile in $AvailableProfiles) {
        $buildMatch = $profile.Builds | Where-Object { $_.PresetName -eq $SelectedPreset } | Select-Object -First 1
        if ($null -ne $buildMatch) {
            return @(New-DeliveryProfileSelection -Profile $profile -Builds @($buildMatch))
        }
    }

    throw "Unknown preset '$SelectedPreset'. Use --help to see the available presets."
}

function Invoke-Step {
    param(
        [string]$Message,
        [scriptblock]$Action
    )

    Write-Host "==> $Message"
    & $Action
}

function Get-ConfigurePresetMap {
    param([string]$PresetsFilePath)

    if (-not (Test-Path $PresetsFilePath)) {
        return @{}
    }

    $presetsDocument = Get-Content -LiteralPath $PresetsFilePath -Raw | ConvertFrom-Json
    $presetMap = @{}
    foreach ($preset in $presetsDocument.configurePresets) {
        $presetMap[$preset.name] = $preset
    }

    return $presetMap
}

function Resolve-ConfigurePresetToolset {
    param(
        [string]$ConfigurePreset,
        [hashtable]$ConfigurePresetMap
    )

    if ([string]::IsNullOrWhiteSpace($ConfigurePreset) -or $null -eq $ConfigurePresetMap) {
        return $null
    }

    $visited = New-Object System.Collections.Generic.HashSet[string]

    function Resolve-FromPreset {
        param(
            [string]$PresetName,
            [hashtable]$PresetMap,
            [System.Collections.Generic.HashSet[string]]$Seen
        )

        if ([string]::IsNullOrWhiteSpace($PresetName)) {
            return $null
        }

        if (-not $Seen.Add($PresetName)) {
            return $null
        }

        $preset = $PresetMap[$PresetName]
        if ($null -eq $preset) {
            return $null
        }

        if ($preset.PSObject.Properties.Name -contains "toolset" -and
            -not [string]::IsNullOrWhiteSpace([string]$preset.toolset)) {
            return [string]$preset.toolset
        }

        if (-not ($preset.PSObject.Properties.Name -contains "inherits")) {
            return $null
        }

        $parents = @()
        if ($preset.inherits -is [System.Array]) {
            $parents = @($preset.inherits)
        }
        elseif (-not [string]::IsNullOrWhiteSpace([string]$preset.inherits)) {
            $parents = @([string]$preset.inherits)
        }

        foreach ($parent in $parents) {
            $resolved = Resolve-FromPreset -PresetName $parent -PresetMap $PresetMap -Seen $Seen
            if (-not [string]::IsNullOrWhiteSpace($resolved)) {
                return $resolved
            }
        }

        return $null
    }

    return Resolve-FromPreset -PresetName $ConfigurePreset -PresetMap $ConfigurePresetMap -Seen $visited
}

function Get-DeliveryToolset {
    param(
        [string]$ConfigurePreset,
        [hashtable]$ConfigurePresetMap
    )

    $presetToolset = Resolve-ConfigurePresetToolset `
        -ConfigurePreset $ConfigurePreset `
        -ConfigurePresetMap $ConfigurePresetMap
    if (-not [string]::IsNullOrWhiteSpace($presetToolset)) {
        return $presetToolset
    }

    return "native"
}

function Invoke-CMakeBuildPreset {
    param(
        [string]$PresetName,
        [string]$BuildDirectory
    )

    & cmake --build --preset $PresetName
    if ($LASTEXITCODE -eq 0) {
        return
    }

    $lockedExecutables = Get-ChildItem `
        -Path (Join-Path $BuildDirectory "tests") `
        -Filter "mfd_api_tests.exe" `
        -Recurse `
        -ErrorAction SilentlyContinue

    if (-not $lockedExecutables) {
        throw "cmake --build --preset $PresetName failed."
    }

    Write-Warning "Retrying $PresetName after removing locked mfd_api_tests.exe."
    foreach ($lockedExecutable in $lockedExecutables) {
        Remove-Item -LiteralPath $lockedExecutable.FullName -Force -ErrorAction SilentlyContinue
    }

    & cmake --build --preset $PresetName
    if ($LASTEXITCODE -ne 0) {
        throw "cmake --build --preset $PresetName failed after retry."
    }
}

function Install-Component {
    param(
        [string]$BuildDirectory,
        [string]$Configuration,
        [string]$Component,
        [string]$Prefix
    )

    & cmake --install $BuildDirectory --config $Configuration --component $Component --prefix $Prefix
    if ($LASTEXITCODE -ne 0) {
        throw "cmake --install $BuildDirectory --config $Configuration --component $Component failed."
    }
}

function Write-PackageManifest {
    param(
        [string]$OutputFile,
        [string]$PackageName,
        [string]$PackageKind,
        [string]$ProjectVersion,
        [string]$Toolset,
        [string[]]$Targets
    )

    $targetList = ($Targets | Where-Object { $_ -and $_.Trim().Length -gt 0 }) -join "|"
    & cmake `
        "-DOUTPUT_FILE=$OutputFile" `
        "-DPACKAGE_NAME=$PackageName" `
        "-DPACKAGE_KIND=$PackageKind" `
        "-DPROJECT_VERSION=$ProjectVersion" `
        "-DTOOLSET=$Toolset" `
        "-DTARGETS=$targetList" `
        -P "cmake/WritePackageManifest.cmake"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to write manifest for $PackageName."
    }
}

function Assert-PathExists {
    param([string]$PathToCheck)

    if (-not (Test-Path $PathToCheck)) {
        throw "Expected path was not generated: $PathToCheck"
    }
}
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $repoRoot
$configurePresetMap = Get-ConfigurePresetMap -PresetsFilePath (Join-Path $repoRoot "CMakePresets.json")

$deliveryRoot = Join-Path $repoRoot "_Deliveries"
$packagesRoot = Join-Path $deliveryRoot "packages_windows"
$runtimePackagesRoot = Join-Path $deliveryRoot "packages_bin_windows"

$deliveryProfiles = @(
    [pscustomobject]@{
        ConfigurePreset = "vs2022-win32-no-tests"
        BuildDirectory = Join-Path $repoRoot "build/vs2022-win32-no-tests"
        Platform = "win32"
        Builds = @(
            [pscustomobject]@{
                PresetName = "debug-win32-no-tests"
                Configuration = "Debug"
            }
            [pscustomobject]@{
                PresetName = "release-win32-no-tests"
                Configuration = "Release"
            }
        )
    }
    [pscustomobject]@{
        ConfigurePreset = "vs2022-x64-no-tests"
        BuildDirectory = Join-Path $repoRoot "build/vs2022-x64-no-tests"
        Platform = "x64"
        Builds = @(
            [pscustomobject]@{
                PresetName = "debug-x64-no-tests"
                Configuration = "Debug"
            }
            [pscustomobject]@{
                PresetName = "release-x64-no-tests"
                Configuration = "Release"
            }
        )
    }
)

$deliveryOptions = Get-DeliveryOptions -Arguments $RemainingArguments -AvailableProfiles $deliveryProfiles
if ($deliveryOptions.ShowHelp) {
    Show-Usage -AvailableProfiles $deliveryProfiles
    return
}

$packageVersion = $deliveryOptions.PackageVersion
$selectedProfiles = Select-DeliveryProfiles `
    -AvailableProfiles $deliveryProfiles `
    -SelectedPreset $deliveryOptions.SelectedPreset

$developmentPackages = @(
    [pscustomobject]@{
        Name = "MFDStudioClientApi"
        Component = "mfd_client_api_development"
        Targets = @("mfd_client_api", "mfd_api")
    }
    [pscustomobject]@{
        Name = "MFDStudioWindowLauncherPlugin"
        Component = "mfd_window_plugin_api_development"
        Targets = @("mfd_window_plugin_api")
    }
)

$runtimePackages = @(
    [pscustomobject]@{
        Name = "MFDStudioClientApi.Install"
        Component = "mfd_client_api_runtime"
        Targets = @("mfd_client_api", "mfd_api")
    }
    [pscustomobject]@{
        Name = "MFDStudioWindowLauncher.Install"
        Component = "mfd_window_launcher_runtime"
        Targets = @("mfd_window", "mfd_window_plugin_api")
    }
    [pscustomobject]@{
        Name = "MFDStudioEditor.Install"
        Component = "mfd_editor_runtime"
        Targets = @("mfd_editor")
    }
)

Invoke-Step -Message "Removing previous _Deliveries layout" -Action {
    Remove-Item -LiteralPath $deliveryRoot -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Path $packagesRoot | Out-Null
    New-Item -ItemType Directory -Path $runtimePackagesRoot | Out-Null
}

foreach ($profile in $selectedProfiles) {
    Invoke-Step -Message "Configuring $($profile.ConfigurePreset) for package version $packageVersion" -Action {
        & cmake --preset $profile.ConfigurePreset "-DMFD_PACKAGE_VERSION=$packageVersion" "-DMFD_API_BUILD_SHARED=ON"
        if ($LASTEXITCODE -ne 0) {
            throw "cmake --preset $($profile.ConfigurePreset) failed."
        }
    }
}

foreach ($profile in $selectedProfiles) {
    foreach ($build in $profile.Builds) {
        Invoke-Step -Message "Building $($build.PresetName)" -Action {
            Invoke-CMakeBuildPreset -PresetName $build.PresetName -BuildDirectory $profile.BuildDirectory
        }
    }
}

foreach ($profile in $selectedProfiles) {
    $toolset = Get-DeliveryToolset `
        -ConfigurePreset $profile.ConfigurePreset `
        -ConfigurePresetMap $configurePresetMap

    foreach ($developmentPackage in $developmentPackages) {
        $prefix = Join-Path $packagesRoot "$($developmentPackage.Name)/build/native"
        foreach ($build in $profile.Builds) {
            Invoke-Step -Message "Installing $($developmentPackage.Name) $($profile.Platform) $($build.Configuration)" -Action {
                Install-Component -BuildDirectory $profile.BuildDirectory -Configuration $build.Configuration -Component $developmentPackage.Component -Prefix $prefix
            }
        }
    }

    foreach ($runtimePackage in $runtimePackages) {
        foreach ($build in $profile.Builds) {
            $prefix = Join-Path $runtimePackagesRoot "$($runtimePackage.Name)/_Exec/$toolset/$($profile.Platform)/$($build.Configuration)"
            Invoke-Step -Message "Installing $($runtimePackage.Name) $($profile.Platform) $($build.Configuration)" -Action {
                Install-Component -BuildDirectory $profile.BuildDirectory -Configuration $build.Configuration -Component $runtimePackage.Component -Prefix $prefix
            }
        }
    }
}

$manifestToolset = "native"
foreach ($profile in $selectedProfiles) {
    $manifestToolset = Get-DeliveryToolset `
        -ConfigurePreset $profile.ConfigurePreset `
        -ConfigurePresetMap $configurePresetMap
    break
}

foreach ($developmentPackage in $developmentPackages) {
    $packageRoot = Join-Path $packagesRoot "$($developmentPackage.Name)/build/native"
    Write-PackageManifest `
        -OutputFile (Join-Path $packageRoot "manifest.json") `
        -PackageName $developmentPackage.Name `
        -PackageKind "development" `
        -ProjectVersion $packageVersion `
        -Toolset $manifestToolset `
        -Targets $developmentPackage.Targets
}

foreach ($runtimePackage in $runtimePackages) {
    $packageRoot = Join-Path $runtimePackagesRoot $runtimePackage.Name
    Write-PackageManifest `
        -OutputFile (Join-Path $packageRoot "manifest.json") `
        -PackageName $runtimePackage.Name `
        -PackageKind "runtime" `
        -ProjectVersion $packageVersion `
        -Toolset $manifestToolset `
        -Targets $runtimePackage.Targets
}

Invoke-Step -Message "Verifying delivery layout" -Action {
    Assert-PathExists (Join-Path $packagesRoot "MFDStudioClientApi/build/native/cmake/MFDStudioClientApiConfig.cmake")
    Assert-PathExists (Join-Path $packagesRoot "MFDStudioWindowLauncherPlugin/build/native/cmake/MFDStudioWindowLauncherPluginConfig.cmake")

    foreach ($profile in $selectedProfiles) {
        $toolset = Get-DeliveryToolset `
            -ConfigurePreset $profile.ConfigurePreset `
            -ConfigurePresetMap $configurePresetMap

        foreach ($build in $profile.Builds) {
            Assert-PathExists (Join-Path $runtimePackagesRoot "MFDStudioClientApi.Install/_Exec/$toolset/$($profile.Platform)/$($build.Configuration)/mfd_client_api.dll")
            Assert-PathExists (Join-Path $runtimePackagesRoot "MFDStudioWindowLauncher.Install/_Exec/$toolset/$($profile.Platform)/$($build.Configuration)/mfd_window.exe")
            Assert-PathExists (Join-Path $runtimePackagesRoot "MFDStudioWindowLauncher.Install/_Exec/$toolset/$($profile.Platform)/$($build.Configuration)/mfd_window_plugin_api.dll")
            Assert-PathExists (Join-Path $runtimePackagesRoot "MFDStudioEditor.Install/_Exec/$toolset/$($profile.Platform)/$($build.Configuration)/mfd_editor.exe")
        }
    }
}

Write-Host ""
Write-Host "Delivery packages generated under $deliveryRoot"
