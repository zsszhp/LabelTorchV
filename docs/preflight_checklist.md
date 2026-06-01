# 标炬（LabelTorch）开发前自检清单与避坑指南 (Pre-flight Checklist)

> 版本：2.0 | 更新日期：2026-05-31
> 状态：开发启动前必读指南（基于 Qt 6 + C++17 + QML + Python）

在正式开发具体功能前，为了避免在"环境搭建"、"多进程通信"、"数据写损坏"及"显卡假死"上浪费时间，您需要依次确认并执行本指南中的步骤。

---

## 一、 本地硬件与环境诊断 (诊断命令)

在开发前，请在您的 Windows CMD/PowerShell 中运行以下命令，验证您的显卡和 Conda 虚拟环境。

### 1. NVIDIA 显卡与驱动自检
打开命令行，输入：
```powershell
nvidia-smi
```
* **确认项**：
  * 右上角 `CUDA Version` 必须 $\ge 12.1$。
  * 记录您显卡的可用显存（如 `Driver Version: 535.xx`, `VRAM: 12GB`），这决定了您的默认 Batch Size 推荐范围。

### 2. Conda 虚拟环境自检
激活您的 Conda 环境并检查依赖包：
```powershell
# 激活环境
conda activate labeltorch

# 确认 Python 版本 (应为 3.11.x)
python --version

# 验证 PyTorch 是否成功加载 CUDA 算力
python -c "import torch; print('CUDA可用性:', torch.cuda.is_available()); print('GPU设备名:', torch.cuda.get_device_name(0) if torch.cuda.is_available() else '无')"

# 验证 Ultralytics 库
python -c "import ultralytics; ultralytics.checks()"
```
> [!WARNING]
> **显卡不匹配警告**：如果 `torch.cuda.is_available()` 返回 `False`，说明安装了 CPU 版本的 PyTorch。您必须卸载并重新安装支持 CUDA 12.1 的 PyTorch：
> `pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121`

### 3. Qt 开发环境自检
```powershell
# 确认 Qt 安装
# Qt 安装路径：C:/Qt/6.11.1/msvc2022_64
# Ninja 路径：C:/Qt/Tools/Ninja

# 确认 MSVC 编译器
cl.exe
# 应输出 Microsoft (R) C/C++ Optimizing Compiler Version 19.x

# 确认 CMake
cmake --version
# 应 >= 3.22
```

---

## 二、 完整的 SQLite 数据库初始化 DDL 脚本

数据库初始化由 `Database::initializeSchema()` 自动执行，DDL 定义在 `src/core/database/Schema.cpp` 的 `Schema::createTableStatements()` 中。以下为完整 DDL 参考（14 张核心表）：

