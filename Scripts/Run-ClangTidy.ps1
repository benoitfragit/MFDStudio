<#
.SYNOPSIS
Runs clang-tidy and the repository AGENTS.md pattern audit.

.DESCRIPTION
This script can be launched from any working directory. It resolves the
repository root from the script location, prepares or reuses a compilation
database, runs the AGENTS.md pattern audit, then launches clang-tidy on the
selected repository paths. By default the scope is limited to production code;
tests and examples must be requested explicitly with -Paths.

.EXAMPLE
.\Scripts\Run-ClangTidy.ps1

.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\Run-ClangTidy.ps1 -Architecture x64 -Configuration Debug

.EXAMPLE
Get-Help .\Scripts\Run-ClangTidy.ps1 -Detailed
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

    [switch]$Reconfigure,
    [switch]$AuditOnly,
    [switch]$Fix
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "QualityCommon.ps1")

function Format-MatchLocation([string]$RepoRoot, $Match)
{
    $relativePath = Normalize-RepoPath($Match.Path.Substring($RepoRoot.Length + 1))
    return "{0}:{1}: {2}" -f $relativePath, $Match.LineNumber, $Match.Line.Trim()
}

function Convert-ToSafeFileName([string]$Value)
{
    $invalidCharacters = [System.IO.Path]::GetInvalidFileNameChars()
    $safeValue = $Value
    foreach ($character in $invalidCharacters)
    {
        $safeValue = $safeValue.Replace($character, '_')
    }

    return ($safeValue -replace '[\\/:\s]+', '_')
}

function Build-HeaderFilter([string[]]$Filters)
{
    $escapedFilters = @(
        $Filters |
            ForEach-Object { Normalize-RepoPath $_ } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            ForEach-Object {
                ([regex]::Escape($_)) -replace '/', '[/\\]'
            }
    )

    if ($escapedFilters.Count -eq 0)
    {
        throw "At least one path filter is required to build the clang-tidy header filter."
    }

    return "^(?!.*(build|third_party)).*(" + ($escapedFilters -join "|") + ")[/\\].*"
}

function Invoke-PatternAudit([string]$RepoRoot, [string[]]$TrackedCodeFiles, [string]$AuditLogPath)
{
    $rules = @(
        @{ Name = "forbidden static_cast<void> suppression"; Pattern = 'static_cast<void>\s*\('; Severity = "Error" },
        @{ Name = "forbidden (void)x suppression"; Pattern = '\(\s*void\s*\)\s*[A-Za-z_][A-Za-z0-9_]*'; Severity = "Error" },
        @{ Name = "structured binding placeholder '_'"; Pattern = '\[\s*_\s*,|,\s*_\s*\]'; Severity = "Error" },
        @{ Name = "immediately invoked lambda"; Pattern = '\}\s*\(\s*\)\s*;'; Severity = "Error" },
        @{ Name = "broad lambda capture [&]"; Pattern = '\[\&\]'; Severity = "Error" },
        @{ Name = "broad lambda capture [=]"; Pattern = '\[\=\]'; Severity = "Error" },
        @{ Name = "identifier names using ignored/dummy/unused prefixes"; Pattern = '\b(?:ignored|dummy)[A-Z0-9_][A-Za-z0-9_]*\b|\bunused[A-Z0-9_][A-Za-z0-9_]*\b'; Severity = "Warning" },
        @{ Name = "manual review for low-level casts"; Pattern = 'reinterpret_cast\s*<|const_cast\s*<'; Severity = "Warning" }
    )

    $reportLines = New-Object System.Collections.Generic.List[string]
    $findings = New-Object System.Collections.Generic.List[object]
    $errorCount = 0
    $warningCount = 0

    Write-Section "AGENTS.md Audit"

    foreach ($rule in $rules)
    {
        $matches = Select-String -Path $TrackedCodeFiles -Pattern $rule.Pattern
        if ($null -eq $matches -or $matches.Count -eq 0)
        {
            continue
        }

        $color = if ($rule.Severity -eq "Error") { "Red" } else { "Yellow" }
        Write-Host ("[{0}] {1}" -f $rule.Severity, $rule.Name) -ForegroundColor $color
        $reportLines.Add(("[{0}] {1}" -f $rule.Severity, $rule.Name))

        foreach ($match in $matches)
        {
            $location = Format-MatchLocation -RepoRoot $RepoRoot -Match $match
            Write-Host ("  " + $location)
            $reportLines.Add(("  " + $location))
            $findings.Add([pscustomobject]@{
                    Rule = $rule.Name
                    Severity = $rule.Severity
                    Location = $location
                })
        }

        if ($rule.Severity -eq "Error")
        {
            $errorCount += $matches.Count
        }
        else
        {
            $warningCount += $matches.Count
        }
    }

    if ($errorCount -eq 0 -and $warningCount -eq 0)
    {
        Write-Host "No AGENTS.md pattern hit found in tracked C++ files." -ForegroundColor Green
        $reportLines.Add("No AGENTS.md pattern hit found in tracked C++ files.")
    }

    Set-Content -LiteralPath $AuditLogPath -Value $reportLines -Encoding UTF8

    return [pscustomobject]@{
        Errors = $errorCount
        Warnings = $warningCount
        Findings = [object[]]$findings
    }
}

function Invoke-ClangTidy
{
    param(
        [string]$RepoRoot,
        [string]$BuildDir,
        [string[]]$SourceFiles,
        [string[]]$Paths,
        [bool]$ApplyFixes,
        [string]$ReportDir
    )

    $clangTidyPath = Find-ClangTidyPath
    $logPath = Join-Path $ReportDir "clang-tidy.log"
    $fixesDir = Join-Path $ReportDir "fixes"
    Ensure-Directory -Path $fixesDir

    Write-Section "clang-tidy"
    Write-Host ("Using clang-tidy: {0}" -f $clangTidyPath)

    $headerFilter = Build-HeaderFilter -Filters $Paths
    Write-KeyValue -Name "header filter" -Value $headerFilter
    $warningCount = 0
    $errorCount = 0
    $failedFiles = 0
    $logLines = New-Object System.Collections.Generic.List[string]
    $fileResults = New-Object System.Collections.Generic.List[object]

    for ($index = 0; $index -lt $SourceFiles.Count; ++$index)
    {
        $relativeSource = Normalize-RepoPath $SourceFiles[$index]
        $absoluteSource = Join-Path $RepoRoot $relativeSource
        Write-Progress -Activity "Running clang-tidy" -Status $relativeSource -PercentComplete ((($index + 1) * 100.0) / $SourceFiles.Count)

        $arguments = @(
            $clangTidyPath,
            "-p", $BuildDir,
            "--quiet",
            "--use-color=false",
            "--header-filter=$headerFilter"
        )

        $fixFile = Join-Path $fixesDir ((Convert-ToSafeFileName $relativeSource) + ".yaml")
        if ($ApplyFixes)
        {
            $arguments += "-fix"
        }

        $arguments += "--export-fixes=$fixFile"
        $arguments += $absoluteSource

        $result = Invoke-VcCommand -Architecture $Architecture -Arguments $arguments
        if ($index -eq 0)
        {
            Write-CommandLine -Label "clang-tidy command" -CommandLine $result.CommandLine
        }
        $hasWarning = @($result.Output | Select-String -Pattern '\bwarning:').Count
        $hasError = @($result.Output | Select-String -Pattern '\berror:').Count

        $warningCount += $hasWarning
        $errorCount += $hasError

        if ((-not (Test-Path $fixFile)) -or ((Get-Item $fixFile).Length -eq 0))
        {
            Remove-Item -LiteralPath $fixFile -ErrorAction SilentlyContinue
        }

        if ($result.ExitCode -ne 0 -or $hasWarning -gt 0 -or $hasError -gt 0)
        {
            ++$failedFiles
            Write-Host ("[{0}] diagnostics in {1}" -f $failedFiles, $relativeSource) -ForegroundColor Yellow
            $logLines.Add(("## {0}" -f $relativeSource))
            foreach ($line in $result.Output)
            {
                Write-Host $line
                $logLines.Add($line)
            }
            $logLines.Add("")
        }

        $fileResults.Add([pscustomobject]@{
                File = $relativeSource
                ExitCode = $result.ExitCode
                WarningCount = $hasWarning
                ErrorCount = $hasError
                FixFile = if (Test-Path $fixFile) { Normalize-RepoPath($fixFile.Substring($RepoRoot.Length + 1)) } else { "" }
            })
    }

    Write-Progress -Activity "Running clang-tidy" -Completed
    Set-Content -LiteralPath $logPath -Value $logLines -Encoding UTF8

    return [pscustomobject]@{
        WarningCount = $warningCount
        ErrorCount = $errorCount
        FailedFiles = $failedFiles
        FileResults = [object[]]$fileResults
        LogPath = $logPath
    }
}

$repoRoot = Get-RepoRoot

if ([string]::IsNullOrWhiteSpace($BuildDir))
{
    $analysisSuffix = if ($Architecture -eq "x86") { "win32" } else { "x64" }
    $BuildDir = Join-Path $repoRoot ("build/clang-tidy-" + $analysisSuffix)
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
    $ReportDir = Join-Path $repoRoot "outputs/quality/clang-tidy/latest"
}
else
{
    $ReportDir = Resolve-OptionalPath -CandidatePath $ReportDir -RepoRoot $repoRoot
}

Reset-Directory -Path $ReportDir

Write-Section "Run Clang-Tidy"
Write-KeyValue -Name "repo root" -Value $repoRoot
Write-KeyValue -Name "build dir" -Value $BuildDir
Write-KeyValue -Name "reference build dir" -Value $ReferenceBuildDir
Write-KeyValue -Name "report dir" -Value $ReportDir
Write-KeyValue -Name "architecture" -Value $Architecture
Write-KeyValue -Name "configuration" -Value $Configuration
Write-KeyValue -Name "audit only" -Value $AuditOnly.IsPresent
Write-KeyValue -Name "apply fixes" -Value $Fix.IsPresent
Write-StringList -Name "path filters" -Values $Paths

