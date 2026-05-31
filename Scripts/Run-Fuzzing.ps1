<#
.SYNOPSIS
Runs the dedicated external fuzzing build and the selected fuzz targets.

.DESCRIPTION
This script can be launched from any working directory. It resolves the
repository root from the script location, configures or reuses one dedicated
Clang/Ninja build, then launches the requested fuzz targets and writes logs and
artifacts under the report directory.

.EXAMPLE
.\Scripts\Run-Fuzzing.ps1 -DurationSeconds 30

.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\Run-Fuzzing.ps1 -Targets mfd_api_json_fuzzer -DurationSeconds 10

.EXAMPLE
Get-Help .\Scripts\Run-Fuzzing.ps1 -Detailed
#>
[CmdletBinding()]
param(
    [ValidateSet("Release", "RelWithDebInfo")]
    [string]$Configuration = "RelWithDebInfo",

    [int]$DurationSeconds = 60,
    [string]$BuildDir = "",
    [string]$ReferenceBuildDir = "",
    [string]$ReportDir = "",

    [string[]]$Targets = @(
        "mfd_api_json_fuzzer",
        "mfd_api_transport_fuzzer",
        "mfd_editor_serializer_fuzzer"
    ),

    [switch]$Reconfigure
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "QualityCommon.ps1")

function Get-FuzzerMetadata([string]$RepoRoot)
{
    return @{
        "mfd_api_json_fuzzer" = @{
            CorpusDir = Join-Path $RepoRoot "tests/fuzz/corpus/api_json"
            Description = "JSON loader and nested window/page validation"
        }
        "mfd_api_transport_fuzzer" = @{
            CorpusDir = Join-Path $RepoRoot "tests/fuzz/corpus/api_transport"
            Description = "Protocol Buffers command and feedback decoding"
        }
        "mfd_editor_serializer_fuzzer" = @{
            CorpusDir = Join-Path $RepoRoot "tests/fuzz/corpus/editor"
            Description = "Editor JSON serialization from mutated in-memory models"
        }
    }
}

function Configure-FuzzBuild
{
    param(
        [string]$RepoRoot,
        [string]$BuildDir,
        [string]$ReferenceBuildDir,
        [string]$Configuration,
        [bool]$ReconfigureRequested
    )

    $buildFile = Join-Path $BuildDir "build.ninja"
    if ((-not $ReconfigureRequested) -and (Test-Path $buildFile))
    {
        Write-Section "Reuse Fuzz Build"
        Write-KeyValue -Name "build file" -Value $buildFile
        return
    }

    Write-Section "Configure Fuzz Build"
    if ($ReconfigureRequested)
    {
        Reset-Directory -Path $BuildDir
    }
    else
    {
        Ensure-Directory -Path $BuildDir
    }
    $ninjaPath = Find-NinjaPath
    $clangPath = Find-LlvmToolPath -ToolName "clang.exe"
    $clangxxPath = Find-LlvmToolPath -ToolName "clang++.exe"

    Write-KeyValue -Name "ninja" -Value $ninjaPath
    Write-KeyValue -Name "clang" -Value $clangPath
    Write-KeyValue -Name "clang++" -Value $clangxxPath

    $arguments = @(
        "cmake",
        "-S", $RepoRoot,
        "-B", $BuildDir,
        "-G", "Ninja",
        "-DCMAKE_MAKE_PROGRAM=$ninjaPath",
        "-DCMAKE_BUILD_TYPE=$Configuration",
        "-DCMAKE_C_COMPILER=$clangPath",
        "-DCMAKE_CXX_COMPILER=$clangxxPath",
        "-DFETCHCONTENT_UPDATES_DISCONNECTED=ON",
        "-DMFD_BUILD_DEMO=OFF",
        "-DMFD_BUILD_TESTS=OFF",
        "-DMFD_ENABLE_FUZZING=ON",
        "-DMFD_MSVC_RUNTIME=static"
    )
    $arguments += Get-DependencySourceArguments -ReferenceBuildDir $ReferenceBuildDir

    $result = Invoke-VcCommand -Architecture "x64" -Arguments $arguments
    Write-CommandLine -Label "cmake command" -CommandLine $result.CommandLine
    if ($result.ExitCode -ne 0)
    {
        $result.Output | ForEach-Object { Write-Host $_ }
        throw "CMake failed while configuring the dedicated fuzzing build."
    }
}

