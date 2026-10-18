param(
    [switch]$SkipBuild = $false
)

$ErrorActionPreference = "Stop"
$rootDir = (Get-Item $PSScriptRoot).Parent.FullName
Set-Location $rootDir

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "  FlowSight v1.2.0 Automated Release Packaging Pipeline" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan

# 1. Build project
if (-not $SkipBuild) {
    Write-Host "[1/6] Building FlowSight and FlowSightEngine in Release mode..." -ForegroundColor Yellow
    cmake --build build --config Release
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed, aborting pipeline!"
    }
} else {
    Write-Host "[1/6] Skipping build, using existing binaries." -ForegroundColor Gray
}

# 2. Prepare portable dist directory
$distRoot = Join-Path $rootDir "dist"
$portableDir = Join-Path $distRoot "FlowSight_v1.2.0_Portable"
Write-Host "[2/6] Preparing release directory: $portableDir" -ForegroundColor Yellow

if (Test-Path $portableDir) {
    Remove-Item -Recurse -Force $portableDir
}
New-Item -ItemType Directory -Force $portableDir | Out-Null

# 3. Copy binaries and runtime dependencies
Write-Host "[3/6] Collecting binaries and runtime dependencies..." -ForegroundColor Yellow

$releaseDir = Join-Path $rootDir "build\Release"
Copy-Item (Join-Path $releaseDir "FlowSight.exe") $portableDir -Force
Copy-Item (Join-Path $releaseDir "FlowSightEngine.dll") $portableDir -Force

# Run windeployqt
$windeployqt = "C:\Qt\6.9.2\msvc2022_64\bin\windeployqt.exe"
if (Test-Path $windeployqt) {
    Write-Host "  Running windeployqt to deploy Qt6 libraries..." -ForegroundColor Gray
    & $windeployqt --release --no-translations --compiler-runtime --dir $portableDir (Join-Path $portableDir "FlowSight.exe")
} else {
    Write-Warning "windeployqt.exe not found!"
}

# Copy Qt plugins: sqldrivers/qsqlite.dll and multimedia
$qtPlugins = "C:\Qt\6.9.2\msvc2022_64\plugins"
if (Test-Path (Join-Path $qtPlugins "sqldrivers\qsqlite.dll")) {
    $sqlDir = Join-Path $portableDir "sqldrivers"
    New-Item -ItemType Directory -Force $sqlDir | Out-Null
    Copy-Item (Join-Path $qtPlugins "sqldrivers\qsqlite.dll") $sqlDir -Force
}
if (Test-Path (Join-Path $qtPlugins "multimedia\ffmpegmediaplugin.dll")) {
    $mmDir = Join-Path $portableDir "multimedia"
    New-Item -ItemType Directory -Force $mmDir | Out-Null
    Copy-Item (Join-Path $qtPlugins "multimedia\ffmpegmediaplugin.dll") $mmDir -Force
}
if (Test-Path (Join-Path $qtPlugins "multimedia\windowsmediaplugin.dll")) {
    $mmDir = Join-Path $portableDir "multimedia"
    New-Item -ItemType Directory -Force $mmDir | Out-Null
    Copy-Item (Join-Path $qtPlugins "multimedia\windowsmediaplugin.dll") $mmDir -Force
}

# Copy OpenCV DLLs
$opencvBin = "D:\opencv\build\x64\vc16\bin"
if (Test-Path (Join-Path $opencvBin "opencv_world480.dll")) {
    Copy-Item (Join-Path $opencvBin "opencv_world480.dll") $portableDir -Force
}
if (Test-Path (Join-Path $opencvBin "opencv_videoio_ffmpeg480_64.dll")) {
    Copy-Item (Join-Path $opencvBin "opencv_videoio_ffmpeg480_64.dll") $portableDir -Force
}

