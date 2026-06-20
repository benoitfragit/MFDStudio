<#
.SYNOPSIS
    Local documentation quality checks for the mdBook guide portal.

.DESCRIPTION
    Runs fast, dependency-light checks over docs/src and the SUMMARY table of
    contents, then (optionally) builds the mdBook portal to confirm it compiles.
    Intended as a pre-commit / CI-friendly gate that needs only mdBook on PATH.

    Checks performed:
      1. broken internal Markdown links (links to files that do not exist)
      2. missing local images
      3. absolute local filesystem paths leaking into the docs
      4. pages under docs/src that are not referenced by SUMMARY.md (orphans)
      5. mdBook build (skipped with -SkipBuild or when mdbook is absent)

    Doxygen "./api/..." links are ignored: they resolve only on the combined
    site produced by BuildDocsSite.ps1.

.PARAMETER SkipBuild
    Skip the mdBook build step and run the static checks only.
#>
param(
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$srcDirectory = Join-Path $repoRoot "docs/src"
$summaryPath = Join-Path $srcDirectory "SUMMARY.md"

$problems = New-Object System.Collections.Generic.List[string]

function Add-Problem {
    param([string]$Message)
    $problems.Add($Message)
}

$markdownFiles = Get-ChildItem -Path $srcDirectory -Recurse -Filter *.md

# --- 1 & 2: internal links and images ---------------------------------------
$linkPattern = [regex]'(!?)\[[^\]]*\]\(([^)]+)\)'
foreach ($file in $markdownFiles) {
    $text = Get-Content -LiteralPath $file.FullName -Raw
    foreach ($match in $linkPattern.Matches($text)) {
        $isImage = $match.Groups[1].Value -eq "!"
        $target = $match.Groups[2].Value.Trim().Split(" ")[0]

        if ($target -match '^(https?:|mailto:|#)') { continue }
        if (-not $isImage -and $target -match '(^\./api/|/api/)') { continue }

        $relative = $target.Split("#")[0]
        if ([string]::IsNullOrWhiteSpace($relative)) { continue }

        $resolved = Join-Path $file.DirectoryName $relative
        if (-not (Test-Path -LiteralPath $resolved)) {
            $kind = if ($isImage) { "missing image" } else { "broken link" }
            Add-Problem ("{0}: {1} -> {2}" -f $file.FullName.Substring($repoRoot.Length + 1), $kind, $target)
        }
    }
}

# --- 3: absolute local paths ------------------------------------------------
$absolutePathPattern = [regex]'([A-Za-z]:\\|/home/|/Users/|/mnt/)'
foreach ($file in $markdownFiles) {
    $lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $file.FullName) {
        $lineNumber++
        if ($absolutePathPattern.IsMatch($line)) {
            Add-Problem ("{0}:{1}: absolute local path: {2}" -f $file.FullName.Substring($repoRoot.Length + 1), $lineNumber, $line.Trim())
        }
    }
}

# --- 4: orphan pages (not referenced by SUMMARY.md) -------------------------
$summaryText = Get-Content -LiteralPath $summaryPath -Raw
$referenced = New-Object System.Collections.Generic.HashSet[string]
foreach ($match in ([regex]'\]\((\./[^)]+\.md)\)').Matches($summaryText)) {
    $resolved = (Resolve-Path -LiteralPath (Join-Path $srcDirectory $match.Groups[1].Value)).Path
    [void]$referenced.Add($resolved)
}
foreach ($file in $markdownFiles) {
    if ($file.Name -eq "SUMMARY.md") { continue }
    if (-not $referenced.Contains($file.FullName)) {
        Add-Problem ("{0}: page is not referenced by SUMMARY.md (orphan)" -f $file.FullName.Substring($repoRoot.Length + 1))
    }
}

# --- 5: mdBook build --------------------------------------------------------
if (-not $SkipBuild) {
    $mdbook = Get-Command mdbook -ErrorAction SilentlyContinue
    if ($null -eq $mdbook) {
        Write-Host "mdbook not found on PATH; skipping the build check." -ForegroundColor Yellow
    }
    else {
        $tempOut = Join-Path ([System.IO.Path]::GetTempPath()) ("mfd-docs-check-" + [System.Guid]::NewGuid().ToString("N"))
        & mdbook build (Join-Path $repoRoot "docs") --dest-dir $tempOut | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Add-Problem "mdbook build failed."
        }
        Remove-Item -Recurse -Force $tempOut -ErrorAction SilentlyContinue
    }
}

# --- report -----------------------------------------------------------------
if ($problems.Count -gt 0) {
    Write-Host ("Documentation checks found {0} problem(s):" -f $problems.Count) -ForegroundColor Red
    foreach ($problem in $problems) {
        Write-Host ("  - {0}" -f $problem)
    }
    exit 1
}

Write-Host "Documentation checks passed." -ForegroundColor Green
exit 0
