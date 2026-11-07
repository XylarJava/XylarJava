# Build one distributable EXE. Qt, native libraries and .NET are embedded inside it.
param(
    [string]$BuildDir = "$PSScriptRoot\..\build-win",
    [string]$Configuration = "Release",
    [switch]$SkipCmakeBuild
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot\..").Path
$buildPath = (Resolve-Path $BuildDir).Path
$dist = Join-Path $root "dist-single"
$payload = Join-Path $dist "payload"
$packer = Join-Path $root "tools\SingleExePacker"
$finalDir = Join-Path $dist "XylarJava-Windows"

function Check-NativeExit([string]$Step) {
    if ($LASTEXITCODE -ne 0) { throw "$Step failed (exit code $LASTEXITCODE)" }
}
if (-not $SkipCmakeBuild) {
    cmake --build $buildPath --config $Configuration --parallel 2
    Check-NativeExit "CMake build"
}
if (Test-Path $dist) { Remove-Item -LiteralPath $dist -Recurse -Force }
New-Item -ItemType Directory -Path $payload -Force | Out-Null
cmake --install $buildPath --config $Configuration --prefix $payload
Check-NativeExit "CMake install"

# OpenSSL is loaded dynamically by Qt, so dependency scanners may miss these DLLs.
foreach ($name in @("libssl-3-x64.dll", "libcrypto-3-x64.dll")) {
    foreach ($directory in ($env:PATH -split ';')) {
        if ([string]::IsNullOrWhiteSpace($directory)) { continue }
        $candidate = Join-Path $directory $name
        if (Test-Path -LiteralPath $candidate) {
            Copy-Item -LiteralPath $candidate -Destination $payload -Force
            break
        }
    }
}

# Account configuration and portable user data do not belong in a public package.
Get-ChildItem $payload -Recurse -File -Filter "auth-settings.ini" | Remove-Item -Force
Get-ChildItem $payload -Recurse -File -Filter "portable.txt" | Remove-Item -Force
foreach ($required in @("XylarJava.exe", "platforms\qwindows.dll", "Qt6Core.dll", "Qt6Widgets.dll")) {
    if (-not (Test-Path (Join-Path $payload $required))) { throw "Missing runtime file: $required" }
}
$clientJar = Join-Path $root "client\build\libs\xylarjava-client-0.1.0.jar"
if (-not (Test-Path $clientJar)) { throw "Build the client jar before packaging Windows" }
Copy-Item $clientJar $payload

@"
[Paths]
Prefix=.
Plugins=.
"@ | Set-Content (Join-Path $payload "qt.conf") -Encoding ascii
$zip = Join-Path $dist "payload.zip"
Compress-Archive -Path (Join-Path $payload "*") -DestinationPath $zip -CompressionLevel Optimal
Copy-Item $zip (Join-Path $packer "payload.zip") -Force
$publish = Join-Path $dist "publish"
dotnet publish (Join-Path $packer "XylarJava.csproj") -c Release -r win-x64 --self-contained true -o $publish
Check-NativeExit "Single EXE publish"
New-Item -ItemType Directory -Path $finalDir | Out-Null
Copy-Item (Join-Path $publish "XylarJava.exe") $finalDir
if (@(Get-ChildItem $finalDir -File).Count -ne 1) { throw "Expected exactly one distributable file" }
Get-FileHash (Join-Path $finalDir "XylarJava.exe") -Algorithm SHA256
Write-Host "Single EXE: $(Join-Path $finalDir 'XylarJava.exe')"
