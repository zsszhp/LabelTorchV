# 标炬（LabelTorch）MVP 产品需求文档

> 版本：v0.1.0 | 更新日期：2026-05-18

---

## 一、MVP 概述

### 产品目标

让用户能完成「创建项目 → 导入 YOLO 数据集 → 配置训练参数 → 训练模型 → 导出 pt/onnx」完整闭环，无需任何外部工具。

### MVP 边界

**包含**：
- 项目管理（创建/打开/删除）
- YOLO txt 数据集导入
- 数据集浏览与统计
- YOLOv8 训练（含早停）
- 模型版本管理
- pt/onnx 模型导出
- 绿色安装包

**不包含**：
- 标注功能（BBox/OBB/多边形绘制）
- 辅助标注（模型推理候选框）
- 增量训练
- 数据快照
- 类别映射重排
- 多模型家族（v5/v10/v11）
- 断点续训
- AMP
- OBB 训练

---

## 二、用户故事

### US-01 创建项目

**作为**工业算法工程师，**我希望**能快速创建一个项目来管理我的数据集和模型，**以便**将不同任务的数据和模型分开管理。

**验收标准**：
- 点击"新建项目"，输入名称和路径，项目目录自动创建
- 项目目录包含 data/、models/、exports/、cache/、logs/ 子目录
- 项目列表显示所有已创建项目
- 可打开/删除项目

### US-02 导入数据集

**作为**工业算法工程师，**我希望**能导入已有的 YOLO txt 格式数据集，**以便**直接开始训练而无需重新标注。

**验收标准**：
- 选择图片目录和标签目录，自动扫描匹配图片-标签对
- 导入进度实时显示
- 导入完成后显示报告（成功数/失败数/异常列表）
- 类别从标签文件自动提取
- 数据集统计面板显示样本数、类别分布

### US-03 训练模型

**作为**工业算法工程师，**我希望**能配置训练参数并启动训练，**以便**得到一个可用的检测模型。

**验收标准**：
- 选择 YOLOv8 变体（n/s/m/l/x）
- 配置 img_size、batch、epochs、patience、device
- 训练启动后日志实时刷新
- 训练进度显示当前 epoch/总 epoch
- 早停生效（patience 到达后自动停止）
- 可随时停止训练
- 训练完成后显示 mAP50、mAP50-95、precision、recall

### US-04 导出模型

**作为**工业算法工程师，**我希望**能将训练好的模型导出为 pt 或 onnx 格式，**以便**部署到生产环境。

**验收标准**：
- 选择模型版本，选择导出格式（pt/onnx）
- ONNX 导出可配置 opset、dynamic、simplify
- 导出进度显示
- 导出完成后验证文件完整性
- 导出历史记录可查看

### US-05 开箱即用

**作为**工业算法工程师，**我希望**下载软件解压后就能直接使用，**以便**不需要花时间配置 Python、CUDA 等环境。

**验收标准**：
- 下载绿色包，解压后双击 exe 即可运行
- Python 运行时、PyTorch、Ultralytics 等依赖随包提供
- 首次启动自动检测 GPU 环境
- GPU 不可用时显示警告但仍可使用 CPU 训练

---

## 三、功能需求

### FR-01 项目管理

| 编号 | 功能 | 优先级 | 说明 |
|------|------|--------|------|
| FR-01-01 | 新建项目 | P0 | 输入名称+路径，创建项目目录和数据库 |
| FR-01-02 | 打开项目 | P0 | 从项目列表打开已有项目 |
| FR-01-03 | 删除项目 | P0 | 删除项目记录（可选删除文件） |
| FR-01-04 | 项目列表 | P0 | 显示所有项目，按更新时间排序 |
| FR-01-05 | 项目目录结构 | P0 | 自动创建 data/models/exports/cache/logs |

### FR-02 数据集导入

| 编号 | 功能 | 优先级 | 说明 |
|------|------|--------|------|
| FR-02-01 | 选择目录 | P0 | 分别选择图片目录和标签目录 |
| FR-02-02 | 扫描匹配 | P0 | 自动匹配图片-标签对，支持 jpg/jpeg/png/bmp |
| FR-02-03 | 导入进度 | P0 | 实时显示导入进度 |
| FR-02-04 | 异常报告 | P0 | 标记空标签、缺标签、格式错误 |
| FR-02-05 | 类别提取 | P0 | 从标签文件自动提取类别列表 |
| FR-02-06 | 数据集统计 | P1 | 样本数、类别分布、标注密度 |

### FR-03 训练