function Build-FuzzerTargets
{
    param(
        [string]$BuildDir,
        [string[]]$TargetsToBuild
    )

    Write-Section "Build Fuzzer Targets"
    $arguments = @(
        "cmake",
        "--build", $BuildDir,
        "--parallel",
        "--target"
    ) + $TargetsToBuild

    $result = Invoke-VcCommand -Architecture "x64" -Arguments $arguments
    Write-CommandLine -Label "build command" -CommandLine $result.CommandLine
    if ($result.ExitCode -ne 0)
    {
        $result.Output | ForEach-Object { Write-Host $_ }
        throw "CMake failed while building the dedicated fuzzing targets."
    }
}

function Invoke-Fuzzer
{
    param(
        [string]$ExecutablePath,
        [string]$CorpusDir,
        [string]$ArtifactDir,
        [string]$LogPath,
        [int]$DurationSeconds,
        [string]$SymbolizerPath
    )

    Ensure-Directory -Path $ArtifactDir

    $previousSymbolizer = $env:ASAN_SYMBOLIZER_PATH
    $env:ASAN_SYMBOLIZER_PATH = $SymbolizerPath
    $env:LLVM_SYMBOLIZER_PATH = $SymbolizerPath
    $env:UBSAN_OPTIONS = "print_stacktrace=1"

    try
    {
        $arguments = @(
            $CorpusDir,
            "-artifact_prefix=$ArtifactDir/",
            "-max_total_time=$DurationSeconds",
            "-print_final_stats=1",
            "-use_value_profile=1",
            "-timeout=10",
            "-rss_limit_mb=4096"
        )

        $previousErrorActionPreference = $ErrorActionPreference
        try
        {
            $ErrorActionPreference = "Continue"
            $output = & $ExecutablePath @arguments 2>&1
            $exitCode = $LASTEXITCODE
        }
        finally
        {
            $ErrorActionPreference = $previousErrorActionPreference
        }

        Set-Content -LiteralPath $LogPath -Value @($output) -Encoding UTF8
    }
    finally
    {
        if ($null -eq $previousSymbolizer)
        {
            Remove-Item Env:ASAN_SYMBOLIZER_PATH -ErrorAction SilentlyContinue
        }
        else
        {
            $env:ASAN_SYMBOLIZER_PATH = $previousSymbolizer
        }
    }

    $crashArtifacts = @(
        Get-ChildItem -LiteralPath $ArtifactDir -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^(crash|leak|timeout|oom)-' }
    )

    return [pscustomobject]@{
        ExitCode = $exitCode
        CrashArtifacts = $crashArtifacts.Count
        CrashArtifactNames = @($crashArtifacts | ForEach-Object { $_.Name })
        LogPath = $LogPath
        Success = ($exitCode -eq 0 -and $crashArtifacts.Count -eq 0)
    }
}

$repoRoot = Get-RepoRoot

if ([string]::IsNullOrWhiteSpace($BuildDir))
{
    $BuildDir = Join-Path $repoRoot "build/clang-fuzz-x64"
}
else
{
    $BuildDir = Resolve-OptionalPath -CandidatePath $BuildDir -RepoRoot $repoRoot
}

if ([string]::IsNullOrWhiteSpace($ReferenceBuildDir))
{
    $ReferenceBuildDir = Join-Path $repoRoot "build/vs2022-x64"
}
else
{
    $ReferenceBuildDir = Resolve-OptionalPath -CandidatePath $ReferenceBuildDir -RepoRoot $repoRoot
}

if ([string]::IsNullOrWhiteSpace($ReportDir))
{
    $ReportDir = Join-Path $repoRoot "outputs/quality/fuzz/latest"
}
else
{
    $ReportDir = Resolve-OptionalPath -CandidatePath $ReportDir -RepoRoot $repoRoot
}

Reset-Directory -Path $ReportDir