# Copy ONNX Runtime DLLs
$ortLib = "D:\sherpa-onnx\build-rt\_deps\onnxruntime-src\lib"
if (Test-Path (Join-Path $ortLib "onnxruntime.dll")) {
    Copy-Item (Join-Path $ortLib "onnxruntime.dll") $portableDir -Force
}
if (Test-Path (Join-Path $ortLib "onnxruntime_providers_shared.dll")) {
    Copy-Item (Join-Path $ortLib "onnxruntime_providers_shared.dll") $portableDir -Force
}
if (Test-Path (Join-Path $ortLib "onnxruntime_providers_cuda.dll")) {
    Copy-Item (Join-Path $ortLib "onnxruntime_providers_cuda.dll") $portableDir -Force
}

# 4. Copy AI models, configs, and demo assets
Write-Host "[4/6] Packaging AI models and demo media assets..." -ForegroundColor Yellow
$destModels = Join-Path $portableDir "models"
New-Item -ItemType Directory -Force $destModels | Out-Null
Copy-Item (Join-Path $rootDir "models\*") $destModels -Force

$destConfigs = Join-Path $portableDir "configs"
New-Item -ItemType Directory -Force $destConfigs | Out-Null
if (Test-Path (Join-Path $rootDir "configs\flowsight.ini")) {
    Copy-Item (Join-Path $rootDir "configs\flowsight.ini") $destConfigs -Force
}
if (Test-Path (Join-Path $rootDir "configs\test_corridor.mp4")) {
    Copy-Item (Join-Path $rootDir "configs\test_corridor.mp4") $destConfigs -Force
}

# Copy icon and visual resources
$destResources = Join-Path $portableDir "resources"
New-Item -ItemType Directory -Force $destResources | Out-Null
Copy-Item (Join-Path $rootDir "resources\*") $destResources -Force

# Create runtime directories
New-Item -ItemType Directory -Force (Join-Path $portableDir "data") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $portableDir "clips") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $portableDir "exports") | Out-Null

# Package C-ABI developer SDK
$sdkDir = Join-Path $portableDir "sdk"
New-Item -ItemType Directory -Force (Join-Path $sdkDir "include") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $sdkDir "lib") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $sdkDir "bin") | Out-Null

Copy-Item (Join-Path $rootDir "include\flowsight\FlowSightAPI.h") (Join-Path $sdkDir "include") -Force
Copy-Item (Join-Path $releaseDir "FlowSightEngine.lib") (Join-Path $sdkDir "lib") -Force
Copy-Item (Join-Path $releaseDir "FlowSightEngine.dll") (Join-Path $sdkDir "bin") -Force

# Copy README
if (Test-Path (Join-Path $distRoot "README.txt")) {
    Copy-Item (Join-Path $distRoot "README.txt") $portableDir -Force
}

# 5. Create Portable ZIP archive
Write-Host "[5/6] Creating standalone portable ZIP package..." -ForegroundColor Yellow
$zipPath = Join-Path $distRoot "FlowSight_v1.2.0_win64_portable.zip"
if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($portableDir, $zipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false)
$zipSizeMb = (Get-Item $zipPath).Length / 1MB
Write-Host ("  Portable ZIP package created: {0} ({1:N2} MB)" -f $zipPath, $zipSizeMb) -ForegroundColor Green

# 6. Compile Inno Setup installer
Write-Host "[6/6] Compiling Inno Setup installer..." -ForegroundColor Yellow
$isccCandidates = @(
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
)
$isccExe = $isccCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if ($isccExe) {
    $issFile = Join-Path $rootDir "scripts\installer.iss"
    & $isccExe $issFile
    $installerPath = Join-Path $distRoot "FlowSight_v1.2.0_Setup.exe"
    if (Test-Path $installerPath) {
        $installerSizeMb = (Get-Item $installerPath).Length / 1MB
        Write-Host ("  Installer executable created: {0} ({1:N2} MB)" -f $installerPath, $installerSizeMb) -ForegroundColor Green
    }
} else {
    Write-Warning "ISCC.exe not found, skipping installer compilation."
}

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "  FlowSight v1.2.0 Packaging Finished! Output directory: dist/" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan
