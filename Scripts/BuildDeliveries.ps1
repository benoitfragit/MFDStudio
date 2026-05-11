[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Phase {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    Write-Host ""
    Write-Host "=== $Name ===" -ForegroundColor Cyan
}

function Write-Step {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    Write-Host "[delivery] $Message"
}

function New-Directory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    [System.IO.Directory]::CreateDirectory($Path) | Out-Null
}

function Write-Utf8File {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Content
    )

    $parentDirectory = Split-Path -Path $Path -Parent
    if (-not [string]::IsNullOrWhiteSpace($parentDirectory)) {
        New-Directory -Path $parentDirectory
    }

    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $utf8NoBom)
}

function Copy-FileStrict {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Missing source file: $Source"
    }

    $parentDirectory = Split-Path -Path $Destination -Parent
    if (-not [string]::IsNullOrWhiteSpace($parentDirectory)) {
        New-Directory -Path $parentDirectory
    }

    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Copy-DirectoryStrict {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        throw "Missing source directory: $Source"
    }

    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }

    $parentDirectory = Split-Path -Path $Destination -Parent
    if (-not [string]::IsNullOrWhiteSpace($parentDirectory)) {
        New-Directory -Path $parentDirectory
    }

    Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
}

function Invoke-ExternalCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Command
    )

    Write-Step ($Command -join " ")
    & $Command[0] $Command[1..($Command.Length - 1)]
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $($Command -join ' ')"
    }
}

function Get-ProjectVersion {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RootCMakeListsPath
    )

    $content = Get-Content -LiteralPath $RootCMakeListsPath -Raw
    $match = [regex]::Match($content, "project\s*\(\s*MFD\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if (-not $match.Success) {
        throw "Unable to resolve project version from $RootCMakeListsPath"
    }

    return $match.Groups[1].Value
}

function Get-IncludeRelativePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourcePath
    )

    $normalizedPath = $SourcePath.Replace("/", "\")
    $marker = "\include\"
    $index = $normalizedPath.IndexOf($marker, [System.StringComparison]::OrdinalIgnoreCase)
    if ($index -lt 0) {
        throw "Unable to resolve include-relative path from $SourcePath"
    }

    return $normalizedPath.Substring($index + $marker.Length)
}

function Copy-PublicHeaders {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepositoryRoot,
        [Parameter(Mandatory = $true)]
        [string]$DestinationIncludeRoot,
        [Parameter(Mandatory = $true)]
        [string[]]$RelativeSourceFiles
    )

    foreach ($relativeSourceFile in $RelativeSourceFiles) {
        $sourcePath = Join-Path $RepositoryRoot $relativeSourceFile
        $includeRelativePath = Get-IncludeRelativePath -SourcePath $sourcePath
        $destinationPath = Join-Path $DestinationIncludeRoot $includeRelativePath
        Copy-FileStrict -Source $sourcePath -Destination $destinationPath
    }
}

function Write-PackageManifest {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$PackageName,
        [Parameter(Mandatory = $true)]
        [string]$PackageKind,
        [Parameter(Mandatory = $true)]
        [string]$ProjectVersion,
        [Parameter(Mandatory = $true)]
        [string]$Toolset,
        [Parameter(Mandatory = $true)]
        [string[]]$Targets
    )

    $manifest = [ordered]@{
        packageName    = $PackageName
        packageKind    = $PackageKind
        projectVersion = $ProjectVersion
        toolset        = $Toolset
        targets        = $Targets
    }

    Write-Utf8File -Path $Path -Content (($manifest | ConvertTo-Json -Depth 4) + [Environment]::NewLine)
}

function Write-PackageConfigFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$TemplatePath,
        [Parameter(Mandatory = $true)]
        [string]$DestinationPath,
        [Parameter(Mandatory = $true)]
        [string]$Toolset,
        [Parameter(Mandatory = $true)]
        [string]$ImportedTargetName
    )

    $content = Get-Content -LiteralPath $TemplatePath -Raw
    $content = $content.Replace("@PACKAGE_INIT@", "")
    $content = $content.Replace("@MFD_PACKAGE_TOOLSET@", $Toolset)
    $content = $content.Replace("@MFD_PACKAGE_IMPORTED_TARGET_NAME@", $ImportedTargetName)
    $content = $content.TrimStart("`r", "`n")

    Write-Utf8File -Path $DestinationPath -Content ($content + [Environment]::NewLine)
}

function Write-PackageConfigVersionFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$ProjectVersion
    )

    $majorVersion = $ProjectVersion.Split(".")[0]
    $content = @"
set(PACKAGE_VERSION "$ProjectVersion")

if(NOT DEFINED PACKAGE_FIND_VERSION OR PACKAGE_FIND_VERSION STREQUAL "")
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    set(PACKAGE_VERSION_EXACT TRUE)
    return()
endif()

if(PACKAGE_FIND_VERSION_RANGE)
    if(NOT DEFINED PACKAGE_FIND_VERSION_MIN_MAJOR OR NOT PACKAGE_FIND_VERSION_MIN_MAJOR STREQUAL "$majorVersion")
        set(PACKAGE_VERSION_COMPATIBLE FALSE)
    elseif((PACKAGE_FIND_VERSION_RANGE_MAX STREQUAL "INCLUDE" AND PACKAGE_VERSION VERSION_LESS_EQUAL PACKAGE_FIND_VERSION_MAX) OR
           (PACKAGE_FIND_VERSION_RANGE_MAX STREQUAL "EXCLUDE" AND PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION_MAX))
        set(PACKAGE_VERSION_COMPATIBLE TRUE)
    else()
        set(PACKAGE_VERSION_COMPATIBLE FALSE)
    endif()
elseif(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
elseif(PACKAGE_FIND_VERSION_MAJOR STREQUAL "$majorVersion")
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
else()
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
endif()

if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)
    set(PACKAGE_VERSION_EXACT TRUE)
endif()
"@

    Write-Utf8File -Path $Path -Content $content
}

function Resolve-BuildArtifactPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepositoryRoot,
        [Parameter(Mandatory = $true)]
        [string]$BuildDirectoryName,
        [Parameter(Mandatory = $true)]
        [string]$ModuleDirectory,
        [Parameter(Mandatory = $true)]
        [string]$Configuration,
        [Parameter(Mandatory = $true)]
        [string]$FileName
    )

    return (Join-Path $RepositoryRoot ("build/{0}/{1}/{2}/{3}" -f $BuildDirectoryName, $ModuleDirectory, $Configuration, $FileName))
}

function Assert-PathExists {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing expected path: $Path"
    }
}

function Assert-PathMissing {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (Test-Path -LiteralPath $Path) {
        throw "Unexpected packaged path: $Path"
    }
}

$scriptDirectory = Split-Path -Path $MyInvocation.MyCommand.Path -Parent
$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $scriptDirectory ".."))
$deliveriesRoot = Join-Path $repositoryRoot "_Deliveries"
$packagesRoot = Join-Path $deliveriesRoot "packages_windows"
$runtimePackagesRoot = Join-Path $deliveriesRoot "packages_bin_windows"
$toolset = "v142"

$configurePresets = @("vs2022-win32", "vs2022-x64")
$buildVariants = @(
    [pscustomobject]@{
        BuildPreset       = "debug-win32"
        BuildDirectory    = "vs2022-win32"
        Platform          = "win32"
        Configuration     = "Debug"
        ConfigurationName = "Debug"
    },
    [pscustomobject]@{
        BuildPreset       = "release-win32"
        BuildDirectory    = "vs2022-win32"
        Platform          = "win32"
        Configuration     = "Release"
        ConfigurationName = "Release"
    },
    [pscustomobject]@{
        BuildPreset       = "debug-x64"
        BuildDirectory    = "vs2022-x64"
        Platform          = "x64"
        Configuration     = "Debug"
        ConfigurationName = "Debug"
    },
    [pscustomobject]@{
        BuildPreset       = "release-x64"
        BuildDirectory    = "vs2022-x64"
        Platform          = "x64"
        Configuration     = "Release"
        ConfigurationName = "Release"
    }
)

$clientApiPublicHeaders = @(
    "mfd_client_api/include/mfd/client/Animation.h",
    "mfd_client_api/include/mfd/client/ClientExport.h",
    "mfd_client_api/include/mfd/client/LatestBatchPublisher.h",
    "mfd_common_api/include/mfd/MfdExport.h",
    "mfd_common_api/include/mfd/core/ArrayView.h",
    "mfd_common_api/include/mfd/control/CommandClient.h",
    "mfd_common_api/include/mfd/control/CommandTransport.h",
    "mfd_common_api/include/mfd/control/CommandTypes.h",
    "mfd_common_api/include/mfd/ipc/ExchangeChannel.h",
    "mfd_common_api/include/mfd/ipc/UdpLimits.h",
    "mfd_common_api/include/mfd/model/PageDefinition.h",
    "mfd_common_api/include/mfd/model/PageName.h",
    "mfd_common_api/include/mfd/model/Reticle.h",
    "mfd_common_api/include/mfd/model/Types.h",
    "mfd_common_api/include/mfd/runtime/GeneratedTransportMap.h"
)

$windowLauncherPublicHeaders = @(
    "mfd_window_plugin_api/include/mfd/window/WindowLauncherPlugin.h",
    "mfd_common_api/include/mfd/core/ArrayView.h"
)