Write-Section "Run Fuzzing"
Write-KeyValue -Name "repo root" -Value $repoRoot
Write-KeyValue -Name "build dir" -Value $BuildDir
Write-KeyValue -Name "reference build dir" -Value $ReferenceBuildDir
Write-KeyValue -Name "report dir" -Value $ReportDir
Write-KeyValue -Name "configuration" -Value $Configuration
Write-KeyValue -Name "duration seconds" -Value $DurationSeconds
Write-StringList -Name "targets" -Values $Targets

$fuzzerMetadata = Get-FuzzerMetadata -RepoRoot $repoRoot
foreach ($target in $Targets)
{
    if (-not $fuzzerMetadata.ContainsKey($target))
    {
        throw "Unknown fuzzer target '$target'."
    }
}

Configure-FuzzBuild `
    -RepoRoot $repoRoot `
    -BuildDir $BuildDir `
    -ReferenceBuildDir $ReferenceBuildDir `
    -Configuration $Configuration `
    -ReconfigureRequested $Reconfigure.IsPresent

Build-FuzzerTargets -BuildDir $BuildDir -TargetsToBuild $Targets

$llvmSymbolizer = Find-LlvmToolPath -ToolName "llvm-symbolizer.exe"
$targetResults = New-Object System.Collections.Generic.List[object]

Write-Section "Run Fuzzers"
foreach ($target in $Targets)
{
    $metadata = $fuzzerMetadata[$target]
    $executablePath = Join-Path $BuildDir ("fuzz/" + $target + ".exe")
    $artifactDir = Join-Path $ReportDir ("artifacts/" + $target)
    $logPath = Join-Path $ReportDir ($target + ".log")

    Write-Host ("Running {0}" -f $target)
    Write-KeyValue -Name "executable" -Value $executablePath
    Write-KeyValue -Name "corpus" -Value $metadata.CorpusDir
    Write-KeyValue -Name "artifact dir" -Value $artifactDir
    Write-KeyValue -Name "log file" -Value $logPath
    $result = Invoke-Fuzzer `
        -ExecutablePath $executablePath `
        -CorpusDir $metadata.CorpusDir `
        -ArtifactDir $artifactDir `
        -LogPath $logPath `
        -DurationSeconds $DurationSeconds `
        -SymbolizerPath $llvmSymbolizer

    $targetResults.Add([pscustomobject]@{
            Target = $target
            Description = $metadata.Description
            ExitCode = $result.ExitCode
            CrashArtifacts = $result.CrashArtifacts
            CrashArtifactNames = $result.CrashArtifactNames
            LogPath = Normalize-RepoPath $logPath
            Success = $result.Success
        })
}

$success = (@($targetResults | Where-Object { -not $_.Success }).Count -eq 0)
$summary = [pscustomobject]@{
    Tool = "libfuzzer"
    BuildDir = Normalize-RepoPath $BuildDir
    ReportDir = Normalize-RepoPath $ReportDir
    Configuration = $Configuration
    DurationSecondsPerTarget = $DurationSeconds
    Targets = [object[]]$targetResults
    Success = $success
}

Write-JsonFile -Path (Join-Path $ReportDir "summary.json") -Value $summary

$markdownLines = New-Object System.Collections.Generic.List[string]
$markdownLines.Add("# fuzzing summary")
$markdownLines.Add("")
$markdownLines.Add("- Build directory: ``$BuildDir``")
$markdownLines.Add("- Configuration: $Configuration")
$markdownLines.Add("- Duration per target: $DurationSeconds seconds")
$markdownLines.Add("- Success: $success")
foreach ($targetResult in $targetResults)
{
    $markdownLines.Add("- $($targetResult.Target): exit=$($targetResult.ExitCode), crashes=$($targetResult.CrashArtifacts), success=$($targetResult.Success)")
}
Set-Content -LiteralPath (Join-Path $ReportDir "summary.md") -Value $markdownLines -Encoding UTF8

Write-Section "Summary"
Write-Host ("fuzzer targets   : {0}" -f $Targets.Count)
Write-Host ("report directory : {0}" -f $ReportDir)
Write-KeyValue -Name "summary json" -Value (Join-Path $ReportDir "summary.json")
Write-KeyValue -Name "summary md" -Value (Join-Path $ReportDir "summary.md")

if (-not $success)
{
    exit 1
}

exit 0
