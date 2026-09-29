<#
.SYNOPSIS
    标炬（LabelTorch）绿色包构建脚本 — 产出解压即用的免安装目录与 zip。

.DESCRIPTION
    流程：版本同步 → CMake 预设构建 → 部署主程序 / QML 模块 → windeployqt →
    拷贝精简 Python 运行时与后端源码 → 生成启动器 → 打包后冒烟校验 → 压缩 zip。

    路径全部参数化，可通过脚本参数或环境变量覆盖，不再依赖固定盘符。
    任一步骤失败立即以非零码退出，避免产出残缺包。

.PARAMETER QtRoot
    Qt 安装根目录。取值顺序：参数 > 环境变量 QT_ROOT > 本机常见路径。

.PARAMETER PythonEnv
    Python 环境根目录。取值顺序：参数 > 环境变量 LABELTORCH_PYTHON_ENV >
    环境变量 PYTHON_ENV > 本机常见路径。

.PARAMETER BuildPreset
    CMake 构建预设名，必须存在于 CMakePresets.json。默认 x64-release。

.PARAMETER PythonMode
    Python 环境打包方式：
      trim       — 仅保留 ultralytics/torch/onnx 运行时依赖链（默认，体积约 1.1~1.4 GB）
      full       — 完整拷贝 site-packages（约 2.2 GB，排查依赖问题时用）
      conda-pack — 调用 conda-pack 打包后解压（需预装 conda-pack）

.PARAMETER IncludeAnomaly
    附加 anomalib 异常检测依赖链（kornia/timm/lightning 等，体积增加约 200 MB）。

.PARAMETER Version
    覆盖版本号。默认从 CMakeLists.txt 的 project(VERSION) 读取（版本单一源）。

.PARAMETER SkipBuild
    跳过 CMake 配置与编译，直接使用 out/build/<preset> 中的既有产物。

.PARAMETER SkipSmoke
    跳过打包后冒烟校验（不推荐；默认失败即中止）。

.PARAMETER SkipZip
    只生成目录，不压缩 zip。

.PARAMETER SkipSync
    跳过版本同步脚本。

.EXAMPLE
    .\scripts\build_green_package.ps1

.EXAMPLE
    .\scripts\build_green_package.ps1 -QtRoot D:\Qt\6.11.1\msvc2022_64 -PythonEnv D:\envs\labeltorch -IncludeAnomaly

.NOTES
    离线 wheel 源（在无外网的打包机上重建 Python 环境时使用）：
        # 有网机器上，按目标平台预下载 wheel（与打包机 Python 小版本一致，当前 3.11）
        python -m pip download -r backend\requirements.txt ^
            -d wheels --platform win_amd64 --only-binary=:all: --python-version 3.11
        # 若启用异常检测，再补一份
        python -m pip download anomalib kornia timm lightning ^
            -d wheels --platform win_amd64 --only-binary=:all: --python-version 3.11
        # 打包机上离线安装到干净环境
        python -m pip install --no-index --find-links=wheels -r backend\requirements.txt

    体积与完整性说明（实测，Python 3.11 conda 环境）：
        完整环境总计约 2334 MB，其中 site-packages 约 2155 MB。
        trim 模式丢弃 PySide6（约 628 MB，与 Qt C++ 前端无关）、测试/打包工具
        （pytest/ruff/pyinstaller）、scikit 系（skimage/sklearn/imagecodecs，约 88 MB）
        等非运行时内容；保留 torch（CPU 版约 451 MB，含 torch_cpu.dll 254 MB）、
        ultralytics、onnx/onnxruntime、opencv、numpy/scipy、supervision、pandas、
        polars（ultralytics 惰性导入，含原生运行时约 182 MB）等训练/推理必需链。
        冒烟阶段会在打包目录内执行关键模块 import 自检，缺依赖会直接报错退出。
#>
[CmdletBinding()]
param(
    [string]$QtRoot = "",
    [string]$PythonEnv = "",
    [string]$BuildPreset = "",
    [ValidateSet("trim", "full", "conda-pack")]
    [string]$PythonMode = "trim",
    [switch]$IncludeAnomaly,
    [string]$Version = "",
    [switch]$SkipBuild,
    [switch]$SkipSmoke,
    [switch]$SkipZip,
    [switch]$SkipSync
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# ---------------------------------------------------------------------------
# 0. 参数与路径解析（参数 > 环境变量 > 默认值）
# ---------------------------------------------------------------------------
function Resolve-FirstPath {
    param([string[]]$Candidates, [string]$Label)
    foreach ($c in $Candidates) {
        if ([string]::IsNullOrWhiteSpace($c)) { continue }
        $p = $c.TrimEnd('\', '/')
        if (Test-Path -LiteralPath $p) { return (Resolve-Path -LiteralPath $p).Path }
    }
    throw "找不到 $Label。请通过参数或环境变量指定。候选：$($Candidates -join ' | ')"
}

if (-not $QtRoot) { $QtRoot = $env:QT_ROOT }
if (-not $BuildPreset) {
    if ($env:LABELTORCH_BUILD_PRESET) { $BuildPreset = $env:LABELTORCH_BUILD_PRESET }
    else { $BuildPreset = "x64-release" }
}
if (-not $PythonEnv) {
    if ($env:LABELTORCH_PYTHON_ENV) { $PythonEnv = $env:LABELTORCH_PYTHON_ENV }
    elseif ($env:PYTHON_ENV) { $PythonEnv = $env:PYTHON_ENV }
}

$QtRoot = Resolve-FirstPath -Candidates @($QtRoot, "C:\Qt\6.11.1\msvc2022_64", "C:\Qt\6.11.1\msvc2022_64\") -Label "Qt 安装目录"
$PythonEnv = Resolve-FirstPath -Candidates @($PythonEnv, "C:\A\anaconda\envs\labeltorch") -Label "Python 环境目录"

$scriptPath = $PSScriptRoot
$rootDir = (Resolve-Path (Join-Path $scriptPath "..")).Path
$buildDir = Join-Path $rootDir "out\build\$BuildPreset"
$deployRoot = Join-Path $rootDir "deploy"

# 版本号：显式参数优先，否则从 CMakeLists.txt 读取（单一源）
if (-not $Version) {
    $cmakeLists = Join-Path $rootDir "CMakeLists.txt"
    # 匹配 project(... VERSION x.y.z)，避免命中 cmake_minimum_required(VERSION ...)
    $m = Select-String -LiteralPath $cmakeLists -Pattern 'project\s*\([^)]*VERSION\s+(\d+\.\d+\.\d+)' | Select-Object -First 1
    if (-not $m) { throw "无法从 CMakeLists.txt 解析版本号（project(VERSION x.y.z)）" }
    $Version = $m.Matches[0].Groups[1].Value
}

$packageName = "LabelTorch-Green-v$Version"
$deployDir = Join-Path $deployRoot $packageName

Write-Host "=== 标炬绿色包构建 ===" -ForegroundColor Cyan
Write-Host "版本        : $Version"
Write-Host "预设        : $BuildPreset"
Write-Host "Qt          : $QtRoot"
Write-Host "Python 环境 : $PythonEnv"
Write-Host "Python 模式 : $PythonMode$(if ($IncludeAnomaly) { ' + anomalib' })"
Write-Host "输出目录    : $deployDir"

# ---------------------------------------------------------------------------
# 1. 版本同步（CMake project(VERSION) → 安装器 / 后端 / 文档）
# ---------------------------------------------------------------------------
if (-not $SkipSync) {
    Write-Host "[1/8] 同步版本号..." -ForegroundColor Yellow
    & (Join-Path $scriptPath "sync_version.ps1") -Version $Version
    if ($LASTEXITCODE -ne 0) { throw "版本同步失败" }
} else {
    Write-Host "[1/8] 跳过版本同步" -ForegroundColor Yellow
}

# ---------------------------------------------------------------------------
# 2. 清理并重建输出目录
# ---------------------------------------------------------------------------
Write-Host "[2/8] 准备输出目录..." -ForegroundColor Yellow
if (Test-Path -LiteralPath $deployDir) {
    Remove-Item -LiteralPath $deployDir -Recurse -Force
}
New-Item -ItemType Directory -Path $deployDir -Force | Out-Null

# ---------------------------------------------------------------------------
# 3. 编译（使用真实存在的 CMake 预设）
# ---------------------------------------------------------------------------
if (-not $SkipBuild) {
    Write-Host "[3/8] 编译主程序（preset: $BuildPreset）..." -ForegroundColor Yellow
    $env:PATH = "$QtRoot\bin;$env:PATH"

    # 允许通过环境变量向 CMake 传入 Python 环境位置（与 FindPythonEnv.cmake 对齐）
    if (-not $env:LABELTORCH_PYTHON_ENV) { $env:LABELTORCH_PYTHON_ENV = $PythonEnv }
    if (-not $env:QT_ROOT) { $env:QT_ROOT = $QtRoot }

    Push-Location $rootDir
    try {
        cmake --preset $BuildPreset
        if ($LASTEXITCODE -ne 0) { throw "cmake --preset $BuildPreset 配置失败" }
        cmake --build --preset $BuildPreset
        if ($LASTEXITCODE -ne 0) { throw "cmake --build --preset $BuildPreset 编译失败" }
    } finally {
        Pop-Location
    }
} else {
    Write-Host "[3/8] 跳过编译，使用既有构建产物" -ForegroundColor Yellow
}

$exePath = Join-Path $buildDir "LabelTorchV.exe"
if (-not (Test-Path -LiteralPath $exePath)) {
    throw "构建产物不存在：$exePath"
}

# ---------------------------------------------------------------------------
# 4. 部署主程序、QML 模块，运行 windeployqt
# ---------------------------------------------------------------------------
Write-Host "[4/8] 部署主程序与 Qt 运行库..." -ForegroundColor Yellow
Copy-Item -LiteralPath $exePath -Destination $deployDir

# QML 模块产物（QTP0004 输出目录），缺失不阻断（资源内嵌时可省略）
$qmlDir = Join-Path $buildDir "LabelTorch"
if (Test-Path -LiteralPath $qmlDir) {
    New-Item -ItemType Directory -Path (Join-Path $deployDir "LabelTorch") -Force | Out-Null
    Copy-Item -Path (Join-Path $qmlDir "*") -Destination (Join-Path $deployDir "LabelTorch") -Recurse -Force
    Write-Host "      已拷贝 QML 模块目录"
} else {
    Write-Host "      未发现外部 QML 模块目录（若 QML 已编入资源可忽略）" -ForegroundColor DarkGray
}

$windeployqt = Join-Path $QtRoot "bin\windeployqt.exe"
if (-not (Test-Path -LiteralPath $windeployqt)) { throw "windeployqt 不存在：$windeployqt" }

# --qmldir 让 qmlimportscanner 识别工程 QML 依赖，补齐 QtQuick 等插件
& $windeployqt (Join-Path $deployDir "LabelTorchV.exe") `
    --release --no-translations --no-opengl-sw `
    --qmldir (Join-Path $rootDir "src")
if ($LASTEXITCODE -ne 0) { throw "windeployqt 失败（exit=$LASTEXITCODE）" }

# ---------------------------------------------------------------------------
# 5. 拷贝 Python 运行时（含 Library/bin DLL 与 python311.dll 依赖链）
# ---------------------------------------------------------------------------
Write-Host "[5/8] 打包 Python 运行时（$PythonMode）..." -ForegroundColor Yellow
$pythonDest = Join-Path $deployDir "python"
New-Item -ItemType Directory -Path $pythonDest -Force | Out-Null

function Copy-EnvFile {
    param([string]$Name, [string]$DstDir = $pythonDest)
    $src = Join-Path $PythonEnv $Name
    if (-not (Test-Path -LiteralPath $src)) {
        throw "Python 环境缺少关键文件：$src"
    }
    Copy-Item -LiteralPath $src -Destination $DstDir -Force
}

# --- 5.1 解释器与 python311.dll 依赖链（位于 conda 环境根目录） ---
# conda 发行版把 python.exe / python311.dll / vcruntime / api-ms-win-* 放在环境根，
# 而非 Library/bin；整条依赖链必须随解释器一起落到 python/ 目录，
# 保证主程序以 applicationDirPath()/python/python.exe 启动后端时 DLL 搜索可命中。
foreach ($f in @(
        "python.exe", "pythonw.exe",
        "python3.dll", "python311.dll",
        "vcruntime140.dll", "vcruntime140_1.dll", "vcruntime140_threads.dll",
        "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
        "msvcp140_atomic_wait.dll", "msvcp140_codecvt_ids.dll",
        "concrt140.dll", "vccorlib140.dll", "vcomp140.dll", "vcamp140.dll",
        "ucrtbase.dll", "zlib.dll"
    )) {
    Copy-EnvFile -Name $f
}

# api-ms-win-* 通用 CRT 转发桩
Get-ChildItem -LiteralPath $PythonEnv -Filter "api-ms-win-*.dll" -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $pythonDest -Force
}

# --- 5.2 Library/bin 运行时 DLL（ssl/bz2/lzma/sqlite/expat/ffi 等） ---
# 这些 DLL 不在 python/ 根目录，_ssl、_bz2、_lzma、_sqlite3、pyexpat 等扩展模块
# 靠 PATH 或 add_dll_directory 定位。绿色包不跑 conda activate，因此：
#   a) 精简后复制到 python/ 根（与 python.exe 同级，DLL 搜索第一站）
#   b) 同时在启动器 PATH 中加入 python\Library\bin 兜底
$libBin = Join-Path $PythonEnv "Library\bin"
if (-not (Test-Path -LiteralPath $libBin)) { throw "Python 环境缺少 Library\bin：$libBin" }

# 必要子集：解释器标准库扩展真正依赖的 DLL。tcl/tk/wish 等 GUI 工具链不需要。
$dllKeep = @(
    "libbz2.dll", "libcrypto-3-x64.dll", "libssl-3-x64.dll",
    "libexpat.dll", "liblzma.dll", "ffi-8.dll",
    "sqlite3.dll", "zlib.dll", "zlib1.dll"
)
foreach ($d in $dllKeep) {
    $src = Join-Path $libBin $d
    if (Test-Path -LiteralPath $src) {
        Copy-Item -LiteralPath $src -Destination $pythonDest -Force
    } else {
        Write-Host "      提示：Library\bin 未找到 $d（不同发行版可能命名不同）" -ForegroundColor DarkGray
    }
}

# 完整 Library\bin 子集目录也保留一份，供启动器 PATH 兜底使用
$libBinDest = Join-Path $pythonDest "Library\bin"
New-Item -ItemType Directory -Path $libBinDest -Force | Out-Null
foreach ($d in $dllKeep) {
    $src = Join-Path $libBin $d
    if (Test-Path -LiteralPath $src) {
        Copy-Item -LiteralPath $src -Destination $libBinDest -Force
    }
}

# --- 5.3 标准库（Lib 下除 site-packages 的部分） ---
$srcLib = Join-Path $PythonEnv "Lib"
$dstLib = Join-Path $pythonDest "Lib"
New-Item -ItemType Directory -Path $dstLib -Force | Out-Null
Get-ChildItem -LiteralPath $srcLib | Where-Object { $_.Name -ne "site-packages" } | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $dstLib -Recurse -Force
}

# --- 5.4 DLLs 目录（标准库 .pyd 扩展） ---
$srcDlls = Join-Path $PythonEnv "DLLs"
if (Test-Path -LiteralPath $srcDlls) {
    Copy-Item -LiteralPath $srcDlls -Destination $pythonDest -Recurse -Force
}

# --- 5.5 site-packages：按模式拷贝 ---
$srcSite = Join-Path $PythonEnv "Lib\site-packages"
$dstSite = Join-Path $dstLib "site-packages"
New-Item -ItemType Directory -Path $dstSite -Force | Out-Null

if ($PythonMode -eq "full") {
    Write-Host "      完整拷贝 site-packages（约 2.2 GB）..." -ForegroundColor DarkGray
    Copy-Item -Path (Join-Path $srcSite "*") -Destination $dstSite -Recurse -Force
}
elseif ($PythonMode -eq "conda-pack") {
    # conda-pack 保留整个环境的前缀结构，解包后即为可搬迁环境
    Write-Host "      调用 conda-pack..." -ForegroundColor DarkGray
    $packOut = Join-Path $deployRoot "_conda-pack\labeltorch.tar.gz"
    New-Item -ItemType Directory -Path (Split-Path $packOut) -Force | Out-Null
    conda pack -p $PythonEnv -o $packOut --compress-level 3
    if ($LASTEXITCODE -ne 0) { throw "conda-pack 失败。可改用 -PythonMode trim" }
    $packExtract = Join-Path $pythonDest "_packed"
    New-Item -ItemType Directory -Path $packExtract -Force | Out-Null
    tar -xf $packOut -C $packExtract
    if ($LASTEXITCODE -ne 0) { throw "解压 conda-pack 归档失败" }
    # 归档内容即环境根，覆盖到 pythonDest
    Copy-Item -Path (Join-Path $packExtract "*") -Destination $pythonDest -Recurse -Force
    Remove-Item -LiteralPath $packExtract -Recurse -Force
}
else {
    # --- trim 模式：按白名单保留 ultralytics / torch / onnx 运行时依赖链 ---
    # 白名单来源：后端全链路 import 追踪（ultralytics+YOLO+torch+onnx/onnxruntime
    # +cv2+PIL+numpy+yaml+supervision）+ ultralytics 惰性依赖（pandas/polars/
    # requests/psutil/thop/lap）+ torch 公共依赖（sympy/networkx/jinja2 等）。
    Write-Host "      拷贝运行时依赖链（白名单）..." -ForegroundColor DarkGray

    $keepPackages = @(
        # --- 推理/训练主链 ---
        "ultralytics", "ultralytics_thop", "torch", "torchgen", "torchvision",
        "onnx", "onnxruntime", "cv2", "PIL", "numpy", "numpy.libs",
        # --- ultralytics 直接/惰性依赖 ---
        "supervision", "yaml", "pandas", "pandas.libs", "polars", "_polars_runtime_32",
        "matplotlib", "mpl_toolkits", "contourpy", "cycler", "kiwisolver",
        "fontTools", "pyparsing", "scipy", "scipy.libs",
        "requests", "urllib3", "certifi", "idna", "charset_normalizer",
        "tqdm", "psutil", "thop", "lap",
        # --- torch / onnx 公共依赖 ---
        "sympy", "mpmath", "networkx", "jinja2", "markupsafe",
        "filelock", "fsspec", "typing_extensions", "mpmath",
        "google", "protobuf", "ml_dtypes", "flatbuffers",
        "packaging", "six", "dateutil", "python_dateutil",
        "defusedxml", "deprecate", "dill", "colorama"
    )

    if ($IncludeAnomaly) {
        # anomalib 异常检测链（可选；体积显著增加）
        $keepPackages += @(
            "anomalib", "kornia", "kornia_rs", "timm", "lightning",
            "lightning_fabric", "lightning_utilities", "pytorch_lightning",
            "torchmetrics", "einops", "omegaconf", "jsonargparse",
            "rich", "markdown_it", "mdurl", "pygments", "FrEIA", "freia",
            "sklearn", "scikit_learn", "joblib", "threadpoolctl",
            "huggingface_hub", "hf_xet", "safetensors", "typer", "shellingham",
            "annotated_doc", "attr", "attrs", "anyio", "h11", "httpcore", "httpx"
        )
    }

    $keepPackages = $keepPackages | Select-Object -Unique

    foreach ($pkg in $keepPackages) {
        # 包形态可能是目录（torch/）、单文件模块（typing_extensions.py）或仅 dist-info
        $srcDir = Join-Path $srcSite $pkg
        $srcFile = Join-Path $srcSite "$pkg.py"
        if (Test-Path -LiteralPath $srcDir) {
            Copy-Item -LiteralPath $srcDir -Destination $dstSite -Recurse -Force
        } elseif (Test-Path -LiteralPath $srcFile) {
            Copy-Item -LiteralPath $srcFile -Destination $dstSite -Force
        } else {
            Write-Host "      提示：site-packages 未找到 $pkg" -ForegroundColor DarkGray
        }
    }

    # dist-info 用于 importlib.metadata 版本查询（ultralytics 等会读）
    foreach ($pkg in $keepPackages) {
        $pattern = "$pkg-*.dist-info"
        Get-ChildItem -LiteralPath $srcSite -Directory -Filter $pattern -ErrorAction SilentlyContinue | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $dstSite -Recurse -Force
        }
        # 连字符包名对应下划线目录时的 dist-info
        $alt = $pkg.Replace("_", "-")
        if ($alt -ne $pkg) {
            Get-ChildItem -LiteralPath $srcSite -Directory -Filter "$alt-*.dist-info" -ErrorAction SilentlyContinue | ForEach-Object {
                Copy-Item -LiteralPath $_.FullName -Destination $dstSite -Recurse -Force
            }
        }
    }
}

