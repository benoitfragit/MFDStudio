<#
.SYNOPSIS
Runs cppcheck on the repository and writes a report.

.DESCRIPTION
This script can be launched from any working directory. It resolves the
repository root from the script location, prepares or reuses a compilation
database, then runs cppcheck with the configured path filters. By default the
scope is limited to production code; tests and examples must be requested
explicitly with -Paths.

.EXAMPLE
.\Scripts\Run-Cppcheck.ps1

.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\Run-Cppcheck.ps1 -Architecture x64 -Configuration Debug

.EXAMPLE
Get-Help .\Scripts\Run-Cppcheck.ps1 -Detailed
#>
[CmdletBinding()]
param(
    [ValidateSet("x86", "x64")]
    [string]$Architecture = "x86",

    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Debug",

    [string]$BuildDir = "",
    [string]$ReferenceBuildDir = "",
    [string]$ReportDir = "",

    [string[]]$Paths = @(
        "mfd_api",
        "mfd_client_api",
        "mfd_common_api",
        "mfd_window",
        "mfd_window_plugin_api",
        "mfd_editor"
    ),

    [switch]$Reconfigure
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "QualityCommon.ps1")

function Build-FileFilterArguments([string[]]$Filters)
{
    $arguments = @()
    foreach ($filter in $Filters)
    {
        $normalized = Normalize-RepoPath $filter
        $arguments += "--file-filter=*$normalized*"
    }

    return $arguments
}

function Build-IgnoreArguments([string]$RepoRoot)
{
    return @(
        "-i$(Join-Path $RepoRoot 'build')",
        "-i$(Join-Path $RepoRoot 'third_party')"
    )
}

function Parse-CppcheckXml([string]$XmlPath, [string]$RepoRoot, [string[]]$ScopedFiles)
{
    $scopedFileSet = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($file in $ScopedFiles)
    {
        [void]$scopedFileSet.Add((Normalize-RepoPath $file))
    }

    if (-not (Test-Path $XmlPath))
    {
        return [pscustomobject]@{
            TotalCount = 0
            RawTotalCount = 0
            FilteredOutCount = 0
            BySeverity = @{}
            Findings = @()
        }
    }

    [xml]$document = Get-Content -LiteralPath $XmlPath
    $severityCounts = @{}
    $findings = New-Object System.Collections.Generic.List[object]
    $rawTotalCount = 0
    $filteredOutCount = 0

    foreach ($error in @($document.results.errors.error))
    {
        if ($null -eq $error)
        {
            continue
        }

        $rawTotalCount += 1
        $firstLocation = if ($error.location) { @($error.location)[0] } else { $null }
        $scopeFile = ""
        if ($null -ne $firstLocation -and -not [string]::IsNullOrWhiteSpace([string]$firstLocation.file))
        {
            $scopeFile = Convert-ToRepoRelativePath -Path ([string]$firstLocation.file) -RepoRoot $RepoRoot
        }
        elseif (-not [string]::IsNullOrWhiteSpace([string]$error.file0))
        {
            $scopeFile = Convert-ToRepoRelativePath -Path ([string]$error.file0) -RepoRoot $RepoRoot
        }

        if ([string]::IsNullOrWhiteSpace($scopeFile) -or -not $scopedFileSet.Contains($scopeFile))
        {
            $filteredOutCount += 1
            continue
        }

        $severity = [string]$error.severity
        if (-not $severityCounts.ContainsKey($severity))
        {
            $severityCounts[$severity] = 0
        }

        $severityCounts[$severity] += 1
        $location = ""
        if ($null -ne $firstLocation)
        {
            $line = [string]$firstLocation.line
            if ([string]::IsNullOrWhiteSpace($line))
            {
                $location = $scopeFile
            }
            else
            {
                $location = "{0}:{1}" -f $scopeFile, $line
            }
        }
        else
        {
            $location = $scopeFile
        }

        $findings.Add([pscustomobject]@{
                Severity = $severity
                Id = [string]$error.id
                Message = [string]$error.msg
                Verbose = [string]$error.verbose
                Location = $location
            })
    }

    return [pscustomobject]@{
        TotalCount = $findings.Count
        RawTotalCount = $rawTotalCount
        FilteredOutCount = $filteredOutCount
        BySeverity = $severityCounts
        Findings = [object[]]$findings
    }
}

$repoRoot = Get-RepoRoot

if ([string]::IsNullOrWhiteSpace($BuildDir))
{
    $analysisSuffix = if ($Architecture -eq "x86") { "win32" } else { "x64" }
    $BuildDir = Join-Path $repoRoot ("build/cppcheck-" + $analysisSuffix)
}
else
{
    $BuildDir = Resolve-OptionalPath -CandidatePath $BuildDir -RepoRoot $repoRoot
}

if ([string]::IsNullOrWhiteSpace($ReferenceBuildDir))
{
    $referenceSuffix = if ($Architecture -eq "x86") { "vs2022-win32" } else { "vs2022-x64" }
    $ReferenceBuildDir = Join-Path $repoRoot ("build/" + $referenceSuffix)
}
else
{
    $ReferenceBuildDir = Resolve-OptionalPath -CandidatePath $ReferenceBuildDir -RepoRoot $repoRoot
}

if ([string]::IsNullOrWhiteSpace($ReportDir))
{
    $ReportDir = Join-Path $repoRoot "outputs/quality/cppcheck/latest"
}
else
{
    $ReportDir = Resolve-OptionalPath -CandidatePath $ReportDir -RepoRoot $repoRoot
}