$trackedCodeFiles = Get-TrackedFiles -RepoRoot $repoRoot -Patterns @("*.cpp", "*.h", "*.hpp")
$filteredCodeFiles = Get-FilteredTrackedFiles -TrackedFiles $trackedCodeFiles -Filters $Paths
if ($filteredCodeFiles.Count -eq 0)
{
    throw "No tracked C++ files matched the requested Paths filter."
}
Write-KeyValue -Name "tracked code files" -Value $filteredCodeFiles.Count

$auditResult = Invoke-PatternAudit `
    -RepoRoot $repoRoot `
    -TrackedCodeFiles ($filteredCodeFiles | ForEach-Object { Join-Path $repoRoot $_ }) `
    -AuditLogPath (Join-Path $ReportDir "agents-audit.log")

if ($AuditOnly)
{
    $summary = [pscustomobject]@{
        Tool = "clang-tidy-audit"
        Paths = $Paths
        AuditErrors = $auditResult.Errors
        AuditWarnings = $auditResult.Warnings
        AuditFindings = $auditResult.Findings
        Success = ($auditResult.Errors -eq 0)
    }

    Write-JsonFile -Path (Join-Path $ReportDir "summary.json") -Value $summary
    Set-Content -LiteralPath (Join-Path $ReportDir "summary.md") -Encoding UTF8 -Value @(
        "# clang-tidy audit summary"
        ""
        "- AGENTS.md hard failures: $($auditResult.Errors)"
        "- AGENTS.md review hits: $($auditResult.Warnings)"
        "- Success: $($auditResult.Errors -eq 0)"
    )

    if ($auditResult.Errors -gt 0)
    {
        exit 1
    }

    exit 0
}

$trackedSourceFiles = Get-TrackedFiles -RepoRoot $repoRoot -Patterns @("*.cpp", "*.cc", "*.cxx")
$filteredSourceFiles = Get-FilteredTrackedFiles -TrackedFiles $trackedSourceFiles -Filters $Paths
if ($filteredSourceFiles.Count -eq 0)
{
    throw "No tracked C++ source file matched the requested Paths filter."
}
Write-KeyValue -Name "tracked sources" -Value $filteredSourceFiles.Count

$compileCommands = Ensure-CompilationDatabase `
    -RepoRoot $repoRoot `
    -BuildDir $BuildDir `
    -Architecture $Architecture `
    -Configuration $Configuration `
    -ReferenceBuildDir $ReferenceBuildDir `
    -Reconfigure $Reconfigure.IsPresent
Write-KeyValue -Name "compile_commands" -Value $compileCommands

$clangTidyResult = Invoke-ClangTidy `
    -RepoRoot $repoRoot `
    -BuildDir $BuildDir `
    -SourceFiles $filteredSourceFiles `
    -Paths $Paths `
    -ApplyFixes $Fix.IsPresent `
    -ReportDir $ReportDir

$success = ($auditResult.Errors -eq 0 -and $clangTidyResult.WarningCount -eq 0 -and $clangTidyResult.ErrorCount -eq 0)
$summary = [pscustomobject]@{
    Tool = "clang-tidy"
    BuildDir = Normalize-RepoPath $BuildDir
    ReportDir = Normalize-RepoPath $ReportDir
    Paths = $Paths
    AuditErrors = $auditResult.Errors
    AuditWarnings = $auditResult.Warnings
    ClangTidyWarnings = $clangTidyResult.WarningCount
    ClangTidyErrors = $clangTidyResult.ErrorCount
    FilesWithDiagnostics = $clangTidyResult.FailedFiles
    Files = $clangTidyResult.FileResults
    Success = $success
}

Write-JsonFile -Path (Join-Path $ReportDir "summary.json") -Value $summary
Set-Content -LiteralPath (Join-Path $ReportDir "summary.md") -Encoding UTF8 -Value @(
    "# clang-tidy summary"
    ""
    "- Build directory: ``$BuildDir``"
    "- AGENTS.md hard failures: $($auditResult.Errors)"
    "- AGENTS.md review hits: $($auditResult.Warnings)"
    "- clang-tidy warnings: $($clangTidyResult.WarningCount)"
    "- clang-tidy errors: $($clangTidyResult.ErrorCount)"
    "- files with diagnostics: $($clangTidyResult.FailedFiles)"
    "- success: $success"
)

Write-Section "Summary"
Write-Host ("AGENTS.md hard failures : {0}" -f $auditResult.Errors)
Write-Host ("AGENTS.md review hits   : {0}" -f $auditResult.Warnings)
Write-Host ("clang-tidy warnings     : {0}" -f $clangTidyResult.WarningCount)
Write-Host ("clang-tidy errors       : {0}" -f $clangTidyResult.ErrorCount)
Write-Host ("files with diagnostics  : {0}" -f $clangTidyResult.FailedFiles)
Write-Host ("report directory        : {0}" -f $ReportDir)
Write-KeyValue -Name "audit log" -Value (Join-Path $ReportDir "agents-audit.log")
Write-KeyValue -Name "clang-tidy log" -Value $clangTidyResult.LogPath

if (-not $success)
{
    exit 1
}

exit 0