# ---------------------------------------------------------------------------
# 6. 拷贝后端源码，生成启动器与说明文件
# ---------------------------------------------------------------------------
Write-Host "[6/8] 部署后端与启动器..." -ForegroundColor Yellow
$backendDest = Join-Path $deployDir "backend"
New-Item -ItemType Directory -Path $backendDest -Force | Out-Null
Copy-Item -Path (Join-Path $rootDir "backend\labeltorch_backend") -Destination $backendDest -Recurse -Force

# 后端 requirements 一并放入，便于现场排障时核对依赖
$reqSrc = Join-Path $rootDir "backend\requirements.txt"
if (Test-Path -LiteralPath $reqSrc) {
    Copy-Item -LiteralPath $reqSrc -Destination (Join-Path $backendDest "requirements.txt") -Force
}

# 启动器：配置内嵌 Python 与 Qt 搜索路径后拉起主程序
$launcherContent = @"
@echo off
rem 标炬 v$Version 绿色版启动脚本
rem 设置内嵌 Python 与运行库搜索路径后启动主程序
set "BASE=%~dp0"
set "PATH=%BASE%;%BASE%python;%BASE%python\DLLs;%BASE%python\Library\bin;%PATH%"
set "PYTHONPATH=%BASE%backend"
set "PYTHONHOME=%BASE%python"
set "PYTHONNOUSERSITE=1"
start "" "%BASE%LabelTorchV.exe"
"@
Set-Content -Path (Join-Path $deployDir "start_LabelTorchV.bat") -Value $launcherContent -Encoding ASCII

