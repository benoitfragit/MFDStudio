[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

function Write-Section([string]$Title)
{
    Write-Host ""
    Write-Host "== $Title ==" -ForegroundColor Cyan
}

function Write-KeyValue([string]$Name, [string]$Value)
{
    Write-Host ("{0,-22}: {1}" -f $Name, $Value)
}

function Write-StringList([string]$Name, [string[]]$Values)
{
    if ($null -eq $Values -or $Values.Count -eq 0)
    {
        Write-KeyValue -Name $Name -Value "<none>"
        return
    }

    Write-KeyValue -Name $Name -Value ($Values -join ", ")
}

function Write-CommandLine([string]$Label, [string]$CommandLine)
{
    Write-KeyValue -Name $Label -Value $CommandLine
}

function Get-RepoRoot
{
    $scriptDirectory = Split-Path -Parent $PSCommandPath
    return (Resolve-Path (Join-Path $scriptDirectory "..")).Path
}

function Normalize-RepoPath([string]$Path)
{
    return $Path.Replace('\', '/').TrimStart('.').TrimStart('/')
}

function Convert-ToRepoRelativePath([string]$Path, [string]$RepoRoot)
{
    if ([string]::IsNullOrWhiteSpace($Path))
    {
        return ""
    }

    $normalizedPath = $Path.Replace('\', '/')
    $normalizedRoot = $RepoRoot.Replace('\', '/').TrimEnd('/')
    if ($normalizedPath.StartsWith($normalizedRoot + "/", [System.StringComparison]::OrdinalIgnoreCase))
    {
        return $normalizedPath.Substring($normalizedRoot.Length + 1)
    }

    return Normalize-RepoPath $normalizedPath
}

function Quote-ForCmd([string]$Value)
{
    if ($Value.Contains('"'))
    {
        throw "Unsupported double quote in command argument: $Value"
    }

    if ($Value -match '[\s\&\|\(\)\<\>\^]')
    {
        return '"' + $Value + '"'
    }

    return $Value
}

function Resolve-OptionalPath([string]$CandidatePath, [string]$RepoRoot)
{
    if ([string]::IsNullOrWhiteSpace($CandidatePath))
    {
        return ""
    }

    $resolvedPath = Resolve-Path -LiteralPath $CandidatePath -ErrorAction SilentlyContinue
    if ($null -ne $resolvedPath)
    {
        return $resolvedPath.Path
    }

    if ([System.IO.Path]::IsPathRooted($CandidatePath))
    {
        return [System.IO.Path]::GetFullPath($CandidatePath)
    }

    return (Join-Path $RepoRoot $CandidatePath)
}

function Ensure-Directory([string]$Path)
{
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

function Reset-Directory([string]$Path)
{
    if (Test-Path $Path)
    {
        Remove-Item -Recurse -Force -LiteralPath $Path
    }

    Ensure-Directory -Path $Path
}

function Write-JsonFile([string]$Path, $Value)
{
    $json = $Value | ConvertTo-Json -Depth 20
    Set-Content -LiteralPath $Path -Value $json -Encoding UTF8
}

function Find-VsInstallationPath
{
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere))
    {
        throw "Unable to locate vswhere.exe under Visual Studio Installer."
    }

    $installationPath = & $vswhere -latest -products * -property installationPath
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installationPath))
    {
        throw "Unable to locate a Visual Studio installation."
    }

    return $installationPath.Trim()
}

function Find-VcVarsAllPath
{
    $installationPath = Find-VsInstallationPath
    $vcVarsAll = Join-Path $installationPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcVarsAll))
    {
        throw "Unable to locate vcvarsall.bat at $vcVarsAll"
    }

    return $vcVarsAll
}