| 编号 | 功能 | 优先级 | 说明 |
|------|------|--------|------|
| FR-03-01 | 模型选择 | P0 | YOLOv8 n/s/m/l/x 变体选择 |
| FR-03-02 | 参数配置 | P0 | img_size/batch/epochs/patience/device/workers |
| FR-03-03 | 数据集选择 | P0 | 从已导入数据集中选择 |
| FR-03-04 | 数据划分 | P0 | train/val 自动划分（默认 80/20） |
| FR-03-05 | 训练启动 | P0 | 调用 Ultralytics YOLO API |
| FR-03-06 | 实时日志 | P0 | 训练输出实时刷新到日志面板 |
| FR-03-07 | 训练进度 | P0 | 当前 epoch/总 epoch |
| FR-03-08 | 早停 | P0 | patience 到达后自动停止 |
| FR-03-09 | 停止训练 | P0 | 可随时优雅停止训练 |
| FR-03-10 | 训练结果 | P0 | mAP50/mAP50-95/precision/recall |
| FR-03-11 | 训练状态 | P0 | running/stopping/succeeded/failed/cancelled |

### FR-04 模型版本管理

| 编号 | 功能 | 优先级 | 说明 |
|------|------|--------|------|
| FR-04-01 | 版本列表 | P0 | 按训练时间排序显示所有模型版本 |
| FR-04-02 | 版本详情 | P0 | 训练配置、指标、权重路径 |
| FR-04-03 | 版本标签 | P1 | baseline/best-so-far/production-candidate |

### FR-05 模型导出

| 编号 | 功能 | 优先级 | 说明 |
|------|------|--------|------|
| FR-05-01 | pt 导出 | P0 | 复制 best.pt 到 exports/ 目录 |
| FR-05-02 | ONNX 导出 | P0 | 配置 opset/dynamic/simplify 后导出 |
| FR-05-03 | 导出验证 | P0 | onnxruntime 可加载验证 |
| FR-05-04 | 导出历史 | P1 | 查看所有导出记录 |

---

## 四、非功能需求

### NFR-01 稳定性

- 主程序零闪退，所有异常必须被捕获并优雅处理
- Python 后端崩溃不影响主界面，自动重启后端
- 训练过程中主界面保持响应
- 关闭主程序时 Python 子进程干净退出

### NFR-02 性能

- 数据集导入 1000 张图片 ≤10 秒
- 缩略图网格浏览流畅滚动
- 训练日志实时刷新无延迟
- 主界面启动时间 ≤5 秒

### NFR-03 兼容性

- Windows 10/11 64 位
- NVIDIA GPU：30/40/50 系列
- CUDA 12.1
- CPU 训练作为降级方案

### NFR-04 可用性

- 全中文界面
- 深色工业风主题
- 危险操作二次确认
- 长任务可见进度
- 错误提示可定位下一步

---

## 五、IPC 契约（MVP 必要命令）

| 命令 | 方向 | 说明 |
|------|------|------|
| `environment.check` | C++→Python | 检查运行环境 |
| `dataset.validate` | C++→Python | 验证数据集格式 |
| `train.start` | C++→Python | 启动训练 |
| `train.stop` | C++→Python | 停止训练 |
| `train.status` | C++→Python | 查询训练状态 |
| `train.data_split` | C++→Python | 数据集划分 |
| `export.run` | C++→Python | 执行模型导出 |
| `artifact.verify` | C++→Python | 验证导出产物 |
| `shutdown` | C++→Python | 关闭后端 |

### 事件流

| 事件 | 说明 |
|------|------|
| `task.started` | 任务开始 |
| `task.progress` | 任务进度更新 |
| `task.log` | 任务日志输出 |
| `task.warning` | 任务警告 |
| `task.failed` | 任务失败 |
| `task.succeeded` | 任务成功 |

---

## 六、数据库 Schema（MVP）

### app_config

```sql
CREATE TABLE IF NOT EXISTS app_config (
    key    TEXT PRIMARY KEY,
    value  TEXT NOT NULL
);
```

### projects

```sql
CREATE TABLE IF NOT EXISTS projects (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    root_path   TEXT NOT NULL,
    task_type   TEXT DEFAULT 'detect',
    created_at  TEXT DEFAULT (datetime('now')),
    updated_at  TEXT DEFAULT (datetime('now'))
);
```

### taxonomies

```sql
CREATE TABLE IF NOT EXISTS taxonomies (
    id          TEXT PRIMARY KEY,
    project_id  TEXT NOT NULL REFERENCES projects(id),
    version     INTEGER DEFAULT 1,
    class_names TEXT NOT NULL,
    class_order TEXT NOT NULL,
    created_at  TEXT DEFAULT (datetime('now'))
);
```

### datasets

```sql
CREATE TABLE IF NOT EXISTS datasets (
    id          TEXT PRIMARY KEY,
    project_id  TEXT NOT NULL REFERENCES projects(id),
    name        TEXT NOT NULL,
    image_dir   TEXT NOT NULL,
    label_dir   TEXT NOT NULL,
    sample_count INTEGER DEFAULT 0,
    status      TEXT DEFAULT 'importing',
    created_at  TEXT DEFAULT (datetime('now'))
);
```

