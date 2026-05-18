# LabelTorch Green Package Build Script
# Creates a portable, no-install package with embedded Python environment

$ErrorActionPreference = "Stop"

Write-Host "=== LabelTorch Green Package Build ===" -ForegroundColor Cyan

# Configuration
$packageName = "LabelTorch-Green-v0.1.0"
$scriptPath = $PSScriptRoot
$rootDir = Resolve-Path "$scriptPath/.."
$deployDir = "$rootDir/deploy/$packageName"
$buildPreset = "mingw-release"
$qtPath = "C:\A\QT\6.9.3\mingw_64"
$pythonEnv = "C:\A\Anaconda\envs\labeltorch"
$buildDir = "$rootDir/out/build/$buildPreset"

Write-Host "[1/6] Cleaning previous build..." -ForegroundColor Yellow
if (Test-Path $deployDir) {
    Remove-Item -Path $deployDir -Recurse -Force
}
New-Item -Path $deployDir -ItemType Directory | Out-Null

Write-Host "[2/6] Building application..." -ForegroundColor Yellow
$env:PATH = "$qtPath/bin;$env:PATH"
Set-Location $rootDir
cmake --preset $buildPreset
cmake --build --preset $buildPreset

Write-Host "[3/6] Copying executable..." -ForegroundColor Yellow
Copy-Item -Path "$buildDir/labeltorch.exe" -Destination $deployDir

Write-Host "[4/6] Running windeployqt..." -ForegroundColor Yellow
& "$qtPath/bin/windeployqt.exe" "$deployDir/labeltorch.exe" --release --no-translations --no-opengl-sw

Write-Host "[5/6] Copying Python environment..." -ForegroundColor Yellow
$pythonDest = "$deployDir/python"
New-Item -Path $pythonDest -ItemType Directory | Out-Null
Copy-Item -Path "$pythonEnv/python.exe" -Destination $pythonDest
Copy-Item -Path "$pythonEnv/pythonw.exe" -Destination $pythonDest
Copy-Item -Path "$pythonEnv/DLLs" -Destination $pythonDest -Recurse
Copy-Item -Path "$pythonEnv/Lib" -Destination $pythonDest -Recurse
Copy-Item -Path "$pythonEnv/Scripts" -Destination $pythonDest -Recurse
Copy-Item -Path "$pythonEnv/conda-meta" -Destination $pythonDest -Recurse

Write-Host "[6/6] Copying backend and creating launcher..." -ForegroundColor Yellow
New-Item -Path "$deployDir/backend" -ItemType Directory | Out-Null
Copy-Item -Path "$rootDir/backend/labeltorch_backend" -Destination "$deployDir/backend" -Recurse

# Create launcher script
$launcherContent = @'
@echo off
set PATH=%~dp0;%~dp0python;%~dp0python\Scripts;%PATH%
set PYTHONPATH=%~dp0backend
start labeltorch.exe
'@
$launcherContent | Out-File -FilePath "$deployDir/start_labeltorch.bat" -Encoding ASCII

# Create README
$readmeContent = @"
LabelTorch v0.1.0 (Green Package)
Industrial Defect Detection Software

Features:
- Import dataset (images + txt labels)
- Train YOLO models
- Export models (pt/onnx)

Run start_labeltorch.bat to launch
"@
$readmeContent | Out-File -FilePath "$deployDir/README.txt" -Encoding UTF8

# Create zip archive
Set-Location "$rootDir/deploy"
Compress-Archive -Path $packageName -DestinationPath "$packageName.zip" -Force

Write-Host "=== Green Package created: deploy/$packageName.zip ===" -ForegroundColor Green
Set-Location $rootDir