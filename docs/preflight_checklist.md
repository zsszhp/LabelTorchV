# 标炬（LabelTorch）开发前自检清单与避坑指南 (Pre-flight Checklist)

> 版本：1.0 | 更新日期：2026-05-30
> 状态：开发启动前必读指南

在正式让 AI 动手编写代码前，为了避免在“环境搭建”、“多进程通信”、“数据写损坏”及“显卡假死”上浪费时间，您需要依次确认并执行本指南中的步骤。

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

---

## 二、 完整的 SQLite 数据库初始化 DDL 脚本

为确保 React/Electron 主进程启动时能够自动建立正确的数据库，这里提供完整的 SQLite SQL 建表脚本。您可以让 AI 直接将其读取并封装入数据库初始化逻辑中。

```sql
-- 开启外键约束与 WAL 模式提高读写稳定性
PRAGMA foreign_keys = ON;

-- 1. 项目表
CREATE TABLE IF NOT EXISTS projects (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    root_path TEXT NOT NULL,
    task_type TEXT NOT NULL CHECK(task_type IN ('detect', 'obb', 'classify', 'anomaly')),
    default_device TEXT DEFAULT 'cuda:0',
    default_model_family TEXT DEFAULT 'YOLOv8',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 2. 类别体系表
CREATE TABLE IF NOT EXISTS taxonomies (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL,
    name TEXT NOT NULL,
    version INTEGER NOT NULL DEFAULT 1,
    class_definitions_json TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE
);

-- 3. 数据集元信息表
CREATE TABLE IF NOT EXISTS datasets (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL,
    name TEXT NOT NULL,
    image_root TEXT NOT NULL,
    label_root TEXT NOT NULL,
    format TEXT DEFAULT 'YOLO',
    sample_count INTEGER DEFAULT 0,
    import_status TEXT DEFAULT 'pending',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE
);

-- 4. 样本表
CREATE TABLE IF NOT EXISTS dataset_samples (
    id TEXT PRIMARY KEY,
    dataset_id TEXT NOT NULL,
    image_path TEXT NOT NULL,
    label_path TEXT,
    width INTEGER NOT NULL,
    height INTEGER NOT NULL,
    hash TEXT,
    validation_status TEXT DEFAULT 'unverified',
    split TEXT DEFAULT 'train' CHECK(split IN ('train', 'val', 'test')),
    FOREIGN KEY(dataset_id) REFERENCES datasets(id) ON DELETE CASCADE
);

-- 5. 数据快照表（保证训练环境不变性）
CREATE TABLE IF NOT EXISTS dataset_snapshots (
    id TEXT PRIMARY KEY,
    dataset_id TEXT NOT NULL,
    sample_manifest_json TEXT NOT NULL,
    split_manifest_json TEXT NOT NULL,
    taxonomy_version INTEGER NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(dataset_id) REFERENCES datasets(id) ON DELETE CASCADE
);

-- 6. 训练任务表
CREATE TABLE IF NOT EXISTS training_runs (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL,
    snapshot_id TEXT NOT NULL,
    config_snapshot_json TEXT NOT NULL,
    status TEXT NOT NULL CHECK(status IN ('draft', 'running', 'succeeded', 'failed', 'stopped')),
    log_uri TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE,
    FOREIGN KEY(snapshot_id) REFERENCES dataset_snapshots(id)
);

-- 7. 模型版本表
CREATE TABLE IF NOT EXISTS model_versions (
    id TEXT PRIMARY KEY,
    run_id TEXT NOT NULL,
    parent_model_version_id TEXT,
    best_weight_path TEXT NOT NULL,
    last_weight_path TEXT,
    metrics_snapshot_json TEXT,
    tag TEXT CHECK(tag IN ('baseline', 'best-so-far', 'production-candidate', 'archived')),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(run_id) REFERENCES training_runs(id) ON DELETE CASCADE
);

-- 8. 标注修改历史审计表 (实现 Undo/Redo)
CREATE TABLE IF NOT EXISTS annotation_revisions (
    id TEXT PRIMARY KEY,
    dataset_id TEXT NOT NULL,
    sample_id TEXT NOT NULL,
    source_type TEXT NOT NULL CHECK(source_type IN ('manual', 'assisted', 'imported')),
    before_snapshot_json TEXT,
    after_snapshot_json TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(dataset_id) REFERENCES datasets(id) ON DELETE CASCADE,
    FOREIGN KEY(sample_id) REFERENCES dataset_samples(id) ON DELETE CASCADE
);
```

---

## 三、 React + Electron 桌面端项目初始化步骤

在终端中按顺序执行以下命令以创建您的前端项目骨架。

```powershell
# 1. 在 LabelTorchV 项目根目录下初始化 Vite + React + TypeScript 项目
npm create vite@latest frontend -- --template react-ts

# 2. 进入前端目录
cd frontend

# 3. 安装 Tailwind CSS 及其辅助依赖
npm install -D tailwindcss postcss autoprefixer
npx tailwindcss init -p

# 4. 安装 Shadcn UI / Ant Design 组件库
npm install antd @ant-design/icons
npm install lucide-react   # 现代轻量图标库

# 5. 安装 Canvas 核心引擎
npm install fabric --save  # 或者 npm install konva react-konva

# 6. 安装 Electron 依赖以实现桌面壳体封装
npm install -D electron electron-builder wait-on concurrently
```

---

## 四、 工业级开发与部署“避坑指南” (Gotchas & Mitigations)

### 1. Windows 子进程管道阻塞问题 (Stdout Buffering)
* **痛点**：Node.js 拉起 Python 后，如果 Python 使用普通的 `print()`，且输出量很大，标准输出缓冲区（Stdout Buffer）满了之后，**Python 进程会直接假死挂起**。
* **规避方案**：
  * 启动 Python 子进程时，必须设置环境变量 `PYTHONUNBUFFERED=1`。
  * Node.js 端读取 `stdout` 时，必须使用 `readline` 逐行读取，立即释放缓冲区，不能积压。
  * Python 中禁止使用阻断式 IO 打印日志，一律使用异步协程将事件推送至队列。

### 2. Windows 显卡假死机制 (TDR - Timeout Detection and Recovery)
* **痛点**：在 Windows 上，如果 GPU 连续执行计算任务超过 2 秒且没有响应操作系统的图形渲染请求，Windows 驱动会强制重置显卡，导致 **PyTorch 直接报 CUDA Error 崩溃退出**。
* **规避方案**：
  * 在训练启动脚本中，控制 `workers` 数量不宜过高（Windows 上通常设为 `0` 或 `2`）。
  * 确保 PyTorch 训练过程不会锁定主渲染通道。

### 3. 便携式打包路径问题
* **痛点**：用户解压软件后的物理路径可能包含中文、空格或深度嵌套，导致 Python `subprocess` 或 PyTorch 的 C++ 动态库加载失败。
* **规避方案**：
  * Electron 主程序在解压启动后，第一时间检测自身绝对路径，检查是否存在中文字符。如果存在，弹窗友好提示用户“请将本绿色免安装包移动到纯英文、无空格的目录下运行”。
  * 数据库的存储路径应统一采用系统的临时本地应用区 `process.env.APPDATA`，不与安装包物理路径绑定，防止用户因覆盖升级导致数据丢失。
