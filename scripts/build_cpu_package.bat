@echo off
REM LabelTorch CPU Green Package Build Script
REM Creates a portable, no-install package for CPU-only systems

echo === LabelTorch CPU Package Build ===

set PACKAGE_NAME=LabelTorch-CPU-v1.0.0
set DEPLOY_DIR=deploy\%PACKAGE_NAME%

REM Clean previous build
if exist %DEPLOY_DIR% rmdir /s /q %DEPLOY_DIR%
mkdir %DEPLOY_DIR%

REM Build and deploy using deploy script
call scripts\deploy_windows.bat

REM Move deployed files
move deploy\LabelTorch\* %DEPLOY_DIR%\

REM Create launcher script
echo @echo off > %DEPLOY_DIR%\start_labeltorch.bat
echo start labeltorch.exe >> %DEPLOY_DIR%\start_labeltorch.bat

REM Create README
echo LabelTorch v1.0.0 (CPU) > %DEPLOY_DIR%\README.txt
echo Industrial Defect Detection Software >> %DEPLOY_DIR%\README.txt
echo. >> %DEPLOY_DIR%\README.txt
echo Run start_labeltorch.bat to launch >> %DEPLOY_DIR%\README.txt

REM Create zip archive
cd deploy
powershell Compress-Archive -Path %PACKAGE_NAME% -DestinationPath %PACKAGE_NAME%.zip
cd ..

echo === CPU Package created: deploy\%PACKAGE_NAME%.zip ===