Push-Location $repositoryRoot
try {
    Write-Phase -Name "Generation"
    Write-Step "Repository root: $repositoryRoot"
    if (Test-Path -LiteralPath $deliveriesRoot) {
        Write-Step "Removing previous _Deliveries tree"
        Remove-Item -LiteralPath $deliveriesRoot -Recurse -Force
    }

    foreach ($buildDirectoryName in @("vs2022-win32", "vs2022-x64")) {
        $buildDirectoryPath = Join-Path $repositoryRoot ("build/{0}" -f $buildDirectoryName)
        if (Test-Path -LiteralPath $buildDirectoryPath) {
            Write-Step "Removing previous build tree $buildDirectoryPath"
            Remove-Item -LiteralPath $buildDirectoryPath -Recurse -Force
        }
    }

    foreach ($configurePreset in $configurePresets) {
        Invoke-ExternalCommand -Command @("cmake", "-T", $toolset, "--preset", $configurePreset)
    }

    $projectVersion = Get-ProjectVersion -RootCMakeListsPath (Join-Path $repositoryRoot "CMakeLists.txt")
    Write-Step "Detected project version: $projectVersion"
    Write-Step "Using MSVC toolset: $toolset"

    Write-Phase -Name "Build"
    foreach ($buildVariant in $buildVariants) {
        Invoke-ExternalCommand -Command @("cmake", "--build", "--preset", $buildVariant.BuildPreset)
    }

    Write-Phase -Name "Copy"
    New-Directory -Path $packagesRoot
    New-Directory -Path $runtimePackagesRoot

    $clientPackageRoot = Join-Path $packagesRoot "MFDStudioClientApi/build/native"
    $clientIncludeRoot = Join-Path $clientPackageRoot "include"
    $clientCMakeRoot = Join-Path $clientPackageRoot "cmake"
    $clientToolsRoot = Join-Path $clientPackageRoot "tools"
    $clientManifestPath = Join-Path $clientPackageRoot "share/MFDStudioClientApi/package_manifest.json"
    Copy-PublicHeaders -RepositoryRoot $repositoryRoot -DestinationIncludeRoot $clientIncludeRoot -RelativeSourceFiles $clientApiPublicHeaders
    Copy-FileStrict -Source (Join-Path $repositoryRoot "mfd_client_api/generator/ClientApiGenerator.cmake") -Destination (Join-Path $clientCMakeRoot "ClientApiGenerator.cmake")
    Copy-DirectoryStrict -Source (Join-Path $repositoryRoot "mfd_client_api/generator") -Destination (Join-Path $clientToolsRoot "generator")
    Write-PackageConfigFile -TemplatePath (Join-Path $repositoryRoot "cmake/packages/MFDStudioClientApiConfig.cmake.in") -DestinationPath (Join-Path $clientCMakeRoot "MFDStudioClientApiConfig.cmake") -Toolset $toolset -ImportedTargetName "MFDStudio::ClientApi"
    Write-PackageConfigVersionFile -Path (Join-Path $clientCMakeRoot "MFDStudioClientApiConfigVersion.cmake") -ProjectVersion $projectVersion
    Write-PackageManifest -Path $clientManifestPath -PackageName "MFDStudioClientApi" -PackageKind "development" -ProjectVersion $projectVersion -Toolset $toolset -Targets @("mfd_client_api", "mfd_api")

    foreach ($buildVariant in $buildVariants) {
        $clientLibRoot = Join-Path $clientPackageRoot ("libs/{0}/{1}/{2}" -f $toolset, $buildVariant.Platform, $buildVariant.ConfigurationName)
        Copy-FileStrict -Source (Resolve-BuildArtifactPath -RepositoryRoot $repositoryRoot -BuildDirectoryName $buildVariant.BuildDirectory -ModuleDirectory "mfd_client_api" -Configuration $buildVariant.Configuration -FileName "mfd_client_api.lib") -Destination (Join-Path $clientLibRoot "mfd_client_api.lib")
        Copy-FileStrict -Source (Resolve-BuildArtifactPath -RepositoryRoot $repositoryRoot -BuildDirectoryName $buildVariant.BuildDirectory -ModuleDirectory "mfd_api" -Configuration $buildVariant.Configuration -FileName "mfd_api.lib") -Destination (Join-Path $clientLibRoot "mfd_api.lib")
    }

    $windowLauncherPackageRoot = Join-Path $packagesRoot "MFDStudioWindowLauncherPlugin/build/native"
    $windowLauncherIncludeRoot = Join-Path $windowLauncherPackageRoot "include"
    $windowLauncherCMakeRoot = Join-Path $windowLauncherPackageRoot "cmake"
    $windowLauncherManifestPath = Join-Path $windowLauncherPackageRoot "share/MFDStudioWindowLauncherPlugin/package_manifest.json"
    Copy-PublicHeaders -RepositoryRoot $repositoryRoot -DestinationIncludeRoot $windowLauncherIncludeRoot -RelativeSourceFiles $windowLauncherPublicHeaders
    Write-PackageConfigFile -TemplatePath (Join-Path $repositoryRoot "cmake/packages/MFDStudioWindowLauncherPluginConfig.cmake.in") -DestinationPath (Join-Path $windowLauncherCMakeRoot "MFDStudioWindowLauncherPluginConfig.cmake") -Toolset $toolset -ImportedTargetName "MFDStudio::WindowLauncherPlugin"
    Write-PackageConfigVersionFile -Path (Join-Path $windowLauncherCMakeRoot "MFDStudioWindowLauncherPluginConfigVersion.cmake") -ProjectVersion $projectVersion
    Write-PackageManifest -Path $windowLauncherManifestPath -PackageName "MFDStudioWindowLauncherPlugin" -PackageKind "development" -ProjectVersion $projectVersion -Toolset $toolset -Targets @("mfd_window_plugin_api")

    foreach ($buildVariant in $buildVariants) {
        $windowLibRoot = Join-Path $windowLauncherPackageRoot ("libs/{0}/{1}/{2}" -f $toolset, $buildVariant.Platform, $buildVariant.ConfigurationName)
        Copy-FileStrict -Source (Resolve-BuildArtifactPath -RepositoryRoot $repositoryRoot -BuildDirectoryName $buildVariant.BuildDirectory -ModuleDirectory "mfd_window_plugin_api" -Configuration $buildVariant.Configuration -FileName "mfd_window_plugin_api.lib") -Destination (Join-Path $windowLibRoot "mfd_window_plugin_api.lib")
    }

    $clientInstallRoot = Join-Path $runtimePackagesRoot "MFDStudioClientApi.Install/_Exec"
    Write-PackageManifest -Path (Join-Path $clientInstallRoot "_Conf/package_manifest.json") -PackageName "MFDStudioClientApi.Install" -PackageKind "runtime" -ProjectVersion $projectVersion -Toolset $toolset -Targets @("mfd_client_api", "mfd_api")

    foreach ($buildVariant in $buildVariants) {
        $runtimeRoot = Join-Path $clientInstallRoot ("{0}/{1}/{2}" -f $toolset, $buildVariant.Platform, $buildVariant.ConfigurationName)
        Copy-FileStrict -Source (Resolve-BuildArtifactPath -RepositoryRoot $repositoryRoot -BuildDirectoryName $buildVariant.BuildDirectory -ModuleDirectory "mfd_client_api" -Configuration $buildVariant.Configuration -FileName "mfd_client_api.dll") -Destination (Join-Path $runtimeRoot "mfd_client_api.dll")
        Copy-FileStrict -Source (Resolve-BuildArtifactPath -RepositoryRoot $repositoryRoot -BuildDirectoryName $buildVariant.BuildDirectory -ModuleDirectory "mfd_api" -Configuration $buildVariant.Configuration -FileName "mfd_api.dll") -Destination (Join-Path $runtimeRoot "mfd_api.dll")
    }

    $editorInstallRoot = Join-Path $runtimePackagesRoot "MFDStudioEditor.Install/_Exec"
    Write-PackageManifest -Path (Join-Path $editorInstallRoot "_Conf/package_manifest.json") -PackageName "MFDStudioEditor.Install" -PackageKind "runtime" -ProjectVersion $projectVersion -Toolset $toolset -Targets @("mfd_editor")

    foreach ($buildVariant in $buildVariants) {
        $runtimeRoot = Join-Path $editorInstallRoot ("{0}/{1}/{2}" -f $toolset, $buildVariant.Platform, $buildVariant.ConfigurationName)
        Copy-FileStrict -Source (Resolve-BuildArtifactPath -RepositoryRoot $repositoryRoot -BuildDirectoryName $buildVariant.BuildDirectory -ModuleDirectory "mfd_editor" -Configuration $buildVariant.Configuration -FileName "mfd_editor.exe") -Destination (Join-Path $runtimeRoot "mfd_editor.exe")
        Copy-DirectoryStrict -Source (Join-Path $repositoryRoot "branding") -Destination (Join-Path $runtimeRoot "branding")
    }

    $windowInstallRoot = Join-Path $runtimePackagesRoot "MFDStudioWindowLauncher.Install/_Exec"
    Write-PackageManifest -Path (Join-Path $windowInstallRoot "_Conf/package_manifest.json") -PackageName "MFDStudioWindowLauncher.Install" -PackageKind "runtime" -ProjectVersion $projectVersion -Toolset $toolset -Targets @("mfd_window", "mfd_window_plugin_api")

    foreach ($buildVariant in $buildVariants) {
        $runtimeRoot = Join-Path $windowInstallRoot ("{0}/{1}/{2}" -f $toolset, $buildVariant.Platform, $buildVariant.ConfigurationName)
        Copy-FileStrict -Source (Resolve-BuildArtifactPath -RepositoryRoot $repositoryRoot -BuildDirectoryName $buildVariant.BuildDirectory -ModuleDirectory "mfd_window" -Configuration $buildVariant.Configuration -FileName "mfd_window.exe") -Destination (Join-Path $runtimeRoot "mfd_window.exe")
        Copy-FileStrict -Source (Resolve-BuildArtifactPath -RepositoryRoot $repositoryRoot -BuildDirectoryName $buildVariant.BuildDirectory -ModuleDirectory "mfd_window_plugin_api" -Configuration $buildVariant.Configuration -FileName "mfd_window_plugin_api.dll") -Destination (Join-Path $runtimeRoot "mfd_window_plugin_api.dll")
        Copy-FileStrict -Source (Join-Path $repositoryRoot "Scripts/Start-MfdWindow.bat") -Destination (Join-Path $runtimeRoot "Start-MfdWindow.bat")
        Copy-DirectoryStrict -Source (Join-Path $repositoryRoot "branding") -Destination (Join-Path $runtimeRoot "branding")
    }

    Write-Phase -Name "Verification"
    Assert-PathExists -Path $packagesRoot
    Assert-PathExists -Path $runtimePackagesRoot
    Assert-PathExists -Path (Join-Path $clientCMakeRoot "MFDStudioClientApiConfig.cmake")
    Assert-PathExists -Path (Join-Path $windowLauncherCMakeRoot "MFDStudioWindowLauncherPluginConfig.cmake")
    Assert-PathExists -Path (Join-Path $clientPackageRoot "tools/generator/scripts/generate_ui.py")

    foreach ($buildVariant in $buildVariants) {
        $clientLibRoot = Join-Path $clientPackageRoot ("libs/{0}/{1}/{2}" -f $toolset, $buildVariant.Platform, $buildVariant.ConfigurationName)
        Assert-PathExists -Path (Join-Path $clientLibRoot "mfd_client_api.lib")
        Assert-PathExists -Path (Join-Path $clientLibRoot "mfd_api.lib")
        Assert-PathMissing -Path (Join-Path $clientLibRoot "mfd_model.lib")
        Assert-PathMissing -Path (Join-Path $clientLibRoot "mfd_transport.lib")

        $windowLibRoot = Join-Path $windowLauncherPackageRoot ("libs/{0}/{1}/{2}" -f $toolset, $buildVariant.Platform, $buildVariant.ConfigurationName)
        Assert-PathExists -Path (Join-Path $windowLibRoot "mfd_window_plugin_api.lib")

        $clientRuntimeRoot = Join-Path $clientInstallRoot ("{0}/{1}/{2}" -f $toolset, $buildVariant.Platform, $buildVariant.ConfigurationName)
        Assert-PathExists -Path (Join-Path $clientRuntimeRoot "mfd_client_api.dll")
        Assert-PathExists -Path (Join-Path $clientRuntimeRoot "mfd_api.dll")
        Assert-PathMissing -Path (Join-Path $clientRuntimeRoot "assets")

        $editorRuntimeRoot = Join-Path $editorInstallRoot ("{0}/{1}/{2}" -f $toolset, $buildVariant.Platform, $buildVariant.ConfigurationName)
        Assert-PathExists -Path (Join-Path $editorRuntimeRoot "mfd_editor.exe")
        Assert-PathExists -Path (Join-Path $editorRuntimeRoot "branding/mfdstudio_app_icon.png")
        Assert-PathMissing -Path (Join-Path $editorRuntimeRoot "assets")
        Assert-PathMissing -Path (Join-Path $editorRuntimeRoot "Start-MfdDemo.bat")

        $windowRuntimeRoot = Join-Path $windowInstallRoot ("{0}/{1}/{2}" -f $toolset, $buildVariant.Platform, $buildVariant.ConfigurationName)
        Assert-PathExists -Path (Join-Path $windowRuntimeRoot "mfd_window.exe")
        Assert-PathExists -Path (Join-Path $windowRuntimeRoot "mfd_window_plugin_api.dll")
        Assert-PathExists -Path (Join-Path $windowRuntimeRoot "Start-MfdWindow.bat")
        Assert-PathExists -Path (Join-Path $windowRuntimeRoot "branding/mfdstudio_app_icon.png")
        Assert-PathMissing -Path (Join-Path $windowRuntimeRoot "assets")
        Assert-PathMissing -Path (Join-Path $windowRuntimeRoot "mfd_framebuffer_stdout_plugin.dll")
        Assert-PathMissing -Path (Join-Path $windowRuntimeRoot "Start-MfdDemo.bat")
        Assert-PathMissing -Path (Join-Path $windowRuntimeRoot "Start-MfdCockpit.bat")
        Assert-PathMissing -Path (Join-Path $windowRuntimeRoot "Start-MfdMinimal.bat")
        Assert-PathMissing -Path (Join-Path $windowRuntimeRoot "Start-MfdTutorial.bat")
    }

    Write-Step "Deliveries generated successfully under $deliveriesRoot"
    exit 0
}
catch {
    Write-Error $_
    exit 1
}
finally {
    Pop-Location
}
