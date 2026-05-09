@echo off
REM LabelTorch Windows Deployment Script
REM Builds Release and deploys Qt dependencies

echo === LabelTorch Windows Deployment ===

REM Set up MSVC environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

REM Build Release
cd /d %~dp0..
cmake --preset msvc2022-release
cmake --build --preset msvc2022-release

REM Create deploy directory
set DEPLOY_DIR=deploy\LabelTorch
if exist %DEPLOY_DIR% rmdir /s /q %DEPLOY_DIR%
mkdir %DEPLOY_DIR%

REM Copy executable
copy out\build\msvc2022-release\labeltorch.exe %DEPLOY_DIR%\

REM Run windeployqt
F:\A\QT\6.11.0\msvc2022_64\bin\windeployqt.exe %DEPLOY_DIR%\labeltorch.exe --release --no-translations --no-opengl-sw

REM Copy Python backend
mkdir %DEPLOY_DIR%\backend
xcopy /e /i /y backend\labeltorch_backend %DEPLOY_DIR%\backend\labeltorch_backend
copy backend\pyproject.toml %DEPLOY_DIR%\backend\
copy backend\requirements.txt %DEPLOY_DIR%\backend\

REM Copy QML modules
mkdir %DEPLOY_DIR%\qml
xcopy /e /i /y out\build\msvc2022-release\qml %DEPLOY_DIR%\qml\

echo === Deployment complete: %DEPLOY_DIR% ===
pause
