<#
.SYNOPSIS
    版本号单一源同步脚本。

.DESCRIPTION
    以 CMakeLists.txt 中 project(VERSION x.y.z) 为唯一权威来源，将版本号
    同步到安装器配置、Python 后端包元数据与文档。打包/发版前调用一次即可。

    被同步的文件：
      - installer/config/config.xml
      - installer/packages/com.labeltorch.main/meta/package.xml
      - backend/labeltorch_backend/__init__.py
      - README.md / README.en.md（版本徽标行）
      - CHANGELOG.md（校验是否已有对应章节，只告警不改写正文）

.PARAMETER Version
    指定版本号。缺省时从 CMakeLists.txt 解析。

.PARAMETER CheckOnly
    只校验各处版本是否一致，不写入（可用于 CI 门禁）。
#>
[CmdletBinding()]
param(
    [string]$Version = "",
    [switch]$CheckOnly
)

$ErrorActionPreference = "Stop"

$scriptPath = $PSScriptRoot
$rootDir = (Resolve-Path (Join-Path $scriptPath "..")).Path

# ---------------------------------------------------------------------------
# 1. 解析权威版本号（必须匹配 project(... VERSION x.y.z)，避免命中 cmake_minimum_required）
# ---------------------------------------------------------------------------
if (-not $Version) {
    $cmakeLists = Join-Path $rootDir "CMakeLists.txt"
    $m = Select-String -LiteralPath $cmakeLists -Pattern 'project\s*\([^)]*VERSION\s+(\d+\.\d+\.\d+)' | Select-Object -First 1
    if (-not $m) { throw "无法从 CMakeLists.txt 解析 project(VERSION x.y.z)" }
    $Version = $m.Matches[0].Groups[1].Value
}

Write-Host "版本单一源同步：目标版本 $Version"
if ($CheckOnly) { Write-Host "（CheckOnly 模式，仅校验不写入）" }

$drift = @()

function Sync-XmlVersion {
    param([string]$RelPath, [string]$Pattern, [string]$Replacement)
    $full = Join-Path $rootDir $RelPath
    if (-not (Test-Path -LiteralPath $full)) {
        Write-Host "  跳过（文件不存在）：$RelPath" -ForegroundColor DarkGray
        return
    }
    $text = [System.IO.File]::ReadAllText($full, [System.Text.Encoding]::UTF8)
    if ($text -notmatch $Pattern) {
        $script:drift += "$RelPath : 未找到版本节点"
        return
    }
    $newText = [regex]::Replace($text, $Pattern, $Replacement, 1)
    $current = [regex]::Match($text, $Pattern).Groups[1].Value
    if ($current -eq $Version) {
        Write-Host "  已一致：$RelPath = $current" -ForegroundColor Green
        return
    }
    $script:drift += "$RelPath : $current -> $Version"
    if (-not $CheckOnly) {
        # 原子写入：先写临时文件再替换，避免中途崩溃留下半截文件
        $tmp = "$full.tmp"
        [System.IO.File]::WriteAllText($tmp, $newText, [System.Text.UTF8Encoding]::new($true))
        Move-Item -LiteralPath $tmp -Destination $full -Force
        Write-Host "  已同步：$RelPath = $Version" -ForegroundColor Yellow
    }
}

function Sync-Regex {
    param([string]$RelPath, [string]$Pattern, [string]$Replacement)
    $full = Join-Path $rootDir $RelPath
    if (-not (Test-Path -LiteralPath $full)) {
        Write-Host "  跳过（文件不存在）：$RelPath" -ForegroundColor DarkGray
        return
    }
    $text = [System.IO.File]::ReadAllText($full, [System.Text.Encoding]::UTF8)
    $m = [regex]::Match($text, $Pattern)
    if (-not $m.Success) {
        $script:drift += "$RelPath : 未找到版本标记"
        return
    }
    $current = $m.Groups[1].Value
    if ($current -eq $Version) {
        Write-Host "  已一致：$RelPath = $current" -ForegroundColor Green
        return
    }
    $script:drift += "$RelPath : $current -> $Version"
    if (-not $CheckOnly) {
        $newText = [regex]::Replace($text, $Pattern, $Replacement, 1)
        $tmp = "$full.tmp"
        [System.IO.File]::WriteAllText($tmp, $newText, [System.Text.UTF8Encoding]::new($true))
        Move-Item -LiteralPath $tmp -Destination $full -Force
        Write-Host "  已同步：$RelPath = $Version" -ForegroundColor Yellow
    }
}

# ---------------------------------------------------------------------------
# 2. 逐个文件对齐
# ---------------------------------------------------------------------------
# 安装器全局配置
Sync-XmlVersion -RelPath "installer\config\config.xml" `
    -Pattern '<Version>([^<]+)</Version>' `
    -Replacement ('<Version>' + $Version + '</Version>')

# 安装包组件元数据
Sync-XmlVersion -RelPath "installer\packages\com.labeltorch.main\meta\package.xml" `
    -Pattern '<Version>([^<]+)</Version>' `
    -Replacement ('<Version>' + $Version + '</Version>')

# Python 后端包版本
Sync-Regex -RelPath "backend\labeltorch_backend\__init__.py" `
    -Pattern '__version__\s*=\s*"([^"]+)"' `
    -Replacement ('__version__ = "' + $Version + '"')

# README 版本徽标行（约定：**当前版本**: x.y.z）
Sync-Regex -RelPath "README.md" `
    -Pattern '\*\*当前版本\*\*:\s*([0-9]+\.[0-9]+\.[0-9]+)' `
    -Replacement ('**当前版本**: ' + $Version)
Sync-Regex -RelPath "README.en.md" `
    -Pattern '\*\*Current version\*\*:\s*([0-9]+\.[0-9]+\.[0-9]+)' `
    -Replacement ('**Current version**: ' + $Version)

# CHANGELOG 只校验章节存在性，不改写历史条目
$changelog = Join-Path $rootDir "CHANGELOG.md"
if (Test-Path -LiteralPath $changelog) {
    $cl = [System.IO.File]::ReadAllText($changelog, [System.Text.Encoding]::UTF8)
    if ($cl -notmatch [regex]::Escape("v$Version")) {
        Write-Host "  警告：CHANGELOG.md 缺少 v$Version 章节，请人工补充" -ForegroundColor Yellow
        $drift += "CHANGELOG.md : 缺少 v$Version 章节"
    } else {
        Write-Host "  已一致：CHANGELOG.md 含 v$Version 章节" -ForegroundColor Green
    }
}

# ---------------------------------------------------------------------------
# 3. 汇总
# ---------------------------------------------------------------------------
if ($drift.Count -gt 0) {
    Write-Host ""
    Write-Host "版本差异清单：" -ForegroundColor Yellow
    $drift | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
    if ($CheckOnly) {
        Write-Host "CheckOnly：存在版本漂移" -ForegroundColor Yellow
        exit 2
    }
} else {
    Write-Host "所有受管文件版本已一致" -ForegroundColor Green
}

Write-Host "版本同步完成（权威源：CMakeLists.txt project(VERSION)）"
exit 0
