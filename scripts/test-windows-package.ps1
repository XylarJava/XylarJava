# PowerShell 7: verify an existing single EXE; no build or packaging commands.
param(
    [string]$Executable = "$PSScriptRoot\..\dist-single\XylarJava-Windows\XylarJava.exe",
    [string]$LogDirectory = "$PSScriptRoot\..\dist-single\smoke-logs",
    [int]$ColdTimeoutSeconds = 180,
    [int]$WarmTimeoutSeconds = 60
)
$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $Executable).Path
New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
$LogDirectory = (Resolve-Path $LogDirectory).Path

# Loader failures must return an exit code instead of opening a system dialog on the runner.
Add-Type @'
using System.Runtime.InteropServices;
using System.Text;
using System.Collections.Generic;
public static class XylarSmokeNative {
    [DllImport("kernel32.dll")]
    public static extern uint SetErrorMode(uint mode);
    private delegate bool ChildVisitor(System.IntPtr window, System.IntPtr state);
    [DllImport("user32.dll")]
    private static extern bool EnumChildWindows(System.IntPtr window, ChildVisitor visitor, System.IntPtr state);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(System.IntPtr window, StringBuilder text, int length);
    public static string ReadDialog(System.IntPtr window) {
        if (window == System.IntPtr.Zero) return "";
        var lines = new List<string>();
        EnumChildWindows(window, (child, state) => {
            var text = new StringBuilder(4096);
            if (GetWindowText(child, text, text.Capacity) > 0) lines.Add(text.ToString());
            return true;
        }, System.IntPtr.Zero);
        return string.Join(System.Environment.NewLine, lines);
    }
}
'@
$previousErrorMode = [XylarSmokeNative]::SetErrorMode(0x8003)
$originalPath = $env:PATH
$originalQtDebug = $env:QT_DEBUG_PLUGINS
$originalQtConsole = $env:QT_LOGGING_TO_CONSOLE
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
$env:QT_DEBUG_PLUGINS = '1'
$env:QT_LOGGING_TO_CONSOLE = '1'

function Invoke-VersionCheck([string]$Name, [int]$TimeoutSeconds) {
    $info = [System.Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $exe
    $info.WorkingDirectory = Split-Path $exe
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.ArgumentList.Add('--version')
    # The package must locate its own Qt plugins, independently of the runner's SDK.
    [void]$info.Environment.Remove('QT_PLUGIN_PATH')
    [void]$info.Environment.Remove('QT_QPA_PLATFORM_PLUGIN_PATH')
    [void]$info.Environment.Remove('QT_QPA_PLATFORM')
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $info
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $finished = $process.WaitForExit($TimeoutSeconds * 1000)
        if (-not $finished) {
            Get-CimInstance Win32_Process -Filter "Name = 'XylarJava.exe'" |
                Select-Object ProcessId, ParentProcessId, ExecutablePath, CommandLine |
                Format-List | Out-File (Join-Path $LogDirectory "$Name-processes.txt")
            Get-Process XylarJava -ErrorAction SilentlyContinue |
                Select-Object Id, MainWindowTitle, Path, @{Name='DialogText'; Expression={ [XylarSmokeNative]::ReadDialog($_.MainWindowHandle) }} |
                Format-List | Out-File (Join-Path $LogDirectory "$Name-windows.txt")
            $process.Kill($true)
            [void]$process.WaitForExit(10000)
        }
        foreach ($stream in @(@('stdout', $stdout), @('stderr', $stderr))) {
            if ($stream[1].Wait(10000)) {
                $text = $stream[1].GetAwaiter().GetResult()
                $text | Out-File -Encoding utf8 (Join-Path $LogDirectory "$Name-$($stream[0]).txt")
                if ($text) { Write-Host $text }
            }
        }
        if (-not $finished) { throw "$Name startup exceeded $TimeoutSeconds seconds; see smoke logs and window titles" }
        if ($process.ExitCode -ne 0) { throw "$Name startup failed: exit $($process.ExitCode); see smoke logs" }
        Write-Host "$Name startup passed in $([math]::Round($timer.Elapsed.TotalSeconds, 1)) seconds"
    }
    finally { $process.Dispose() }
}

try {
    Get-FileHash $exe -Algorithm SHA256 | Format-List | Out-File (Join-Path $LogDirectory 'package-sha256.txt')
    Invoke-VersionCheck 'cold' $ColdTimeoutSeconds
    Invoke-VersionCheck 'warm' $WarmTimeoutSeconds
    $shell = New-Object -ComObject WScript.Shell
    try {
        foreach ($folder in @([Environment+SpecialFolder]::DesktopDirectory, [Environment+SpecialFolder]::Programs)) {
            $directory = [Environment]::GetFolderPath($folder)
            $shortcut = Join-Path $directory 'XylarJava.lnk'
            if (-not (Test-Path $shortcut)) { throw "Missing shortcut: $shortcut" }
            $link = $shell.CreateShortcut($shortcut)
            try {
                if (-not (Test-Path $link.TargetPath)) { throw "Missing shortcut target: $($link.TargetPath)" }
            }
            finally { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($link) }
        }
    }
    finally { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell) }
}
finally {
    $env:PATH = $originalPath
    $env:QT_DEBUG_PLUGINS = $originalQtDebug
    $env:QT_LOGGING_TO_CONSOLE = $originalQtConsole
    [void][XylarSmokeNative]::SetErrorMode($previousErrorMode)
}