### dataset_samples

```sql
CREATE TABLE IF NOT EXISTS dataset_samples (
    id          TEXT PRIMARY KEY,
    dataset_id  TEXT NOT NULL REFERENCES datasets(id) ON DELETE CASCADE,
    file_name   TEXT NOT NULL,
    image_path  TEXT NOT NULL,
    label_path  TEXT,
    is_valid    INTEGER DEFAULT 1,
    error_code  TEXT,
    created_at  TEXT DEFAULT (datetime('now'))
);
```

### training_runs

```sql
CREATE TABLE IF NOT EXISTS training_runs (
    id          TEXT PRIMARY KEY,
    project_id  TEXT NOT NULL REFERENCES projects(id),
    dataset_id  TEXT NOT NULL REFERENCES datasets(id),
    config_json TEXT NOT NULL,
    status      TEXT DEFAULT 'draft',
    started_at  TEXT,
    finished_at TEXT,
    created_at  TEXT DEFAULT (datetime('now'))
);
```

### model_versions

```sql
CREATE TABLE IF NOT EXISTS model_versions (
    id          TEXT PRIMARY KEY,
    project_id  TEXT NOT NULL REFERENCES projects(id),
    run_id      TEXT NOT NULL REFERENCES training_runs(id),
    best_weight_path TEXT,
    last_weight_path TEXT,
    metrics_json TEXT,
    tags        TEXT DEFAULT '[]',
    parent_model_version_id TEXT,
    created_at  TEXT DEFAULT (datetime('now'))
);
```

### export_artifacts

```sql
CREATE TABLE IF NOT EXISTS export_artifacts (
    id          TEXT PRIMARY KEY,
    model_version_id TEXT NOT NULL REFERENCES model_versions(id),
    format      TEXT NOT NULL,
    output_path TEXT NOT NULL,
    status      TEXT DEFAULT 'pending',
    options_snapshot_json TEXT,
    file_size_bytes INTEGER,
    created_at  TEXT DEFAULT (datetime('now'))
);
```

---

## 七、MVP 实施任务分解

### 阶段 1：修复与验证（基于现有代码）

| 任务 | 说明 | 优先级 |
|------|------|--------|
| T-01 | 修复 DatasetService 链接错误（appendImport/resplitDataset 未实现） | P0 |
| T-02 | 修复 ExportService IPC 事件监听缺失 | P0 |
| T-03 | 修复 IpcClient 请求超时和重连机制 | P1 |
| T-04 | 完善 Python 后端训练进度事件推送 | P0 |
| T-05 | CMake 构建验证与修复 | P0 |

### 阶段 2：核心功能完善

| 任务 | 说明 | 优先级 |
|------|------|--------|
| T-06 | 完善数据导入流程（事务批量插入、进度回调） | P0 |
| T-07 | 完善训练工作流（数据划分→训练→结果注册） | P0 |
| T-08 | 完善导出工作流（导出→验证→状态更新） | P0 |
| T-09 | 完善模型版本管理（列表、详情、标签） | P0 |

### 阶段 3：集成测试与修复

| 任务 | 说明 | 优先级 |
|------|------|--------|
| T-10 | 端到端测试：创建项目→导入→训练→导出 | P0 |
| T-11 | 异常场景测试（GPU不可用、空数据集、训练中断） | P0 |
| T-12 | 稳定性测试（长时间训练、大量数据导入） | P1 |

### 阶段 4：打包与发布

| 任务 | 说明 | 优先级 |
|------|------|--------|
| T-13 | windeployqt 部署 Qt 依赖 | P0 |
| T-14 | 嵌入 Python 环境与依赖 | P0 |
| T-15 | 创建绿色包（7z 压缩） | P0 |
| T-16 | Git tag + Release 发布 | P0 |

---

## 八、验收检查单

### 功能验收

- [ ] 新建项目成功，目录结构正确
- [ ] 导入 100+ 图片 YOLO 数据集，成功率 100%
- [ ] 数据集统计面板正确显示类别分布
- [ ] 配置 YOLOv8n 参数并启动训练
- [ ] 训练日志实时刷新
- [ ] 早停生效
- [ ] 停止训练可优雅终止
- [ ] 训练完成后模型版本自动注册
- [ ] 导出 pt 文件到 exports/ 目录
- [ ] 导出 ONNX 文件，onnxruntime 可加载验证
- [ ] GPU 不可用时显示警告

### 非功能验收

- [ ] 主程序不闪退
- [ ] Python 后端崩溃后主界面保持响应
- [ ] 关闭主程序后无残留 Python 进程
- [ ] 绿色包解压后双击 exe 即可运行
- [ ] 界面全中文

### 发布验收

- [ ] Git tag v0.1.0 已创建
- [ ] GitHub/Gitee Release 已发布
- [ ] 绿色包可下载
- [ ] CHANGELOG 已更新