$readmeContent = @"
标炬 LabelTorch v$Version（绿色包）
工业缺陷检测软件 — 解压即用，无需安装

快速开始
--------
1. 解压本压缩包到任意目录（路径建议不含特殊符号）
2. 双击 start_LabelTorchV.bat 启动
   （也可直接运行 LabelTorchV.exe，但启动器会先配好内嵌 Python 搜索路径，推荐用 bat）
3. 首次使用：创建项目 → 导入数据集（图片 + YOLO txt 标签）→ 训练 → 导出模型

目录结构
--------
LabelTorchV.exe   主程序
LabelTorch\       QML 模块（如存在）
python\           内嵌 Python 运行时（已按运行时依赖链裁剪）
backend\          Python 后端（训练/推理/导出服务）
start_LabelTorchV.bat  启动脚本

运行依赖
--------
- Windows 10/11 x64
- 训练 GPU 场景需 NVIDIA 驱动（本包内嵌 CPU 版 PyTorch；GPU 版请替换 python\torch）
- 无需预装 Python / conda / Qt

注意事项
--------
- 项目数据保存在用户数据目录，卸载/删除绿色包不会清除已有项目
- 界面中可在设置里指定外部 Python 路径，默认使用内嵌 python\python.exe
"@
Set-Content -Path (Join-Path $deployDir "README.txt") -Value $readmeContent -Encoding UTF8

