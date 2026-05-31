<#
.SYNOPSIS
Runs the full repository quality suite and consolidates all reports.

.DESCRIPTION
This script can be launched from any working directory. It orchestrates the
Win32 GoogleTest build, clang-tidy, cppcheck, and the dedicated fuzzing build,
then writes one consolidated report directory.

.EXAMPLE
.\Scripts\Run-QualitySuite.ps1

.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\Run-QualitySuite.ps1 -SkipFuzzing -Reconfigure

.EXAMPLE
Get-Help .\Scripts\Run-QualitySuite.ps1 -Detailed
#>
[CmdletBinding()]
param(
    [ValidateSet("x86", "x64")]
    [string]$Architecture = "x86",

    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Debug",

    [string]$ReportDir = "",
    [int]$FuzzDurationSeconds = 60,
    [switch]$Reconfigure,
    [switch]$SkipClangTidy,
    [switch]$SkipCppcheck,
    [switch]$SkipFuzzing,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "QualityCommon.ps1")

function Invoke-LoggedCommand
{
    param(
        [string]$LogPath,
        [scriptblock]$Command
    )

    $outputLines = New-Object System.Collections.Generic.List[string]
    $previousErrorActionPreference = $ErrorActionPreference
    try
    {
        $ErrorActionPreference = "Continue"
        & $Command 2>&1 | ForEach-Object {
            $line = $_.ToString()
            Write-Host $line
            $outputLines.Add($line)
        }
        $exitCode = $LASTEXITCODE
    }
    finally
    {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    Set-Content -LiteralPath $LogPath -Value ([string[]]$outputLines) -Encoding UTF8

    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = [string[]]$outputLines
    }
}

function Parse-JUnitSummary([string]$XmlPath)
{
    if (-not (Test-Path $XmlPath))
    {
        return [pscustomobject]@{
            Tests = 0
            Failures = 0
            Errors = 0
            Skipped = 0
        }
    }

    [xml]$document = Get-Content -LiteralPath $XmlPath
    $root = $document.testsuites
    if ($null -eq $root)
    {
        $root = $document.testsuite
    }

    return [pscustomobject]@{
        Tests = [int]$root.tests
        Failures = [int]$root.failures
        Errors = [int]$root.errors
        Skipped = [int]$root.skipped
    }
}

function Invoke-GoogleTests
{
    param(
        [string]$RepoRoot,
        [string]$ReportRoot,
        [bool]$ReconfigureRequested
    )

    $testsReportDir = Join-Path $ReportRoot "tests"
    Ensure-Directory -Path $testsReportDir
    $configureLog = Join-Path $testsReportDir "configure.log"
    $buildLog = Join-Path $testsReportDir "build.log"
    $ctestLog = Join-Path $testsReportDir "ctest.log"
    $junitPath = Join-Path $testsReportDir "ctest-results.xml"
    $buildDirectory = Join-Path $RepoRoot "build/vs2022-win32"

    if ($ReconfigureRequested -or -not (Test-Path (Join-Path $buildDirectory "CMakeCache.txt")))
    {
        Write-Section "Configure Win32 Tests"
        Write-KeyValue -Name "configure command" -Value "cmake --preset vs2022-win32"
        $configureResult = Invoke-LoggedCommand -LogPath $configureLog -Command {
            cmake --preset vs2022-win32
        }
        if ($configureResult.ExitCode -ne 0)
        {
            return [pscustomobject]@{
                Tool = "gtest"
                Success = $false
                ExitCode = $configureResult.ExitCode
                ConfigureLog = Normalize-RepoPath $configureLog
                BuildLog = Normalize-RepoPath $buildLog
                TestLog = Normalize-RepoPath $ctestLog
                Tests = 0
                Failures = 0
                Errors = 1
                Skipped = 0
            }
        }
    }

    Write-Section "Build Win32 Tests"
    Write-KeyValue -Name "build command" -Value "cmake --build --preset debug-win32 --parallel"
    $buildResult = Invoke-LoggedCommand -LogPath $buildLog -Command {
        cmake --build --preset debug-win32 --parallel
    }
    if ($buildResult.ExitCode -ne 0)
    {
        return [pscustomobject]@{
            Tool = "gtest"
            Success = $false
            ExitCode = $buildResult.ExitCode
            ConfigureLog = Normalize-RepoPath $configureLog
            BuildLog = Normalize-RepoPath $buildLog
            TestLog = Normalize-RepoPath $ctestLog
            Tests = 0
            Failures = 0
            Errors = 1
            Skipped = 0
        }
    }

    Write-Section "Run GoogleTest Suite"
    Write-KeyValue -Name "ctest command" -Value ("ctest --preset test-debug-win32 --output-junit " + $junitPath)
    $ctestResult = Invoke-LoggedCommand -LogPath $ctestLog -Command {
        ctest --preset test-debug-win32 --output-junit $junitPath
    }
    $testSummary = Parse-JUnitSummary -XmlPath $junitPath
    return [pscustomobject]@{
        Tool = "gtest"
        Success = ($ctestResult.ExitCode -eq 0 -and $testSummary.Failures -eq 0 -and $testSummary.Errors -eq 0)
        ExitCode = $ctestResult.ExitCode
        ConfigureLog = Normalize-RepoPath $configureLog
        BuildLog = Normalize-RepoPath $buildLog
        TestLog = Normalize-RepoPath $ctestLog
        JUnit = Normalize-RepoPath $junitPath
        Tests = $testSummary.Tests
        Failures = $testSummary.Failures
        Errors = $testSummary.Errors
        Skipped = $testSummary.Skipped
    }
}