Reset-Directory -Path $ReportDir
Ensure-Directory -Path (Join-Path $BuildDir "cppcheck-cache")

Write-Section "Run Cppcheck"
Write-KeyValue -Name "repo root" -Value $repoRoot
Write-KeyValue -Name "build dir" -Value $BuildDir
Write-KeyValue -Name "reference build dir" -Value $ReferenceBuildDir
Write-KeyValue -Name "report dir" -Value $ReportDir
Write-KeyValue -Name "architecture" -Value $Architecture
Write-KeyValue -Name "configuration" -Value $Configuration
Write-StringList -Name "path filters" -Values $Paths

$trackedCodeFiles = Get-TrackedFiles -RepoRoot $repoRoot -Patterns @("*.cpp", "*.h", "*.hpp")
$filteredCodeFiles = Get-FilteredTrackedFiles -TrackedFiles $trackedCodeFiles -Filters $Paths
if ($filteredCodeFiles.Count -eq 0)
{
    throw "No tracked C++ files matched the requested Paths filter."
}
Write-KeyValue -Name "tracked files" -Value $filteredCodeFiles.Count

$compileCommands = Ensure-CompilationDatabase `
    -RepoRoot $repoRoot `
    -BuildDir $BuildDir `
    -Architecture $Architecture `
    -Configuration $Configuration `
    -ReferenceBuildDir $ReferenceBuildDir `
    -Reconfigure $Reconfigure.IsPresent
Write-KeyValue -Name "compile_commands" -Value $compileCommands

$cppcheckPath = Find-CppcheckPath
$xmlPath = Join-Path $ReportDir "cppcheck.xml"
$logPath = Join-Path $ReportDir "cppcheck.log"

Write-Section "cppcheck"
Write-Host ("Using cppcheck: {0}" -f $cppcheckPath)
Write-KeyValue -Name "progress mode" -Value "cppcheck --report-progress"

$arguments = @(
    "--project=$compileCommands",
    "--cppcheck-build-dir=$(Join-Path $BuildDir 'cppcheck-cache')",
    "--check-level=exhaustive",
    "--enable=warning,style,performance,portability,information",
    "--inconclusive",
    "--inline-suppr",
    "--std=c++17",
    "--report-progress",
    "--xml",
    "--xml-version=2",
    "--output-file=$xmlPath",
    "--suppress=missingIncludeSystem",
    "--error-exitcode=1"
)
$arguments += Build-FileFilterArguments -Filters $Paths
$arguments += Build-IgnoreArguments -RepoRoot $repoRoot

$quotedArguments = @($arguments | ForEach-Object { Quote-ForCmd $_ }) -join " "
$commandLine = (Quote-ForCmd $cppcheckPath) + " " + $quotedArguments
Write-CommandLine -Label "cppcheck command" -CommandLine $commandLine

$progressLines = New-Object System.Collections.Generic.List[string]
 $previousErrorActionPreference = $ErrorActionPreference
try
{
    $ErrorActionPreference = "Continue"
    & $cppcheckPath @arguments 2>&1 | ForEach-Object {
        $line = $_.ToString()
        Write-Host $line
        $progressLines.Add($line)
    }
    $exitCode = $LASTEXITCODE
}
finally
{
    $ErrorActionPreference = $previousErrorActionPreference
}
Set-Content -LiteralPath $logPath -Value $progressLines -Encoding UTF8

$xmlSummary = Parse-CppcheckXml -XmlPath $xmlPath -RepoRoot $repoRoot -ScopedFiles $filteredCodeFiles
$success = ($exitCode -le 1 -and $xmlSummary.TotalCount -eq 0)
$summary = [pscustomobject]@{
    Tool = "cppcheck"
    BuildDir = Normalize-RepoPath $BuildDir
    ReportDir = Normalize-RepoPath $ReportDir
    Paths = $Paths
    ExitCode = $exitCode
    RawFindingCount = $xmlSummary.RawTotalCount
    FindingCount = $xmlSummary.TotalCount
    FilteredOutCount = $xmlSummary.FilteredOutCount
    BySeverity = $xmlSummary.BySeverity
    Findings = $xmlSummary.Findings
    Success = $success
}

Write-JsonFile -Path (Join-Path $ReportDir "summary.json") -Value $summary

$markdownLines = New-Object System.Collections.Generic.List[string]
$markdownLines.Add("# cppcheck summary")
$markdownLines.Add("")
$markdownLines.Add("- Build directory: ``$BuildDir``")
$markdownLines.Add("- raw findings: $($xmlSummary.RawTotalCount)")
$markdownLines.Add("- findings: $($xmlSummary.TotalCount)")
$markdownLines.Add("- filtered out (build/dependency noise): $($xmlSummary.FilteredOutCount)")
$markdownLines.Add("- success: $success")
foreach ($severity in @($xmlSummary.BySeverity.Keys | Sort-Object))
{
    $markdownLines.Add("- ${severity}: $($xmlSummary.BySeverity[$severity])")
}

Set-Content -LiteralPath (Join-Path $ReportDir "summary.md") -Value $markdownLines -Encoding UTF8

Write-Section "Summary"
Write-Host ("cppcheck findings : {0}" -f $xmlSummary.TotalCount)
Write-Host ("report directory  : {0}" -f $ReportDir)
Write-KeyValue -Name "xml report" -Value $xmlPath
Write-KeyValue -Name "text log" -Value $logPath

if (-not $success)
{
    exit 1
}

exit 0