function Find-ClangTidyPath
{
    $command = Get-Command clang-tidy -ErrorAction SilentlyContinue
    if ($null -ne $command)
    {
        return $command.Source
    }

    $installationPath = Find-VsInstallationPath
    $candidates = @(
        (Join-Path $installationPath "VC\Tools\Llvm\x64\bin\clang-tidy.exe"),
        (Join-Path $installationPath "VC\Tools\Llvm\bin\clang-tidy.exe"),
        (Join-Path $installationPath "VC\Tools\Llvm\ARM64\bin\clang-tidy.exe"),
        "C:\Program Files\LLVM\bin\clang-tidy.exe"
    )

    foreach ($candidate in $candidates)
    {
        if (Test-Path $candidate)
        {
            return $candidate
        }
    }

    throw "Unable to locate clang-tidy.exe. Install the Visual Studio LLVM tools or LLVM.LLVM."
}

function Find-CppcheckPath
{
    $command = Get-Command cppcheck -ErrorAction SilentlyContinue
    if ($null -ne $command)
    {
        return $command.Source
    }

    $candidates = @(
        "C:\Program Files\Cppcheck\cppcheck.exe",
        "C:\Program Files (x86)\Cppcheck\cppcheck.exe")

    foreach ($candidate in $candidates)
    {
        if (Test-Path $candidate)
        {
            return $candidate
        }
    }

    throw "Unable to locate cppcheck.exe. Install Cppcheck.Cppcheck or add cppcheck to PATH."
}

function Find-LlvmToolPath([string]$ToolName)
{
    $command = Get-Command $ToolName -ErrorAction SilentlyContinue
    if ($null -ne $command)
    {
        return $command.Source
    }

    $candidate = Join-Path "C:\Program Files\LLVM\bin" $ToolName
    if (Test-Path $candidate)
    {
        return $candidate
    }

    throw "Unable to locate $ToolName. Install LLVM.LLVM or add it to PATH."
}

function Find-NinjaPath
{
    $command = Get-Command ninja -ErrorAction SilentlyContinue
    if ($null -ne $command)
    {
        return $command.Source
    }

    $installationPath = Find-VsInstallationPath
    $candidate = Join-Path $installationPath "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    if (Test-Path $candidate)
    {
        return $candidate
    }

    throw "Unable to locate ninja.exe. Install the Visual Studio CMake components or add Ninja to PATH."
}

function Get-TrackedFiles([string]$RepoRoot, [string[]]$Patterns)
{
    $files = & git -C $RepoRoot ls-files -- @Patterns
    if ($LASTEXITCODE -ne 0)
    {
        throw "git ls-files failed."
    }

    return @($files | Where-Object { $_ -and -not $_.StartsWith("third_party/") -and -not $_.StartsWith("build/") })
}

function Get-FilteredTrackedFiles([string[]]$TrackedFiles, [string[]]$Filters)
{
    $normalizedFilters = @($Filters | ForEach-Object { Normalize-RepoPath $_ })
    return @(
        $TrackedFiles | Where-Object {
            $file = Normalize-RepoPath $_
            $matchesFilter = $false
            foreach ($filter in $normalizedFilters)
            {
                if ($file -eq $filter -or $file.StartsWith($filter.TrimEnd('/') + "/"))
                {
                    $matchesFilter = $true
                    break
                }
            }

            $matchesFilter
        }
    )
}

function Invoke-VcCommand
{
    param(
        [ValidateSet("x86", "x64")]
        [string]$Architecture,
        [string[]]$Arguments,
        [string]$ExtraPrefix = ""
    )

    $vcVarsAll = Find-VcVarsAllPath
    $quotedArguments = @($Arguments | ForEach-Object { Quote-ForCmd $_ }) -join " "
    $commandLine =
        "call " + (Quote-ForCmd $vcVarsAll) + " " + $Architecture + " >nul && " +
        $ExtraPrefix +
        $quotedArguments

    $output = cmd /c ($commandLine + " 2>&1")
    return [pscustomobject]@{
        CommandLine = $commandLine
        ExitCode = $LASTEXITCODE
        Output = @($output)
    }
}

