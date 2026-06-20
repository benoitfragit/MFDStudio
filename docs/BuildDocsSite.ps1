param(
    [string]$Version = "local",
    [string]$PlantUmlJarPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $repoRoot

$siteDirectory = Join-Path $repoRoot "build/docs/site"
$doxygenHtmlDirectory = Join-Path $repoRoot "build/docs/doxygen/html"
$apiDirectory = Join-Path $siteDirectory "api"
$bookDirectory = Join-Path $repoRoot "docs"

function Assert-Command {
    param(
        [Parameter(Mandatory = $true)] [string]$Name,
        [Parameter(Mandatory = $true)] [string]$Hint
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) {
        throw "Required tool '$Name' was not found on PATH. $Hint"
    }

    return $command.Source
}

# 1. Clean the published site directory.
if (Test-Path $siteDirectory) {
    Remove-Item -Recurse -Force $siteDirectory
}
New-Item -ItemType Directory -Force $siteDirectory | Out-Null

# 2. Verify mdBook is available.
Assert-Command -Name "mdbook" -Hint "Install it from https://github.com/rust-lang/mdBook/releases or with 'cargo install mdbook'." | Out-Null

# 3. Generate the mdBook portal directly into build/docs/site.
Write-Host ("Building mdBook portal into {0}" -f $siteDirectory)
& mdbook build $bookDirectory --dest-dir $siteDirectory
if ($LASTEXITCODE -ne 0) {
    throw ("mdBook build failed with exit code {0}" -f $LASTEXITCODE)
}

# 4. Generate the Doxygen C++ API reference.
#    GenerateDocs.ps1 validates Doxygen, Java, and PlantUML and emits clear errors.
Write-Host "Generating Doxygen C++ API reference"
& pwsh -File (Join-Path $bookDirectory "GenerateDocs.ps1") -Version $Version -PlantUmlJarPath $PlantUmlJarPath
if ($LASTEXITCODE -ne 0) {
    throw ("Doxygen generation failed with exit code {0}" -f $LASTEXITCODE)
}

if (-not (Test-Path (Join-Path $doxygenHtmlDirectory "index.html"))) {
    throw "Doxygen HTML output was not produced at $doxygenHtmlDirectory/index.html."
}

# 5. Copy the Doxygen HTML into the published site under /api.
Write-Host ("Copying Doxygen HTML into {0}" -f $apiDirectory)
New-Item -ItemType Directory -Force $apiDirectory | Out-Null
Copy-Item -Path (Join-Path $doxygenHtmlDirectory "*") -Destination $apiDirectory -Recurse -Force

# 6. Disable Jekyll processing on GitHub Pages.
New-Item -ItemType File -Force (Join-Path $siteDirectory ".nojekyll") | Out-Null

# 7. Verify the expected published outputs exist.
$expectedOutputs = @(
    (Join-Path $siteDirectory "index.html"),
    (Join-Path $apiDirectory "index.html")
)

foreach ($expectedOutput in $expectedOutputs) {
    if (-not (Test-Path $expectedOutput)) {
        throw "Expected documentation output missing: $expectedOutput"
    }
}

Write-Host ""
Write-Host ("Documentation site ready: {0}" -f $siteDirectory)
Write-Host ("  main portal: {0}" -f (Join-Path $siteDirectory "index.html"))
Write-Host ("  C++ API:     {0}" -f (Join-Path $apiDirectory "index.html"))
