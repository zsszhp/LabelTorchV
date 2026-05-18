@echo off
REM LabelTorch Green Package Build Script
REM Creates a portable, no-install package with embedded Python environment

setlocal enabledelayedexpansion

echo === LabelTorch Green Package Build ===

REM Configuration
set PACKAGE_NAME=LabelTorch-Green-v0.1.0
set DEPLOY_DIR=%~dp0..\deploy\%PACKAGE_NAME%
set BUILD_PRESET=mingw-release
set QT_PATH=C:\A\QT\6.9.3\mingw_64
set PYTHON_ENV=C:\A\Anaconda\envs\labeltorch
set BUILD_DIR=%~dp0..\out\build\%BUILD_PRESET%

echo [1/6] Cleaning previous build...
if exist "%DEPLOY_DIR%" rmdir /s /q "%DEPLOY_DIR%"
mkdir "%DEPLOY_DIR%"

echo [2/6] Building application...
cd /d %~dp0..
set PATH=%QT_PATH%\bin;%PATH%
cmake --preset %BUILD_PRESET%
cmake --build --preset %BUILD_PRESET%

echo [3/6] Copying executable...
copy "%BUILD_DIR%\labeltorch.exe" "%DEPLOY_DIR%\"

echo [4/6] Running windeployqt...
"%QT_PATH%\bin\windeployqt.exe" "%DEPLOY_DIR%\labeltorch.exe" --release --no-translations --no-opengl-sw

echo [5/6] Copying Python environment...
mkdir "%DEPLOY_DIR%\python"
xcopy /e /i /y "%PYTHON_ENV%\python.exe" "%DEPLOY_DIR%\python\"
xcopy /e /i /y "%PYTHON_ENV%\pythonw.exe" "%DEPLOY_DIR%\python\"
xcopy /e /i /y "%PYTHON_ENV%\DLLs" "%DEPLOY_DIR%\python\DLLs"
xcopy /e /i /y "%PYTHON_ENV%\Lib" "%DEPLOY_DIR%\python\Lib"
xcopy /e /i /y "%PYTHON_ENV%\Scripts" "%DEPLOY_DIR%\python\Scripts"
xcopy /e /i /y "%PYTHON_ENV%\conda-meta" "%DEPLOY_DIR%\python\conda-meta"

echo [6/6] Copying backend and creating launcher...
mkdir "%DEPLOY_DIR%\backend"
xcopy /e /i /y %~dp0..\backend\labeltorch_backend "%DEPLOY_DIR%\backend\labeltorch_backend"

REM Create launcher script
echo @echo off > "%DEPLOY_DIR%\start_labeltorch.bat"
echo set PATH=%%~dp0;%%~dp0python;%%~dp0python\Scripts;%%PATH%% >> "%DEPLOY_DIR%\start_labeltorch.bat"
echo set PYTHONPATH=%%~dp0backend >> "%DEPLOY_DIR%\start_labeltorch.bat"
echo start labeltorch.exe >> "%DEPLOY_DIR%\start_labeltorch.bat"

REM Create README
echo LabelTorch v0.1.0 (Green Package) > "%DEPLOY_DIR%\README.txt"
echo Industrial Defect Detection Software >> "%DEPLOY_DIR%\README.txt"
echo. >> "%DEPLOY_DIR%\README.txt"
echo Features: >> "%DEPLOY_DIR%\README.txt"
echo - Import dataset (images + txt labels) >> "%DEPLOY_DIR%\README.txt"
echo - Train YOLO models >> "%DEPLOY_DIR%\README.txt"
echo - Export models (pt/onnx) >> "%DEPLOY_DIR%\README.txt"
echo. >> "%DEPLOY_DIR%\README.txt"
echo Run start_labeltorch.bat to launch >> "%DEPLOY_DIR%\README.txt"

REM Create zip archive
cd %~dp0..\deploy
powershell Compress-Archive -Path %PACKAGE_NAME% -DestinationPath %PACKAGE_NAME%.zip

echo === Green Package created: deploy\%PACKAGE_NAME%.zip ===
pause