# ---------------------------------------------------------------------------
# 7. 打包后冒烟校验
# ---------------------------------------------------------------------------
if (-not $SkipSmoke) {
    Write-Host "[7/8] 打包后冒烟校验..." -ForegroundColor Yellow

    # --- 7.1 关键文件清单 ---
    $manifest = @(
        "LabelTorchV.exe",
        "start_LabelTorchV.bat",
        "README.txt",
        "python\python.exe",
        "python\python311.dll",
        "python\vcruntime140.dll",
        "python\Library\bin\libssl-3-x64.dll",
        "python\Library\bin\libcrypto-3-x64.dll",
        "python\Library\bin\sqlite3.dll",
        "python\DLLs\_ssl.pyd",
        "python\DLLs\_bz2.pyd",
        "python\DLLs\_lzma.pyd",
        "python\DLLs\_sqlite3.pyd",
        "python\Lib\site-packages\torch\__init__.py",
        "python\Lib\site-packages\ultralytics\__init__.py",
        "python\Lib\site-packages\onnxruntime\__init__.py",
        "python\Lib\site-packages\cv2\__init__.py",
        "backend\labeltorch_backend\server.py",
        "backend\labeltorch_backend\handlers\training.py"
    )
    # Qt 运行库清单
    $manifest += @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Qml.dll", "Qt6Quick.dll", "platforms\qwindows.dll")

    $missing = @()
    foreach ($rel in $manifest) {
        if (-not (Test-Path -LiteralPath (Join-Path $deployDir $rel))) {
            $missing += $rel
        }
    }
    if ($missing.Count -gt 0) {
        Write-Host "冒烟失败，缺少关键文件：" -ForegroundColor Red
        $missing | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
        throw "绿色包文件清单校验未通过"
    }
    Write-Host "      文件清单校验通过（$($manifest.Count) 项）" -ForegroundColor Green

    # --- 7.2 打包内 Python 关键模块 import 自检 ---
    $smokePy = @"
import sys
sys.path.insert(0, r'$($backendDest -replace '\\', '/')')
mods = [
    'ultralytics', 'torch', 'onnx', 'onnxruntime',
    'cv2', 'PIL', 'numpy', 'yaml', 'supervision',
    'labeltorch_backend.server',
    'labeltorch_backend.adapters.registry',
]
for m in mods:
    __import__(m)
print('SMOKE_PY_OK')
"@
    $smokePyPath = Join-Path $deployDir "_smoke_import.py"
    Set-Content -Path $smokePyPath -Value $smokePy -Encoding UTF8

    $oldPath = $env:PATH
    $oldPyPath = $env:PYTHONPATH
    $oldPyHome = $env:PYTHONHOME
    try {
        $env:PATH = "$deployDir;$pythonDest;$(Join-Path $pythonDest 'DLLs');$(Join-Path $pythonDest 'Library\bin');$oldPath"
        $env:PYTHONPATH = $backendDest
        $env:PYTHONHOME = $pythonDest
        $env:PYTHONNOUSERSITE = "1"

        $pyExe = Join-Path $pythonDest "python.exe"
        $pyOut = & $pyExe $smokePyPath 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0 -or $pyOut -notmatch "SMOKE_PY_OK") {
            throw "打包内 Python 导入自检失败：$pyOut"
        }
        Write-Host "      Python 依赖链导入自检通过" -ForegroundColor Green
    } finally {
        $env:PATH = $oldPath
        $env:PYTHONPATH = $oldPyPath
        if ($null -eq $oldPyHome) { Remove-Item Env:PYTHONHOME -ErrorAction SilentlyContinue }
        else { $env:PYTHONHOME = $oldPyHome }
    }
    Remove-Item -LiteralPath $smokePyPath -Force -ErrorAction SilentlyContinue

    # --- 7.3 启动 exe 冒烟（存活检测；缺少 Qt DLL 会秒退） ---
    Write-Host "      启动 LabelTorchV.exe 冒烟..." -ForegroundColor DarkGray
    $proc = Start-Process -FilePath (Join-Path $deployDir "LabelTorchV.exe") `
        -WorkingDirectory $deployDir -PassThru
    Start-Sleep -Seconds 8
    if ($proc.HasExited) {
        $code = $proc.ExitCode
        throw "LabelTorchV.exe 启动后立即退出（exit=$code）。常见原因：缺少 Qt 插件 / QML 模块 / Python 后端。"
    }
    Write-Host "      主程序启动冒烟通过（进程 PID=$($proc.Id)）" -ForegroundColor Green
    try { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue } catch { }
    # 等待进程树退出，避免残留后端 python
    Start-Sleep -Seconds 2
} else {
    Write-Host "[7/8] 跳过冒烟校验" -ForegroundColor Yellow
}

# ---------------------------------------------------------------------------
# 8. 压缩 zip
# ---------------------------------------------------------------------------
if (-not $SkipZip) {
    Write-Host "[8/8] 压缩发布包..." -ForegroundColor Yellow
    $zipPath = Join-Path $deployRoot "$packageName.zip"
    if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
    Compress-Archive -Path $deployDir -DestinationPath $zipPath -CompressionLevel Optimal
    $zipMb = [math]::Round((Get-Item -LiteralPath $zipPath).Length / 1MB, 1)
    Write-Host "=== 绿色包已生成：deploy\$packageName.zip（${zipMb} MB）===" -ForegroundColor Green
} else {
    Write-Host "[8/8] 跳过 zip 压缩" -ForegroundColor Yellow
    Write-Host "=== 绿色包目录已生成：$deployDir ===" -ForegroundColor Green
}
