param(
    [string]$Version = "release",
    [string]$PlantUmlJarPath = ""
)

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $repoRoot

function Find-PlantUmlJar {
    param([string]$RequestedPath)

    $candidatePaths = [System.Collections.Generic.List[string]]::new()
    if (-not [string]::IsNullOrWhiteSpace($RequestedPath)) {
        $candidatePaths.Add($RequestedPath)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:MFD_DOCS_PLANTUML_JAR)) {
        $candidatePaths.Add($env:MFD_DOCS_PLANTUML_JAR)
    }

    $commonCandidates = @(
        (Join-Path $repoRoot "build/tools/plantuml/plantuml.jar"),
        (Join-Path $env:ProgramData "chocolatey/lib/plantuml/tools/plantuml.jar"),
        (Join-Path $env:ProgramFiles "PlantUML/plantuml.jar")
    )

    foreach ($candidate in $commonCandidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate)) {
            $candidatePaths.Add($candidate)
        }
    }

    foreach ($candidatePath in $candidatePaths) {
        if (-not [string]::IsNullOrWhiteSpace($candidatePath) -and (Test-Path $candidatePath)) {
            return (Resolve-Path $candidatePath).Path
        }
    }

    $searchRoots = @()
    if (-not [string]::IsNullOrWhiteSpace($env:ProgramData)) {
        $searchRoots += (Join-Path $env:ProgramData "chocolatey/lib")
    }
    if (-not [string]::IsNullOrWhiteSpace($env:ProgramFiles)) {
        $searchRoots += $env:ProgramFiles
    }

    foreach ($searchRoot in $searchRoots) {
        if (-not (Test-Path $searchRoot)) {
            continue
        }

        $jar = Get-ChildItem -Path $searchRoot -Filter plantuml.jar -Recurse -File -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
        if (-not [string]::IsNullOrWhiteSpace($jar)) {
            return (Resolve-Path $jar).Path
        }
    }

    return $null
}

function Get-JavaVersionInfo {
    param([string]$JavaExecutable)

    if ([string]::IsNullOrWhiteSpace($JavaExecutable) -or -not (Test-Path $JavaExecutable)) {
        return $null
    }

    $resolvedPath = (Resolve-Path $JavaExecutable).Path
    $javaVersionOutput = & $resolvedPath -version 2>&1 | Select-Object -First 1
    $javaVersionText = [string]$javaVersionOutput
    if ([string]::IsNullOrWhiteSpace($javaVersionText)) {
        return $null
    }

    $javaMajorVersion = $null
    if ($javaVersionText -match '"([^"]+)"') {
        $javaVersionToken = $matches[1]
        if ($javaVersionToken.StartsWith("1.")) {
            $javaMajorVersion = [int]$javaVersionToken.Split(".")[1]
        }
        else {
            $javaMajorVersion = [int]$javaVersionToken.Split(".")[0]
        }
    }

    [pscustomobject]@{
        Path = $resolvedPath
        VersionText = $javaVersionText
        MajorVersion = $javaMajorVersion
    }
}

function Find-JavaExecutable {
    $candidatePaths = [System.Collections.Generic.List[string]]::new()
    $seenPaths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

    function Add-JavaCandidate {
        param([string]$CandidatePath)

        if ([string]::IsNullOrWhiteSpace($CandidatePath) -or -not (Test-Path $CandidatePath)) {
            return
        }

        $resolvedCandidate = (Resolve-Path $CandidatePath).Path
        if ($seenPaths.Add($resolvedCandidate)) {
            $candidatePaths.Add($resolvedCandidate)
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($env:JAVA_HOME)) {
        Add-JavaCandidate (Join-Path $env:JAVA_HOME "bin/java.exe")
    }

    $javaCommand = Get-Command java -ErrorAction SilentlyContinue
    if ($null -ne $javaCommand) {
        Add-JavaCandidate $javaCommand.Source
    }

    $searchRoots = @(
        (Join-Path $env:ProgramFiles "Java"),
        (Join-Path $env:ProgramFiles "ojdkbuild"),
        (Join-Path $env:ProgramFiles "Eclipse Adoptium"),
        (Join-Path $env:ProgramFiles "Zulu"),
        (Join-Path $env:ProgramFiles "Microsoft"),
        (Join-Path $env:LOCALAPPDATA "Programs")
    )

    foreach ($searchRoot in $searchRoots) {
        if ([string]::IsNullOrWhiteSpace($searchRoot) -or -not (Test-Path $searchRoot)) {
            continue
        }

        Get-ChildItem -Path $searchRoot -Filter java.exe -Recurse -File -ErrorAction SilentlyContinue |
            ForEach-Object { Add-JavaCandidate $_.FullName }
    }

    foreach ($candidatePath in $candidatePaths) {
        $versionInfo = Get-JavaVersionInfo -JavaExecutable $candidatePath
        if ($null -ne $versionInfo -and $null -ne $versionInfo.MajorVersion -and $versionInfo.MajorVersion -ge 11) {
            return $versionInfo
        }
    }

    return $null
}

$null = Get-Command doxygen -ErrorAction Stop

$resolvedJava = Find-JavaExecutable
if ($null -eq $resolvedJava) {
    throw "PlantUML generation requires Java 11 or newer. Set JAVA_HOME, update PATH, or install a newer JDK."
}

$resolvedPlantUmlJar = Find-PlantUmlJar -RequestedPath $PlantUmlJarPath
if (-not $resolvedPlantUmlJar) {
    throw "PlantUML jar not found. Install PlantUML or pass -PlantUmlJarPath."
}

$javaBinDirectory = Split-Path -Parent $resolvedJava.Path
$javaHome = Split-Path -Parent $javaBinDirectory
$env:JAVA_HOME = $javaHome
if (-not ($env:Path -split ';' | Where-Object { $_ -eq $javaBinDirectory })) {
    $env:Path = $javaBinDirectory + ';' + $env:Path
}

$doxyfileTemplatePath = Join-Path $repoRoot "docs/Doxyfile"
$generatedConfigPath = Join-Path $repoRoot "build/docs/MFDStudio.generated.Doxyfile"

New-Item -ItemType Directory -Force (Split-Path $generatedConfigPath -Parent) | Out-Null
New-Item -ItemType Directory -Force "build/docs/doxygen" | Out-Null

$doxyfileText = Get-Content -Path $doxyfileTemplatePath -Raw
$projectNumberReplacement = 'PROJECT_NUMBER         = "' + $Version + '"'
$plantUmlReplacement = 'PLANTUML_JAR_PATH      = "' + ($resolvedPlantUmlJar -replace '\\', '/') + '"'
$doxyfileText = $doxyfileText -replace 'PROJECT_NUMBER\s*=.*', $projectNumberReplacement
$doxyfileText = $doxyfileText -replace 'PLANTUML_JAR_PATH\s*=.*', $plantUmlReplacement
[System.IO.File]::WriteAllText(
    $generatedConfigPath,
    $doxyfileText,
    (New-Object System.Text.UTF8Encoding($false)))

Write-Host ("Generating Doxygen HTML for version {0}" -f $Version)
Write-Host ("Using Java: {0} ({1})" -f $resolvedJava.Path, $resolvedJava.VersionText)
Write-Host ("Using PlantUML jar: {0}" -f $resolvedPlantUmlJar)

doxygen $generatedConfigPath
if ($LASTEXITCODE -ne 0) {
    throw ("Doxygen generation failed with exit code {0}" -f $LASTEXITCODE)
}