function Invoke-SubReportScript
{
    param(
        [string]$ScriptPath,
        [System.Collections.IDictionary]$Parameters,
        [string]$SummaryPath
    )

    $commandSegments = New-Object System.Collections.Generic.List[string]
    $commandSegments.Add($ScriptPath)
    foreach ($entry in $Parameters.GetEnumerator())
    {
        if ($entry.Value -is [bool])
        {
            if ([bool]$entry.Value)
            {
                $commandSegments.Add("-$($entry.Key)")
            }

            continue
        }

        $commandSegments.Add("-$($entry.Key)")
        $commandSegments.Add([string]$entry.Value)
    }

    Write-CommandLine -Label "sub-script call" -CommandLine ($commandSegments -join " ")
    & $ScriptPath @Parameters
    $exitCode = $LASTEXITCODE
    $summary = $null
    if (Test-Path $SummaryPath)
    {
        $summary = Get-Content -LiteralPath $SummaryPath -Raw | ConvertFrom-Json
    }

    return [pscustomobject]@{
        ExitCode = $exitCode
        Summary = $summary
    }
}

$repoRoot = Get-RepoRoot
if ([string]::IsNullOrWhiteSpace($ReportDir))
{
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $ReportDir = Join-Path $repoRoot ("outputs/quality/runs/" + $timestamp)
}
else
{
    $ReportDir = Resolve-OptionalPath -CandidatePath $ReportDir -RepoRoot $repoRoot
}

Reset-Directory -Path $ReportDir

Write-Section "Run Quality Suite"
Write-KeyValue -Name "repo root" -Value $repoRoot
Write-KeyValue -Name "report dir" -Value $ReportDir
Write-KeyValue -Name "architecture" -Value $Architecture
Write-KeyValue -Name "configuration" -Value $Configuration
Write-KeyValue -Name "fuzz seconds" -Value $FuzzDurationSeconds
Write-KeyValue -Name "skip tests" -Value $SkipTests.IsPresent
Write-KeyValue -Name "skip clang-tidy" -Value $SkipClangTidy.IsPresent
Write-KeyValue -Name "skip cppcheck" -Value $SkipCppcheck.IsPresent
Write-KeyValue -Name "skip fuzzing" -Value $SkipFuzzing.IsPresent

$results = New-Object System.Collections.Generic.List[object]

if (-not $SkipTests)
{
    $results.Add((Invoke-GoogleTests -RepoRoot $repoRoot -ReportRoot $ReportDir -ReconfigureRequested $Reconfigure.IsPresent))
}