function Get-DependencySourceArguments([string]$ReferenceBuildDir)
{
    $dependencySources = @{
        "RAYLIB" = "raylib-src"
        "NLOHMANN_JSON" = "nlohmann_json-src"
        "ENTT" = "entt-src"
        "PROTOBUF" = "protobuf-src"
        "IMGUI" = "imgui-src"
        "RLIMGUI" = "rlimgui-src"
        "GOOGLETEST" = "googletest-src"
    }

    $arguments = @()
    $depsRoot = Join-Path $ReferenceBuildDir "_deps"
    foreach ($entry in $dependencySources.GetEnumerator())
    {
        $sourcePath = Join-Path $depsRoot $entry.Value
        if (Test-Path $sourcePath)
        {
            $arguments += "-DFETCHCONTENT_SOURCE_DIR_{0}={1}" -f $entry.Key, $sourcePath
        }
    }

    return $arguments
}

function Ensure-CompilationDatabase
{
    param(
        [string]$RepoRoot,
        [string]$BuildDir,
        [ValidateSet("x86", "x64")]
        [string]$Architecture,
        [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
        [string]$Configuration,
        [string]$ReferenceBuildDir,
        [bool]$Reconfigure
    )

    $compileCommands = Join-Path $BuildDir "compile_commands.json"
    if ((-not $Reconfigure) -and (Test-Path $compileCommands))
    {
        Write-Section "Reuse Analysis Build"
        Write-KeyValue -Name "compile_commands" -Value $compileCommands
        return $compileCommands
    }

    Write-Section "Configure Analysis Build"

    Ensure-Directory -Path $BuildDir
    $cmakeArguments = @(
        "cmake",
        "-S", $RepoRoot,
        "-B", $BuildDir,
        "-G", "NMake Makefiles",
        "-DCMAKE_BUILD_TYPE=$Configuration",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        "-DFETCHCONTENT_UPDATES_DISCONNECTED=ON",
        "-DMFD_BUILD_DEMO=ON",
        "-DMFD_BUILD_TESTS=ON"
    )
    $cmakeArguments += Get-DependencySourceArguments -ReferenceBuildDir $ReferenceBuildDir
    $dependencySourceCount = @($cmakeArguments | Where-Object { $_ -like "-DFETCHCONTENT_SOURCE_DIR_*" }).Count

    Write-KeyValue -Name "repo root" -Value $RepoRoot
    Write-KeyValue -Name "build dir" -Value $BuildDir
    Write-KeyValue -Name "reference build dir" -Value $ReferenceBuildDir
    Write-KeyValue -Name "architecture" -Value $Architecture
    Write-KeyValue -Name "configuration" -Value $Configuration
    Write-KeyValue -Name "dependency sources" -Value $dependencySourceCount

    $result = Invoke-VcCommand -Architecture $Architecture -Arguments $cmakeArguments
    Write-CommandLine -Label "cmake command" -CommandLine $result.CommandLine
    if ($result.ExitCode -ne 0)
    {
        $result.Output | ForEach-Object { Write-Host $_ }
        throw "CMake failed while generating the compilation database."
    }

    if (-not (Test-Path $compileCommands))
    {
        throw "compile_commands.json was not generated in $BuildDir"
    }

    return $compileCommands
}

function Invoke-AnalysisBuildTarget
{
    param(
        [ValidateSet("x86", "x64")]
        [string]$Architecture,
        [string]$BuildDir,
        [string]$TargetName
    )

    Write-Section "Prepare Analysis Generated Files"
    Write-KeyValue -Name "analysis build dir" -Value $BuildDir
    Write-KeyValue -Name "analysis target" -Value $TargetName

    $arguments = @(
        "cmake",
        "--build", $BuildDir,
        "--target", $TargetName
    )

    $result = Invoke-VcCommand -Architecture $Architecture -Arguments $arguments
    Write-CommandLine -Label "analysis build command" -CommandLine $result.CommandLine
    if ($result.ExitCode -ne 0)
    {
        $result.Output | ForEach-Object { Write-Host $_ }
        throw "Failed to build analysis prerequisite target '$TargetName'."
    }
}
