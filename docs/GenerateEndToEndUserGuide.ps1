param(
    [string]$SourcePath = "docs/user_guide/mfdstudio_end_to_end_user_guide.md",
    [string]$OutputPath = "docs/user_guide/MFDStudio_End_To_End_User_Guide.docx",
    [string]$RenderedDiagramDirectory = "docs/user_guide/rendered",
    [switch]$SkipDiagramRendering,
    [switch]$SkipPdfExport
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$builderScript = Join-Path $repositoryRoot "docs/user_guide/build_user_guide.py"

if (-not (Test-Path -LiteralPath $builderScript)) {
    throw "Python builder not found: $builderScript"
}

function Get-JavaMajorVersion {
    param(
        [Parameter(Mandatory = $true)]
        [string]$JavaExecutable
    )

    try {
        $firstLine = (& $JavaExecutable -version 2>&1 | Select-Object -First 1).ToString()
    }
    catch {
        return 0
    }

    if ($firstLine -match '"([^"]+)"') {
        $version = $matches[1]
        if ($version -match '^1\.(\d+)') {
            return [int]$matches[1]
        }
        if ($version -match '^(\d+)') {
            return [int]$matches[1]
        }
    }

    return 0
}

function Resolve-JavaExecutable {
    $candidates = New-Object System.Collections.Generic.List[string]

    $preferredOpenJdk = 'C:\Program Files\ojdkbuild\java-17-openjdk-17.0.3.0.6-1\bin\java.exe'
    if (Test-Path -LiteralPath $preferredOpenJdk) {
        return $preferredOpenJdk
    }

    foreach ($candidate in (& where.exe java 2>$null)) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and -not $candidates.Contains($candidate)) {
            [void]$candidates.Add($candidate)
        }
    }

    $selected = $null
    $selectedMajor = 0
    foreach ($candidate in $candidates) {
        $major = Get-JavaMajorVersion -JavaExecutable $candidate
        if ($major -ge 11 -and $major -gt $selectedMajor) {
            $selected = $candidate
            $selectedMajor = $major
        }
    }

    if ($null -eq $selected) {
        throw "A Java 11+ runtime is required to render the PlantUML diagrams locally."
    }

    return $selected
}

function Render-GuideDiagrams {
    param(
        [string]$RepositoryRoot,
        [string]$RenderedDiagramDirectory
    )

    $diagramDirectory = Join-Path $RepositoryRoot "docs/user_guide/diagrams"
    if (-not (Test-Path -LiteralPath $RenderedDiagramDirectory)) {
        New-Item -ItemType Directory -Path $RenderedDiagramDirectory | Out-Null
    }

    $plantUmlJar = Join-Path $env:TEMP "plantuml.jar"
    if (-not (Test-Path -LiteralPath $plantUmlJar)) {
        Invoke-WebRequest `
            -UseBasicParsing `
            -Uri "https://github.com/plantuml/plantuml/releases/latest/download/plantuml.jar" `
            -OutFile $plantUmlJar
    }

    $javaExecutable = Resolve-JavaExecutable
    $dotExecutable = (Get-Command dot -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1)
    if ($dotExecutable) {
        $env:GRAPHVIZ_DOT = $dotExecutable
    }

    $diagramFiles = Get-ChildItem -LiteralPath $diagramDirectory -Filter *.puml -File | Sort-Object Name
    foreach ($diagramFile in $diagramFiles) {
        & $javaExecutable -jar $plantUmlJar -charset UTF-8 -tpng -output $RenderedDiagramDirectory $diagramFile.FullName
        if ($LASTEXITCODE -ne 0) {
            throw "PlantUML rendering failed for $($diagramFile.FullName)."
        }
    }
}

$absoluteRenderedDiagramDirectory = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot $RenderedDiagramDirectory))
if (-not $SkipDiagramRendering) {
    Render-GuideDiagrams -RepositoryRoot $repositoryRoot -RenderedDiagramDirectory $absoluteRenderedDiagramDirectory
}

$pythonArgs = @(
    $builderScript,
    "--source", $SourcePath,
    "--output", $OutputPath,
    "--rendered-diagram-dir", $RenderedDiagramDirectory,
    "--skip-diagram-rendering"
)

& py -3 @pythonArgs
if ($LASTEXITCODE -ne 0) {
    throw "The Python document builder failed."
}

$absoluteOutputPath = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot $OutputPath))
$absolutePdfPath = [System.IO.Path]::ChangeExtension($absoluteOutputPath, ".pdf")

$word = $null
$document = $null
try {
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $document = $word.Documents.Open($absoluteOutputPath)
    $document.Fields.Update() | Out-Null
    if ($document.TablesOfContents.Count -gt 0) {
        $document.TablesOfContents.Item(1).Update() | Out-Null
    }
    $document.Save()

    if (-not $SkipPdfExport) {
        $document.ExportAsFixedFormat($absolutePdfPath, 17)
    }
}
finally {
    if ($null -ne $document) {
        $document.Close([ref]0)
        [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($document)
    }
    if ($null -ne $word) {
        $word.Quit()
        [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($word)
    }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}