if (-not $SkipClangTidy)
{
    $clangReportDir = Join-Path $ReportDir "clang-tidy"
    Write-KeyValue -Name "clang-tidy script" -Value (Join-Path $PSScriptRoot "Run-ClangTidy.ps1")
    $clangParameters = [ordered]@{
        Architecture = $Architecture
        Configuration = $Configuration
        ReportDir = $clangReportDir
    }
    if ($Reconfigure)
    {
        $clangParameters["Reconfigure"] = $true
    }
    $clangResult = Invoke-SubReportScript `
        -ScriptPath (Join-Path $PSScriptRoot "Run-ClangTidy.ps1") `
        -Parameters $clangParameters `
        -SummaryPath (Join-Path $clangReportDir "summary.json")

    if ($null -ne $clangResult.Summary)
    {
        $results.Add($clangResult.Summary)
    }
    else
    {
        $results.Add([pscustomobject]@{
                Tool = "clang-tidy"
                Success = $false
                ExitCode = $clangResult.ExitCode
            })
    }
}

if (-not $SkipCppcheck)
{
    $cppcheckReportDir = Join-Path $ReportDir "cppcheck"
    Write-KeyValue -Name "cppcheck script" -Value (Join-Path $PSScriptRoot "Run-Cppcheck.ps1")
    $cppcheckParameters = [ordered]@{
        Architecture = $Architecture
        Configuration = $Configuration
        ReportDir = $cppcheckReportDir
    }
    if ($Reconfigure)
    {
        $cppcheckParameters["Reconfigure"] = $true
    }
    $cppcheckResult = Invoke-SubReportScript `
        -ScriptPath (Join-Path $PSScriptRoot "Run-Cppcheck.ps1") `
        -Parameters $cppcheckParameters `
        -SummaryPath (Join-Path $cppcheckReportDir "summary.json")

    if ($null -ne $cppcheckResult.Summary)
    {
        $results.Add($cppcheckResult.Summary)
    }
    else
    {
        $results.Add([pscustomobject]@{
                Tool = "cppcheck"
                Success = $false
                ExitCode = $cppcheckResult.ExitCode
            })
    }
}

if (-not $SkipFuzzing)
{
    $fuzzReportDir = Join-Path $ReportDir "fuzz"
    Write-KeyValue -Name "fuzz script" -Value (Join-Path $PSScriptRoot "Run-Fuzzing.ps1")
    $fuzzParameters = [ordered]@{
        Configuration = "RelWithDebInfo"
        DurationSeconds = $FuzzDurationSeconds
        ReportDir = $fuzzReportDir
    }
    if ($Reconfigure)
    {
        $fuzzParameters["Reconfigure"] = $true
    }
    $fuzzResult = Invoke-SubReportScript `
        -ScriptPath (Join-Path $PSScriptRoot "Run-Fuzzing.ps1") `
        -Parameters $fuzzParameters `
        -SummaryPath (Join-Path $fuzzReportDir "summary.json")

    if ($null -ne $fuzzResult.Summary)
    {
        $results.Add($fuzzResult.Summary)
    }
    else
    {
        $results.Add([pscustomobject]@{
                Tool = "libfuzzer"
                Success = $false
                ExitCode = $fuzzResult.ExitCode
            })
    }
}

$success = (@($results | Where-Object { -not $_.Success }).Count -eq 0)
$summary = [pscustomobject]@{
    Tool = "quality-suite"
    ReportDir = Normalize-RepoPath $ReportDir
    Configuration = $Configuration
    Architecture = $Architecture
    FuzzDurationSeconds = $FuzzDurationSeconds
    Steps = [object[]]$results
    Success = $success
}

Write-JsonFile -Path (Join-Path $ReportDir "summary.json") -Value $summary

$markdownLines = New-Object System.Collections.Generic.List[string]
$markdownLines.Add("# quality suite summary")
$markdownLines.Add("")
$markdownLines.Add("- Report directory: ``$ReportDir``")
$markdownLines.Add("- Success: $success")
foreach ($result in $results)
{
    $tool = [string]$result.Tool
    $stepSuccess = [string]$result.Success
    $markdownLines.Add("- ${tool}: success=$stepSuccess")
}

Set-Content -LiteralPath (Join-Path $ReportDir "summary.md") -Value $markdownLines -Encoding UTF8

Write-Section "Summary"
Write-Host ("report directory : {0}" -f $ReportDir)
Write-Host ("overall success  : {0}" -f $success)

if (-not $success)
{
    exit 1
}

exit 0
