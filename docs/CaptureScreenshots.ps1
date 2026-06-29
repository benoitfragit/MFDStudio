param(
    [string]$RuntimeDirectory = "",
    [string]$WindowFile = "assets/windows/demo_pages_cockpit.json",
    [string]$OutputDirectory = "docs/src/images"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $repoRoot

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class Win32DocCapture
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool MoveWindow(IntPtr hWnd, int x, int y, int width, int height, bool repaint);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);
}
'@

function Resolve-RuntimeDirectory
{
    param([string]$RequestedDirectory)

    if (-not [string]::IsNullOrWhiteSpace($RequestedDirectory))
    {
        $resolved = Resolve-Path $RequestedDirectory
        if (-not (Test-Path (Join-Path $resolved "mfd_window.exe")))
        {
            throw "Unable to find mfd_window.exe in '$resolved'."
        }

        return $resolved.Path
    }

    $candidate = Get-ChildItem -Path (Join-Path $repoRoot "_Exec") -Filter mfd_window.exe -Recurse -File -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty DirectoryName

    if ([string]::IsNullOrWhiteSpace($candidate))
    {
        throw "Unable to locate a staged runtime under '_Exec'. Build the Win32 debug targets first."
    }

    return $candidate
}

function Start-GuiProcess
{
    param(
        [string]$FilePath,
        [string]$WorkingDirectory,
        [string[]]$Arguments = @()
    )

    if ($Arguments.Count -gt 0)
    {
        return Start-Process -FilePath $FilePath -WorkingDirectory $WorkingDirectory -ArgumentList $Arguments -PassThru
    }

    return Start-Process -FilePath $FilePath -WorkingDirectory $WorkingDirectory -PassThru
}

function Wait-MainWindow
{
    param(
        [System.Diagnostics.Process]$Process,
        [int]$TimeoutSeconds = 20
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do
    {
        Start-Sleep -Milliseconds 250
        $Process.Refresh()
        if ($Process.HasExited)
        {
            throw ("Process '{0}' exited before exposing a main window." -f $Process.ProcessName)
        }
    }
    while ($Process.MainWindowHandle -eq 0 -and (Get-Date) -lt $deadline)

    if ($Process.MainWindowHandle -eq 0)
    {
        throw ("Timed out while waiting for the '{0}' main window." -f $Process.ProcessName)
    }

    return $Process
}

function Set-WindowBounds
{
    param(
        [System.Diagnostics.Process]$Process,
        [int]$X,
        [int]$Y,
        [int]$Width,
        [int]$Height
    )

    [Win32DocCapture]::ShowWindowAsync($Process.MainWindowHandle, 9) | Out-Null
    [Win32DocCapture]::MoveWindow($Process.MainWindowHandle, $X, $Y, $Width, $Height, $true) | Out-Null
}

function Capture-WindowImage
{
    param(
        [System.Diagnostics.Process]$Process,
        [string]$OutputFile
    )

    [Win32DocCapture]::ShowWindowAsync($Process.MainWindowHandle, 9) | Out-Null
    [Win32DocCapture]::SetForegroundWindow($Process.MainWindowHandle) | Out-Null
    Start-Sleep -Milliseconds 600

    $rect = New-Object Win32DocCapture+RECT
    if (-not [Win32DocCapture]::GetWindowRect($Process.MainWindowHandle, [ref]$rect))
    {
        throw ("Unable to read the bounds of '{0}'." -f $Process.ProcessName)
    }

    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -le 0 -or $height -le 0)
    {
        throw ("Invalid capture bounds for '{0}'." -f $Process.ProcessName)
    }

    $bitmap = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)

    try
    {
        $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
        $bitmap.Save($OutputFile, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally
    {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Stop-GuiProcess
{
    param([System.Diagnostics.Process]$Process)

    if ($null -eq $Process)
    {
        return
    }

    try
    {
        $Process.Refresh()
        if ($Process.HasExited)
        {
            return
        }

        $Process.CloseMainWindow() | Out-Null
        if (-not $Process.WaitForExit(3000))
        {
            Stop-Process -Id $Process.Id -Force
        }
    }
    catch
    {
        if (-not $Process.HasExited)
        {
            Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
        }
    }
}

$runtimeDirectory = Resolve-RuntimeDirectory -RequestedDirectory $RuntimeDirectory
$runtimeExe = Join-Path $runtimeDirectory "mfd_window.exe"
$clientExe = Join-Path $runtimeDirectory "client_mockup.exe"
$editorExe = Join-Path $runtimeDirectory "mfd_editor.exe"
$resolvedWindowFile = (Resolve-Path (Join-Path $repoRoot $WindowFile)).Path
$resolvedOutputDirectory = Join-Path $repoRoot $OutputDirectory
$resolvedEditorOutputDirectory = Join-Path $resolvedOutputDirectory "editor"

New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $resolvedEditorOutputDirectory | Out-Null

$screenBounds = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
$runtimeProcess = $null
$clientProcess = $null
$editorProcess = $null

try
{
    $runtimeProcess = Wait-MainWindow (Start-GuiProcess -FilePath $runtimeExe -WorkingDirectory $runtimeDirectory -Arguments @("--window", $resolvedWindowFile))
    Set-WindowBounds -Process $runtimeProcess -X ($screenBounds.Left + 40) -Y ($screenBounds.Top + 40) -Width ([Math]::Min(1320, $screenBounds.Width - 80)) -Height ([Math]::Min(860, $screenBounds.Height - 100))
    [Win32DocCapture]::SetForegroundWindow($runtimeProcess.MainWindowHandle) | Out-Null
    Start-Sleep -Milliseconds 400
    [System.Windows.Forms.SendKeys]::SendWait("4")
    Start-Sleep -Milliseconds 800

    $clientProcess = Wait-MainWindow (Start-GuiProcess -FilePath $clientExe -WorkingDirectory $runtimeDirectory)
    Set-WindowBounds -Process $clientProcess -X ($screenBounds.Left + 80) -Y ($screenBounds.Top + 80) -Width ([Math]::Min(620, $screenBounds.Width - 160)) -Height ([Math]::Min(900, $screenBounds.Height - 140))

    $editorProcess = Wait-MainWindow (Start-GuiProcess -FilePath $editorExe -WorkingDirectory $runtimeDirectory)
    Set-WindowBounds -Process $editorProcess -X ($screenBounds.Left + 100) -Y ($screenBounds.Top + 60) -Width ([Math]::Min(1460, $screenBounds.Width - 200)) -Height ([Math]::Min(900, $screenBounds.Height - 120))

    Start-Sleep -Seconds 2

    $captures = @(
        @{ Process = $runtimeProcess; File = (Join-Path $resolvedOutputDirectory "mfd_window_cockpit_capture.png") },
        @{ Process = $clientProcess; File = (Join-Path $resolvedOutputDirectory "client_mockup_demo.png") },
        @{ Process = $editorProcess; File = (Join-Path $resolvedEditorOutputDirectory "editor-startpage.png") }
    )

    foreach ($capture in $captures)
    {
        Capture-WindowImage -Process $capture.Process -OutputFile $capture.File
        Write-Host ("Captured {0}" -f (Resolve-Path -Relative $capture.File))
    }
}
finally
{
    Stop-GuiProcess -Process $editorProcess
    Stop-GuiProcess -Process $clientProcess
    Stop-GuiProcess -Process $runtimeProcess
}
