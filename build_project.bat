@echo off
echo Starting build process...
cd /d "f:\project\my\LabelTorchV"

echo Initializing Visual Studio environment...
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

if errorlevel 1 (
    echo ERROR: Failed to initialize VS environment
    exit /b 1
)

echo VS environment initialized successfully
echo.

echo Running CMake configuration...
cmake --preset msvc2022-debug

if errorlevel 1 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

echo CMake configuration succeeded
echo.

echo Building project...
cmake --build out/build/msvc2022-debug --config Debug

if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

echo Build completed successfully

echo.
echo Checking for built executable...
if exist "out\build\msvc2022-debug\Debug\labeltorch.exe" (
    echo Found: out\build\msvc2022-debug\Debug\labeltorch.exe
) else (
    echo WARNING: Executable not found at expected location
)

exit /b 0