```sql
-- 开启外键约束与 WAL 模式提高读写稳定性
PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;

-- 1. 项目表
CREATE TABLE IF NOT EXISTS projects (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    root_path TEXT NOT NULL UNIQUE,
    default_device TEXT DEFAULT 'auto',
    default_model_family TEXT DEFAULT 'yolov8',
    task_type TEXT DEFAULT 'detect',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 2. 类别体系表
CREATE TABLE IF NOT EXISTS taxonomies (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    version INTEGER NOT NULL DEFAULT 1,
    class_definitions_json TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 3. 数据集元信息表
CREATE TABLE IF NOT EXISTS datasets (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    image_root TEXT NOT NULL,
    label_root TEXT NOT NULL,
    format TEXT NOT NULL DEFAULT 'yolo_txt',
    sample_count INTEGER DEFAULT 0,
    import_status TEXT NOT NULL DEFAULT 'idle',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 4. 样本表
CREATE TABLE IF NOT EXISTS dataset_samples (
    id TEXT PRIMARY KEY,
    dataset_id TEXT NOT NULL REFERENCES datasets(id) ON DELETE CASCADE,
    image_path TEXT NOT NULL,
    label_path TEXT,
    width INTEGER,
    height INTEGER,
    hash TEXT,
    validation_status TEXT DEFAULT 'valid',
    split TEXT DEFAULT 'train',
    error_code TEXT
);

-- 5. 导入标签类别表
CREATE TABLE IF NOT EXISTS imported_label_schemas (
    id TEXT PRIMARY KEY,
    dataset_id TEXT NOT NULL REFERENCES datasets(id) ON DELETE CASCADE,
    raw_class_names_json TEXT NOT NULL,
    raw_class_order_json TEXT NOT NULL,
    source_format TEXT NOT NULL DEFAULT 'yolo_txt'
);

-- 6. 类别映射修订表
CREATE TABLE IF NOT EXISTS class_mapping_revisions (
    id TEXT PRIMARY KEY,
    dataset_id TEXT NOT NULL REFERENCES datasets(id) ON DELETE CASCADE,
    source_schema_id TEXT NOT NULL REFERENCES imported_label_schemas(id),
    target_taxonomy_id TEXT NOT NULL REFERENCES taxonomies(id),
    mapping_rules_json TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 7. 标注修订表 (实现 Undo/Redo)
CREATE TABLE IF NOT EXISTS annotation_revisions (
    id TEXT PRIMARY KEY,
    dataset_id TEXT NOT NULL REFERENCES datasets(id) ON DELETE CASCADE,
    sample_id TEXT NOT NULL REFERENCES dataset_samples(id) ON DELETE CASCADE,
    source_type TEXT NOT NULL DEFAULT 'manual',
    before_snapshot_json TEXT,
    after_snapshot_json TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 8. 数据快照表（保证训练环境不变性）
CREATE TABLE IF NOT EXISTS dataset_snapshots (
    id TEXT PRIMARY KEY,
    dataset_id TEXT NOT NULL REFERENCES datasets(id) ON DELETE CASCADE,
    sample_manifest_json TEXT NOT NULL,
    split_manifest_json TEXT,
    taxonomy_version TEXT,
    annotation_revision_boundary TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 9. 训练任务表
CREATE TABLE IF NOT EXISTS training_runs (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    snapshot_id TEXT NOT NULL REFERENCES dataset_snapshots(id),
    config_snapshot_json TEXT NOT NULL,
    runtime_env_snapshot_json TEXT,
    status TEXT NOT NULL DEFAULT 'draft',
    log_uri TEXT,
    started_at DATETIME,
    finished_at DATETIME
);

-- 10. 模型版本表
CREATE TABLE IF NOT EXISTS model_versions (
    id TEXT PRIMARY KEY,
    run_id TEXT NOT NULL REFERENCES training_runs(id) ON DELETE CASCADE,
    parent_model_version_id TEXT REFERENCES model_versions(id),
    best_weight_path TEXT,
    last_weight_path TEXT,
    metrics_snapshot_json TEXT,
    tag TEXT,
    export_registry_json TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 11. 辅助标注批次表
CREATE TABLE IF NOT EXISTS assisted_label_batches (
    id TEXT PRIMARY KEY,
    model_version_id TEXT NOT NULL REFERENCES model_versions(id),
    dataset_id TEXT NOT NULL REFERENCES datasets(id),
    target_sample_scope TEXT NOT NULL,
    conf_threshold REAL NOT NULL DEFAULT 0.25,
    iou_threshold REAL NOT NULL DEFAULT 0.45,
    candidate_snapshot_json TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 12. 导出产物表
CREATE TABLE IF NOT EXISTS export_artifacts (
    id TEXT PRIMARY KEY,
    model_version_id TEXT NOT NULL REFERENCES model_versions(id),
    format TEXT NOT NULL,
    options_snapshot_json TEXT,
    output_path TEXT NOT NULL,
    validation_result TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 13. 任务事件表
CREATE TABLE IF NOT EXISTS task_events (
    id TEXT PRIMARY KEY,
    task_type TEXT NOT NULL,
    task_id TEXT NOT NULL,
    event_type TEXT NOT NULL,
    payload_json TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 14. 训练指标表（每 epoch 记录）
CREATE TABLE IF NOT EXISTS run_metrics (
    id TEXT PRIMARY KEY,
    run_id TEXT NOT NULL REFERENCES training_runs(id) ON DELETE CASCADE,
    epoch INTEGER NOT NULL,
    metric_name TEXT NOT NULL,
    metric_value REAL NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 推荐索引（提升查询性能）
CREATE INDEX IF NOT EXISTS idx_dataset_samples_dataset_id ON dataset_samples(dataset_id);
CREATE INDEX IF NOT EXISTS idx_training_runs_project_id ON training_runs(project_id);
CREATE INDEX IF NOT EXISTS idx_run_metrics_run_id ON run_metrics(run_id);
CREATE INDEX IF NOT EXISTS idx_model_versions_run_id ON model_versions(run_id);
```

---

## 三、 Qt 6 + CMake 构建配置

