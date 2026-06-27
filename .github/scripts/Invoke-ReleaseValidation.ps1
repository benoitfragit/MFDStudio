[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SummaryMarkdownPath,
    [Parameter(Mandatory = $true)]
    [string]$SummaryJsonPath
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$validationMatrix = @(
    [pscustomobject]@{
        Label = "Debug Win32"
        ConfigurePreset = "vs2022-win32"
        BuildPreset = "debug-win32"
        TestPreset = "test-debug-win32"
        Scope = "Configure, build, automated tests"
    },
    [pscustomobject]@{
        Label = "Release Win32"
        ConfigurePreset = "vs2022-win32"
        BuildPreset = "release-win32"
        TestPreset = $null
        Scope = "Configure and build"
    }
)

function Assert-LastExitCode {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

function Get-TestCount {
    param(
        [Parameter(Mandatory = $true)]
        [string]$TestPreset
    )

    $jsonText = (& ctest --preset $TestPreset --show-only=json-v1 | Out-String)
    Assert-LastExitCode "CTest discovery for preset '$TestPreset'"

    $payload = $jsonText | ConvertFrom-Json
    if ($null -eq $payload.tests) {
        return 0
    }

    return @($payload.tests).Count
}

function Ensure-ParentDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $parentDirectory = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parentDirectory)) {
        New-Item -ItemType Directory -Force -Path $parentDirectory | Out-Null
    }
}

$validationResults = New-Object System.Collections.Generic.List[object]
$overallSucceeded = $true
$failureMessage = $null
$failedIndex = $null

for ($index = 0; $index -lt $validationMatrix.Count; $index++) {
    $validation = $validationMatrix[$index]
    $result = [ordered]@{
        Label = $validation.Label
        Status = "PASS"
        Scope = $validation.Scope
        Details = "Completed."
        TestCount = 0
    }

    Write-Host "::group::$($validation.Label)"
    try {
        & cmake --preset $validation.ConfigurePreset
        Assert-LastExitCode "Configure preset '$($validation.ConfigurePreset)'"

        & cmake --build --preset $validation.BuildPreset
        Assert-LastExitCode "Build preset '$($validation.BuildPreset)'"

        if (-not [string]::IsNullOrWhiteSpace($validation.TestPreset)) {
            $testCount = Get-TestCount -TestPreset $validation.TestPreset
            & ctest --preset $validation.TestPreset --output-on-failure
            Assert-LastExitCode "CTest preset '$($validation.TestPreset)'"

            $result.TestCount = $testCount
            $result.Details = "$testCount tests passed."
        }
        else {
            $result.Details = "Build-only validation passed."
        }
    }
    catch {
        $result.Status = "FAIL"
        $result.Details = $_.Exception.Message
        $overallSucceeded = $false
        $failureMessage = $_.Exception.Message
        $failedIndex = $index
    }
    finally {
        Write-Host "::endgroup::"
    }

    $validationResults.Add([pscustomobject]$result)

    if (-not $overallSucceeded) {
        break
    }
}

if ($null -ne $failedIndex) {
    for ($index = $failedIndex + 1; $index -lt $validationMatrix.Count; $index++) {
        $validation = $validationMatrix[$index]
        $validationResults.Add(
            [pscustomobject]@{
                Label = $validation.Label
                Status = "SKIPPED"
                Scope = $validation.Scope
                Details = "Not reached after an earlier validation failure."
                TestCount = 0
            })
    }
}

$testSuiteCount = @($validationMatrix | Where-Object { -not [string]::IsNullOrWhiteSpace($_.TestPreset) }).Count
$totalTestCases = ($validationResults | Measure-Object -Property TestCount -Sum).Sum
if ($null -eq $totalTestCases) {
    $totalTestCases = 0
}

$summaryLines = @(
    "## Release validation",
    "",
    "| Target | Status | Scope | Details |",
    "| --- | --- | --- | --- |"
)

foreach ($result in $validationResults) {
    $summaryLines += "| $($result.Label) | $($result.Status) | $($result.Scope) | $($result.Details) |"
}

$summaryLines += ""
$summaryLines += "- Build targets in scope: $($validationMatrix.Count)"
$summaryLines += "- Automated test suites in scope: $testSuiteCount"
$summaryLines += "- Automated test cases discovered: $totalTestCases"

if (-not $overallSucceeded -and -not [string]::IsNullOrWhiteSpace($failureMessage)) {
    $summaryLines += "- Failure: $failureMessage"
}

$summaryPayload = [pscustomobject]@{
    OverallStatus = $(if ($overallSucceeded) { "passed" } else { "failed" })
    BuildCount = $validationMatrix.Count
    TestSuiteCount = $testSuiteCount
    TestCaseCount = [int]$totalTestCases
    Results = $validationResults
}

Ensure-ParentDirectory -Path $SummaryMarkdownPath
Ensure-ParentDirectory -Path $SummaryJsonPath

$summaryLines | Set-Content -Path $SummaryMarkdownPath -Encoding utf8
$summaryPayload | ConvertTo-Json -Depth 5 | Set-Content -Path $SummaryJsonPath -Encoding utf8

if (-not $overallSucceeded) {
    throw $failureMessage
}
