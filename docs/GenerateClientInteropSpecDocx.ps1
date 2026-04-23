param(
    [string]$SourcePath = "docs/standards/mfd_client_interoperability_specification.md",
    [string]$OutputPath = "docs/standards/MFDStudio_External_Client_Interoperability_Specification.docx"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Split-MarkdownTableRow {
    param([string]$Line)

    $trimmed = $Line.Trim()
    if ($trimmed.StartsWith("|")) {
        $trimmed = $trimmed.Substring(1)
    }
    if ($trimmed.EndsWith("|")) {
        $trimmed = $trimmed.Substring(0, $trimmed.Length - 1)
    }

    return ($trimmed -split "\|") | ForEach-Object { $_.Trim() }
}

function Add-ParagraphText {
    param(
        $Selection,
        [string]$Text,
        [int]$StyleId = -1
    )

    if ([string]::IsNullOrWhiteSpace($Text)) {
        $Selection.TypeParagraph()
        return
    }

    $Selection.Style = $StyleId
    $Selection.TypeText($Text)
    $Selection.TypeParagraph()
}

function Add-CodeBlock {
    param(
        $Document,
        $Selection,
        [string[]]$Lines
    )

    if ($Lines.Count -eq 0) {
        return
    }

    $text = ($Lines -join "`r")
    $start = $Selection.Start
    $Selection.Style = -1
    $Selection.TypeText($text)
    $end = $Selection.End

    $range = $Document.Range($start, $end)
    $range.Font.Name = "Consolas"
    $range.Font.Size = 9
    $range.ParagraphFormat.LeftIndent = 18
    $range.ParagraphFormat.SpaceAfter = 6

    $Selection.SetRange($end, $end)
    $Selection.TypeParagraph()
    $Selection.TypeParagraph()
}

function Add-TableFromMarkdown {
    param(
        $Document,
        $Selection,
        [string[]]$Lines
    )

    if ($Lines.Count -lt 2) {
        foreach ($line in $Lines) {
            Add-ParagraphText -Selection $Selection -Text $line
        }
        return
    }

    $header = Split-MarkdownTableRow -Line $Lines[0]
    $dataLines = @()
    if ($Lines.Count -gt 2) {
        $dataLines = $Lines[2..($Lines.Count - 1)]
    }

    $rowCount = 1 + $dataLines.Count
    $columnCount = $header.Count
    $table = $Document.Tables.Add($Selection.Range, $rowCount, $columnCount)
    $table.Borders.Enable = 1
    $table.Rows.Item(1).Range.Bold = $true

    for ($columnIndex = 0; $columnIndex -lt $header.Count; ++$columnIndex) {
        $table.Cell(1, $columnIndex + 1).Range.Text = $header[$columnIndex]
    }

    for ($rowIndex = 0; $rowIndex -lt $dataLines.Count; ++$rowIndex) {
        $cells = Split-MarkdownTableRow -Line $dataLines[$rowIndex]
        for ($columnIndex = 0; $columnIndex -lt $columnCount; ++$columnIndex) {
            $value = if ($columnIndex -lt $cells.Count) { $cells[$columnIndex] } else { "" }
            $table.Cell($rowIndex + 2, $columnIndex + 1).Range.Text = $value
        }
    }

    $table.Range.Font.Name = "Aptos"
    $table.Range.Font.Size = 10
    $table.AutoFitBehavior(1) | Out-Null

    $Selection.SetRange($table.Range.End, $table.Range.End)
    $Selection.TypeParagraph()
}

if (-not (Test-Path -LiteralPath $SourcePath)) {
    throw "Source markdown not found: $SourcePath"
}

$sourceLines = Get-Content -LiteralPath $SourcePath
$titleLine = $sourceLines | Where-Object { $_ -match '^# ' } | Select-Object -First 1
$title = if ($null -ne $titleLine) { $titleLine.Substring(2).Trim() } else { "MFDStudio Interoperability Specification" }

$word = $null
$document = $null

try {
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $document = $word.Documents.Add()
    $selection = $word.Selection

    try {
        $document.BuiltInDocumentProperties("Title").Value = $title
        $document.BuiltInDocumentProperties("Subject").Value = "External client interoperability contract"
        $document.BuiltInDocumentProperties("Company").Value = "MFDStudio"
    }
    catch {
    }

    $selection.Style = -63
    $selection.TypeText($title)
    $selection.TypeParagraph()
    $selection.TypeParagraph()

    $selection.Font.Name = "Aptos"
    $selection.Font.Size = 11
    $selection.Font.Italic = $true
    $selection.TypeText("Generated from $SourcePath")
    $selection.TypeParagraph()
    $selection.TypeText("Generated on $(Get-Date -Format 'yyyy-MM-dd HH:mm')")
    $selection.TypeParagraph()
    $selection.Font.Italic = $false
    $selection.InsertBreak(7)

    $selection.Style = -2
    $selection.TypeText("Table of Contents")
    $selection.TypeParagraph()
    $tocRange = $selection.Range
    $document.TablesOfContents.Add($tocRange, $true, 1, 3) | Out-Null
    $selection.TypeParagraph()
    $selection.InsertBreak(7)

    $paragraphBuffer = New-Object System.Collections.Generic.List[string]
    $codeBuffer = New-Object System.Collections.Generic.List[string]
    $tableBuffer = New-Object System.Collections.Generic.List[string]
    $listBuffer = New-Object System.Collections.Generic.List[string]
    $inCodeBlock = $false

    $flushParagraph = {
        if ($paragraphBuffer.Count -eq 0) {
            return
        }

        $text = ($paragraphBuffer -join " ").Trim()
        Add-ParagraphText -Selection $selection -Text $text -StyleId -1
        $paragraphBuffer.Clear()
    }

    $flushList = {
        if ($listBuffer.Count -eq 0) {
            return
        }

        foreach ($item in $listBuffer) {
            Add-ParagraphText -Selection $selection -Text $item -StyleId -1
        }
        $selection.TypeParagraph()
        $listBuffer.Clear()
    }

    $flushTable = {
        if ($tableBuffer.Count -eq 0) {
            return
        }

        Add-TableFromMarkdown -Document $document -Selection $selection -Lines $tableBuffer.ToArray()
        $selection.TypeParagraph()
        $tableBuffer.Clear()
    }

    $flushCode = {
        if ($codeBuffer.Count -eq 0) {
            return
        }

        Add-CodeBlock -Document $document -Selection $selection -Lines $codeBuffer.ToArray()
        $codeBuffer.Clear()
    }

    $skippedPrimaryTitle = $false

    foreach ($line in $sourceLines) {
        if (-not $skippedPrimaryTitle -and $line -eq $titleLine) {
            $skippedPrimaryTitle = $true
            continue
        }

        if ($inCodeBlock) {
            if ($line.Trim().StartsWith('```')) {
                & $flushCode
                $inCodeBlock = $false
            }
            else {
                $codeBuffer.Add($line)
            }
            continue
        }

        if ($line.Trim().StartsWith('```')) {
            & $flushParagraph
            & $flushList
            & $flushTable
            $inCodeBlock = $true
            continue
        }

        if ($line -match '^\|') {
            & $flushParagraph
            & $flushList
            $tableBuffer.Add($line)
            continue
        }

        if ($tableBuffer.Count -gt 0) {
            & $flushTable
        }

        if ([string]::IsNullOrWhiteSpace($line)) {
            & $flushParagraph
            & $flushList
            continue
        }

        if ($line -match '^### ') {
            & $flushParagraph
            & $flushList
            Add-ParagraphText -Selection $selection -Text $line.Substring(4).Trim() -StyleId -4
            continue
        }

        if ($line -match '^## ') {
            & $flushParagraph
            & $flushList
            Add-ParagraphText -Selection $selection -Text $line.Substring(3).Trim() -StyleId -3
            continue
        }

        if ($line -match '^# ') {
            & $flushParagraph
            & $flushList
            Add-ParagraphText -Selection $selection -Text $line.Substring(2).Trim() -StyleId -2
            continue
        }

        if ($line -match '^\d+\. ') {
            & $flushParagraph
            $listBuffer.Add($line.Trim())
            continue
        }

        if ($line -match '^- ') {
            & $flushParagraph
            $listBuffer.Add($line.Trim())
            continue
        }

        $paragraphBuffer.Add($line.Trim())
    }

    & $flushParagraph
    & $flushList
    & $flushTable
    & $flushCode

    if ($document.TablesOfContents.Count -gt 0) {
        $document.TablesOfContents.Item(1).Update() | Out-Null
    }

    $outputDirectory = Split-Path -Parent $OutputPath
    if (-not [string]::IsNullOrWhiteSpace($outputDirectory) -and -not (Test-Path -LiteralPath $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory | Out-Null
    }

    $outputFile = New-Item -ItemType File -Path $OutputPath -Force
    $resolvedOutputPath = (Resolve-Path -LiteralPath $outputFile.FullName).Path
    $document.SaveAs([ref]$resolvedOutputPath, [ref]16)
}
finally {
    if ($null -ne $document) {
        $document.Close()
        [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($document)
    }

    if ($null -ne $word) {
        $word.Quit()
        [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($word)
    }

    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}
