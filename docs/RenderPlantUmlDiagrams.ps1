param(
    [string]$InputDirectory = "docs/images",
    [string]$ServerUrl = "https://kroki.io/plantuml/svg"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $repoRoot

$sourceDirectory = Resolve-Path (Join-Path $repoRoot $InputDirectory)
$diagramFiles = Get-ChildItem -Path $sourceDirectory -Filter *.puml -File | Sort-Object Name

if ($diagramFiles.Count -eq 0)
{
    throw "No PlantUML source files were found in '$sourceDirectory'."
}

foreach ($diagramFile in $diagramFiles)
{
    $outputFile = [System.IO.Path]::ChangeExtension($diagramFile.FullName, ".svg")
    $diagramText = Get-Content -Path $diagramFile.FullName -Raw

    Invoke-WebRequest `
        -UseBasicParsing `
        -Method Post `
        -Uri $ServerUrl `
        -ContentType "text/plain" `
        -Body $diagramText `
        -OutFile $outputFile

    Write-Host ("Rendered {0}" -f (Resolve-Path -Relative $outputFile))
}
