# 运行 C++ 测试套件（仓库相对路径，适配任意盘符/机器）
# 用法：powershell -File scripts/run_tests.ps1 [-BuildDir out/build/x64-release]
param(
    [string]$BuildDir = ""
)

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) {
    # 自动探测：优先 x64-release，其次任一含 CTestTestfile.cmake 的构建目录
    $candidate = Join-Path $repoRoot "out/build/x64-release"
    if (Test-Path (Join-Path $candidate "CTestTestfile.cmake")) {
        $BuildDir = $candidate
    } else {
        $found = Get-ChildItem -Path (Join-Path $repoRoot "out/build") -Recurse -Filter "CTestTestfile.cmake" -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($found) { $BuildDir = Split-Path -Parent $found.FullName }
    }
}

if (-not $BuildDir -or -not (Test-Path (Join-Path $BuildDir "CTestTestfile.cmake"))) {
    Write-Host "ERROR: 未找到构建目录（请先 cmake --preset x64-release 或用 -BuildDir 指定）" -ForegroundColor Red
    exit 1
}

Write-Host "=== C++ 测试 @ $BuildDir ==="
$env:PATH = "C:\Qt\6.11.1\msvc2022_64\bin;" + $env:PATH
Push-Location $BuildDir
try {
    ctest -C Release --output-on-failure
    $code = $LASTEXITCODE
} finally {
    Pop-Location
}
exit $code