### 1. 构建前提
确保以下工具已安装：
- Qt 6.11.1 (msvc2022_64)
- CMake 3.22+
- Ninja
- MSVC 2022 (v145 工具集)
- Windows SDK 10.0.26100.0

### 2. 构建命令
```powershell
# 使用 CMake Preset 构建（推荐）
cmake --preset msvc2022-release
cmake --build --preset msvc2022-release

# Debug 构建
cmake --preset msvc2022-debug
cmake --build --preset msvc2022-debug
```

构建输出目录：`out/build/msvc2022-release/` 或 `out/build/msvc2022-debug/`

### 3. 运行测试
```powershell
# C++ 测试
ctest --preset msvc2022-release

# Python 测试
cd backend && python -m pytest tests/

# Python Lint
cd backend && python -m ruff check .
```

### 4. 可用的 CMake Preset
| Preset 名称 | 编译器 | 构建类型 |
|-------------|--------|---------|
| `msvc2022-debug` | MSVC 2022 | Debug |
| `msvc2022-release` | MSVC 2022 | Release |
| `x64-debug` | MSVC x64 | Debug |
| `x64-release` | MSVC x64 | Release |
| `mingw-debug` | MinGW | Debug |
| `mingw-release` | MinGW | Release |

---

## 四、 工业级开发与部署"避坑指南" (Gotchas & Mitigations)

### 1. Windows 子进程管道阻塞问题 (Stdout Buffering)
* **痛点**：Qt 通过 `QProcess` 拉起 Python 后，如果 Python 使用普通的 `print()`，且输出量很大，标准输出缓冲区（Stdout Buffer）满了之后，**Python 进程会直接假死挂起**。
* **规避方案**：
  * `IpcClient` 启动 Python 子进程时，必须设置环境变量 `PYTHONUNBUFFERED=1`。
  * Qt 端通过 `QProcess::readyReadStandardOutput` 信号逐行读取，立即释放缓冲区，不能积压。
  * Python 中禁止使用阻断式 IO 打印日志，一律使用 `logging` 模块输出到 `stderr`。

### 2. Windows 显卡假死机制 (TDR - Timeout Detection and Recovery)
* **痛点**：在 Windows 上，如果 GPU 连续执行计算任务超过 2 秒且没有响应操作系统的图形渲染请求，Windows 驱动会强制重置显卡，导致 **PyTorch 直接报 CUDA Error 崩溃退出**。
* **规避方案**：
  * 在训练启动脚本中，控制 `workers` 数量不宜过高（Windows 上通常设为 `0` 或 `2`）。
  * 确保 PyTorch 训练过程不会锁定主渲染通道。

### 3. 便携式打包路径问题
* **痛点**：用户解压软件后的物理路径可能包含中文、空格或深度嵌套，导致 Python `subprocess` 或 PyTorch 的 C++ 动态库加载失败。
* **规避方案**：
  * Qt 主程序在启动后，第一时间检测自身绝对路径（`QCoreApplication::applicationDirPath()`），检查是否存在中文字符。如果存在，弹窗友好提示用户"请将本绿色免安装包移动到纯英文、无空格的目录下运行"。
  * 数据库的存储路径应统一采用 `QStandardPaths::AppDataLocation`，不与安装包物理路径绑定，防止用户因覆盖升级导致数据丢失。

### 4. SQLite 跨线程访问
* **痛点**：Qt 的 `QSqlDatabase` 连接不能跨线程使用。如果在 `QtConcurrent::run` 的后台线程中直接使用 `Database::instance().database()`，可能导致竞争条件或崩溃。
* **规避方案**：
  * 在后台线程中需要访问数据库时，必须创建独立的 `QSqlDatabase` 连接（使用不同的连接名）。
  * 或在主线程中提前取出所需数据，传递给后台线程，后台线程仅做 IO 密集操作。

### 5. Qt 6.11 NaN ASSERT 问题
* **痛点**：Qt 6.11 Debug 模式下 `qCheckedFPConversionToInteger` 检测到 NaN 会调用 `qFatal` 导致程序退出。NaN 来自 Qt Quick 布局引擎内部初始化竞态条件，不影响程序正常运行。
* **当前方案**：`main.cpp` 中已安装自定义消息处理器（`customMessageHandler`）和 MSVC CRT 报告钩子（`msvcReportHook`），将 NaN 相关的 Fatal 降级为 Warning。
