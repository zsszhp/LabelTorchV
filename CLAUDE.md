# LabelTorchV 项目上下文 (CLAUDE.md)

本文件是标炬（LabelTorch）项目的 Claude Code 专用记忆文件，包含项目架构、技术栈、编码规范和开发约束的完整定义。AI 助手在每次会话中应优先读取本文件以获取项目上下文。

---

## 1. 项目概述

**标炬（LabelTorch）** — 面向工业缺陷检测的本地离线视觉数据治理与模型闭环平台。

核心工作流：导入数据集（图片+YOLO txt标签）→ 数据快照 → 训练 → 推理/辅助标注 → 导出模型（pt/onnx）

支持任务类型：`detect`（目标检测）、`obb`（旋转框检测）、`classify`（分类）、`anomaly`（异常检测）

---

## 2. 技术栈

| 层级 | 技术 |
|------|------|
| 前端 | Qt 6.11 + QML + C++17 |
| 后端 | Python 3.11 + Ultralytics + Anomalib（可选） |
| IPC | stdin/stdout JSON-RPC（QProcess 管理 Python 子进程） |
| 数据库 | SQLite 3（WAL 模式，14张核心表） |
| 构建 | CMake 3.22+ / Ninja / MSVC 2022 (v145工具集 14.51.36231) |
| CUDA | 12.1 |
| Qt 组件 | Core, Quick, QuickControls2, QuickDialogs2, Sql |

---

## 3. 环境与路径

### 3.1 开发环境

- **操作系统**: Windows 10/11 x64
- **编译器**: MSVC 2022 v145 (14.51.36231)
- **MSVC 路径**: `C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe`
- **Qt 安装路径**: `C:/Qt/6.11.1/msvc2022_64`
- **Ninja 路径**: `C:/Qt/Tools/Ninja`
- **Windows SDK**: 10.0.26100.0

### 3.2 Python 环境

- **Conda 环境名**: `labeltorch`
- **Conda 环境路径**: `C:/A/anaconda/envs/labeltorch`
- **Python 可执行文件**: `C:/A/anaconda/envs/labeltorch/python.exe`
- **Python 版本**: 3.11+
- **关键依赖**: ultralytics>=8.0, onnxruntime>=1.15, opencv-python>=4.8, Pillow>=10.0, numpy>=1.24
- **可选依赖**: anomalib（异常检测适配器，未安装时自动跳过注册）, onnx（模型验证）
- **CMake 查找脚本**: `cmake/FindPythonEnv.cmake`

### 3.3 项目数据库

- **位置**: `QStandardPaths::AppDataLocation` → `labeltorch.db`
- **模式**: SQLite WAL
- **Schema 定义**: `src/core/database/Schema.h` + `Schema.cpp`
- **数据库单例**: `Database::instance()`

---

## 4. 目录结构

```
LabelTorchV/
├── CMakeLists.txt                  # 根构建脚本，Qt6 + C++17
├── CMakePresets.json               # 构建预设 (msvc2022-debug/release, x64-debug/release, mingw)
├── cmake/
│   └── FindPythonEnv.cmake         # 查找 labeltorch conda 环境
├── src/
│   ├── app/                        # 应用入口
│   │   ├── main.cpp                # 程序入口：初始化所有Service/Model → QML上下文注入 → 加载Main.qml
│   │   └── AppController.h/cpp     # 全局状态：当前页面、当前项目、后端就绪状态
│   ├── core/                       # 核心基础设施 (labeltorch_core 静态库)
│   │   ├── cache/
│   │   │   ├── ThumbnailCache      # 内存缩略图缓存 (QCache，按字节限容)
│   │   │   ├── ThumbnailProvider   # QQuickImageProvider 缩略图供给
│   │   │   └── ImageDecodeCache    # 画布单次解码缓存
│   │   ├── database/
│   │   │   ├── Database            # SQLite 单例，连接/迁移管理（schema V4）
│   │   │   └── Schema              # 核心表DDL + 性能索引常量
│   │   ├── filesystem/
│   │   │   └── ProjectFs           # 项目目录结构管理
│   │   ├── ipc/
│   │   │   ├── IpcClient           # QProcess管理Python后端，JSON-RPC收发，指数退避重启
│   │   │   └── IpcProtocol         # IPC消息类型、命令常量、事件类型定义
│   │   ├── utils/
│   │   │   ├── AppSettings         # QSettings封装（最近项目、Python路径、窗口状态）
│   │   │   ├── AuditLog            # 统一审计日志（写入 task_events）
│   │   │   ├── Id                  # UUID生成
│   │   │   ├── JsonHelper          # JSON序列化工具
│   │   │   └── Log                 # 分模块日志系统 (lt.core/lt.ipc/lt.db/...)
│   │   ├── ThumbnailGenerator      # 多线程缩略图生成 (QThreadPool)
│   │   └── CMakeLists.txt
│   ├── features/                   # 业务功能模块
│   │   ├── project/                # 项目管理 + 类别体系
│   │   │   ├── ProjectService      # 项目CRUD、任务类型管理、级联删除+审计
│   │   │   ├── ProjectModel        # 项目列表QAbstractListModel
│   │   │   ├── TaxonomyService     # 类别体系版本管理（废弃保留id）
│   │   │   ├── TaxonomyModel       # 类别列表Model
│   │   │   └── qml/                # ProjectPage, TaxonomyPage, ProjectCard, TaskTypeSwitcher
│   │   ├── dataset/                # 数据集导入/扫描/统计/异常检测
│   │   │   ├── DatasetService      # YOLO/COCO/Anomaly格式导入、样本统计、异常检测
│   │   │   ├── DatasetModel        # 数据集列表Model
│   │   │   ├── ImportScanner       # 文件夹扫描、格式自动探测
│   │   │   ├── ClassMappingService # 源schema→目标taxonomy类别映射
│   │   │   └── qml/                # ImportPage, DatasetBrowserPage, ClassMappingPage, DatasetStatsView
│   │   ├── annotation/             # 标注（HBB/OBB/分类/异常）+ 修订追踪
│   │   │   ├── AnnotationService   # YOLO txt读写、修订追踪（undo/audit）
│   │   │   ├── AnnotationModel     # 标注数据Model
│   │   │   ├── canvas/
│   │   │   │   ├── CanvasController # 画布控制（缩放/平移/坐标变换/绘制模式）
│   │   │   │   ├── InteractionManager # 交互管理
│   │   │   │   └── RenderLayer     # 渲染层定义
│   │   │   ├── geometry/
│   │   │   │   ├── Geometry.h      # ShapeType枚举(HBB/OBB/Polygon) + Annotation元数据
│   │   │   │   ├── AxisAlignedBox  # HBB水平框
│   │   │   │   ├── RotatedBox      # OBB旋转框
│   │   │   │   └── Polygon         # 多边形（预留）
│   │   │   ├── labelio/
│   │   │   │   ├── YoloTxtReader   # YOLO txt标签解析（HBB/OBB/分类/异常）
│   │   │   │   └── YoloTxtWriter   # YOLO txt标签写入（原子写入）
│   │   │   └── qml/                # AnnotationPage, AnnotCanvas, ClassPanel, SampleList
│   │   ├── training/               # 训练任务 + 数据快照
│   │   │   ├── TrainingService     # 训练任务生命周期管理（draft→running→succeeded/failed/stopped）
│   │   │   ├── TrainingModel       # 训练运行列表Model
│   │   │   ├── SnapshotService     # 数据快照（不可变）、train/val划分、物理目录准备
│   │   │   ├── SnapshotModel       # 快照列表Model
│   │   │   └── qml/                # TrainingPage, SnapshotPage, ConfigPanel, LogView
│   │   ├── model/                  # 模型版本注册 + 指标对比
│   │   │   ├── ModelRegistry       # 模型版本注册、血缘追踪、标签(baseline/best/production)
│   │   │   ├── MetricService       # 指标查询与对比
│   │   │   ├── ModelVersionModel   # 模型版本列表Model
│   │   │   └── qml/                # ModelPage, ComparePage, MetricChart
│   │   ├── inference/              # 推理 + 异常检测 + 辅助标注 + 主动学习
│   │   │   ├── InferenceService    # 批量推理（YOLO单张/批量）
│   │   │   ├── AnomalyService      # 异常检测推理
│   │   │   ├── AnomalyDetector     # 异常检测器封装
│   │   │   ├── AssistedLabelService # 辅助标注审核
│   │   │   ├── ActiveLearningService # 主动学习（低置信/误检/漏检/难例队列，落库 active_learning_items）
│   │   │   └── qml/                # AssistPage(智能辅助), ActiveLearningPage, AnomalyInferPanel, VideoInferencePage, ReviewDialog等
│   │   ├── testing/                # 模型测试评估
│   │   │   ├── TestingService      # 评估运行、PR曲线/混淆矩阵/逐类指标、阈值推荐
│   │   │   └── qml/                # TestingPage
│   │   └── export/                 # 模型导出 (pt/onnx/tflite/engine) + 产物验证
│   │       ├── ExportService       # 导出生命周期（pending→running→verifying→succeeded/failed）+ 交付目录
│   │       └── qml/                # ExportPage, OnnxConfigPanel
│   ├── components/                 # 通用QML组件（LabelTorch.Components）
│   │   └── qml/                    # Toast/ToastBus, EmptyState, ConfirmDialog, FormField,
│   │                               # ModalDialog, Splitter, ToggleSwitch, Stepper, FilterBar 等
│   ├── theme/                      # Theme.qml 设计系统（颜色/字体/间距/圆角/动画/阴影令牌）
│   ├── shell/                      # QML Shell层
│   │   ├── qml/
│   │   │   ├── Main.qml            # 主窗口：可折叠导航栏 + StackLayout + 日志面板 + 快捷键
│   │   │   ├── SvgIcon.qml         # SVG图标渲染
│   │   │   └── LogPanel.qml        # 日志面板（可折叠）
│   │   └── CMakeLists.txt
│   └── CMakeLists.txt              # 汇总子模块
├── backend/                        # Python 后端
│   ├── pyproject.toml              # labeltorch-backend 包定义
│   ├── requirements.txt            # Python 依赖
│   └── labeltorch_backend/
│       ├── __main__.py             # 入口点
│       ├── server.py               # IpcServer 主循环 (stdin/stdout JSON-RPC, asyncio)
│       ├── protocol.py             # JSON-RPC 消息编解码 (request/response/event)
│       ├── handlers/               # 命令处理器
│       │   ├── environment.py      # environment.check - 环境检测
│       │   ├── training.py         # train.start/stop/status/list_adapters/data_split
│       │   ├── inference.py        # inference.run - YOLO推理
│       │   ├── export.py           # export.run / artifact.verify - 导出与验证
│       │   ├── anomaly.py          # anomaly.infer - 异常检测推理
│       │   └── active_learning.py  # 主动学习：低置信收集/队列排序/统计
│       ├── adapters/               # 训练适配器（策略模式）
│       │   ├── base.py             # TrainingAdapter 抽象基类（7个抽象方法）
│       │   ├── registry.py         # TrainingAdapterRegistry 插件注册表
│       │   ├── ultralytics_adapter.py  # YOLO训练/推理/导出适配器
│       │   └── anomalib_adapter.py     # 异常检测训练/推理/导出适配器（可选依赖）
│       └── tools/
│           └── data_split.py       # 数据集划分工具
├── tests/                          # C++ 测试
│   ├── test_database.cpp           # 数据库测试
│   ├── test_labelio.cpp            # 标签IO测试
│   ├── test_geometry.cpp           # 几何内核测试
│   ├── test_ipc.cpp                # IPC测试
│   ├── test_taxonomy.cpp           # 类别体系测试
│   ├── test_snapshot.cpp           # 快照测试
│   ├── test_training.cpp           # 训练服务测试
│   ├── test_model.cpp              # 模型服务测试
│   ├── test_inference.cpp          # 推理服务测试
│   ├── test_export.cpp             # 导出服务测试
│   ├── test_classify.cpp           # 分类端到端测试
│   ├── test_audit_log.cpp          # 审计落库与指标历史双路测试（P2-12）
│   ├── test_snapshot_hash.cpp      # 快照哈希冻结测试（P0-1）
│   ├── test_taxonomy_delete.cpp    # 类别删除防错位测试（P0-2）
│   ├── test_export_verify.cpp      # 导出验证闭环测试（P0-3）
│   ├── test_labelio_atomic.cpp     # 标签原子写入测试（P0-4）
│   ├── test_project_cascade.cpp    # 项目删除级联测试（P0-5）
│   ├── test_dataset_hash.cpp       # hash落库与去重测试（P0-6）
│   ├── test_ipc_restart.cpp        # 后端进程治理测试（P0-8）
│   ├── test_thumbnail_lifecycle.cpp # 缩略图生命周期测试（P0-11）
│   ├── test_p1_workflow.cpp        # P1主工作流闭环测试
│   ├── test_thumbnail_cache.cpp    # 缩略图限容测试（P1-21）
│   ├── test_perf_indexes.cpp       # 数据库索引测试（P1-23）
│   └── CMakeLists.txt
├── scripts/                        # 构建/部署脚本
├── docs/                           # 设计文档
│   └── 用户手册/                    # 终端用户手册（01~06）
├── installer/                      # IFW安装包配置
├── LICENSE                         # MIT 许可证
├── CHANGELOG.md                    # 版本变更记录
└── .ai_context.md                  # AI共享记忆（多IDE同步用）
```

---

## 5. 架构详解

### 5.1 整体架构

```
QML UI 层 (Main.qml + 8个主导航页 + 6个孤页 + Theme.qml + Components)
  ↓ setContextProperty 注入
C++ Service 层 (ProjectService / DatasetService / AnnotationService / TrainingService / ...)
  ↓ SQLite / IPC
数据层 (Database单例 + IpcClient → Python后端)
  ↓ stdin/stdout JSON-RPC
Python 后端 (IpcServer + Handlers + Adapters)
  ↓ Ultralytics / Anomalib
训练框架层
```

### 5.2 前端模块功能

| 模块 | 路径 | 核心类 | 职责 |
|------|------|--------|------|
| **项目管理** | `src/features/project/` | ProjectService, TaxonomyService | 项目CRUD、任务类型(detect/obb/classify/anomaly)、级联删除+审计、类别体系版本管理 |
| **数据集** | `src/features/dataset/` | DatasetService, ImportScanner, ClassMappingService, TagService | YOLO/COCO/Anomaly格式导入、样本扫描统计、异常检测、类别映射、数据集标签 |
| **标注** | `src/features/annotation/` | AnnotationService, AnnotationModel, CanvasController | HBB/OBB/分类/异常标注、YOLO txt读写、修订追踪（undo/audit） |
| **训练** | `src/features/training/` | TrainingService, SnapshotService | 训练任务生命周期、数据冻结版（哈希冻结快照）、失败诊断(failure_info_json)、train/val划分 |
| **模型** | `src/features/model/` | ModelRegistry, MetricService | 模型版本注册与血缘追踪、标签(baseline/best/production)、指标对比、run_metrics 指标历史 |
| **推理** | `src/features/inference/` | InferenceService, AnomalyService, AssistedLabelService, ActiveLearningService | 批量推理、异常检测推理、辅助标注审核、难例挖掘（active_learning_items 落库） |
| **测试** | `src/features/testing/` | TestingService | 模型评估运行、PR曲线/混淆矩阵、逐类指标、部署阈值推荐 |
| **导出** | `src/features/export/` | ExportService | 模型导出(pt/onnx/tflite/engine)、产物验证(ONNX Runtime校验)、交付目录 |
| **通用组件** | `src/components/` | Toast/ToastBus, EmptyState, ConfirmDialog, FormField 等 | UX反馈体系：Toast通知、空态引导、危险操作确认、表单校验 |

### 5.3 后端模块功能

| 处理器 | IPC命令 | 功能 |
|--------|---------|------|
| environment.py | `environment.check` | 检测Python/PyTorch/CUDA/Ultralytics/ONNX Runtime版本 |
| training.py | `train.start` | 启动训练任务，通过适配器模式支持多框架，epoch级进度推送 |
| training.py | `train.stop` | 停止训练任务 |
| training.py | `train.status` | 查询训练状态 |
| training.py | `train.list_adapters` | 列出已注册的训练适配器 |
| training.py | `train.data_split` | 数据集划分 |
| inference.py | `inference.run` | YOLO模型推理（单张/批量） |
| export.py | `export.run` | 模型导出（动态选择适配器） |
| export.py | `artifact.verify` | 导出产物验证（ONNX Runtime/TorchScript） |
| anomaly.py | `anomaly.infer` | 异常检测推理，生成异常热力图 |
| active_learning.py | `active_learning.collect_low_conf` | 低置信度样本收集 |
| active_learning.py | `active_learning.prioritize_queue` | 队列优先级排序 |
| active_learning.py | `active_learning.queue_stats` | 队列统计 |

### 5.4 训练适配器体系

```
TrainingAdapter (抽象基类，7个抽象方法)
├── validate_config()          # 验证训练配置
├── prepare_dataset_snapshot() # 准备训练数据快照
├── start_training()           # 启动训练
├── stop_training()            # 停止训练
├── parse_logs()               # 解析训练日志
├── collect_metrics()          # 收集训练指标
└── export_model()             # 导出模型

具体实现：
├── UltralyticsAdapter    # YOLO系列: yolov5/yolov8/yolov8_obb/yolov8_cls/yolov10/yolov11
└── AnomalibAdapter       # 异常检测: efficient_ad/patchcore/padim/stfpm/cflow/dfkde/dfm/ganomaly/fastflow/reverse_distillation/csflow/devnet
```

- 注册机制：`TrainingAdapterRegistry.register(name, class)` 插件式注册
- 内置适配器通过 `register_builtin_adapters()` 自动注册
- Anomalib 为可选依赖，未安装时跳过注册（`try/except ImportError`）

---

## 6. IPC 通信协议

### 6.1 消息格式

```json
// 请求 (C++ → Python)
{"type": "request", "request_id": "req_N_M", "command": "train.start", "payload": {...}, "timestamp": ...}

// 响应 (Python → C++)
{"type": "response", "request_id": "req_N_M", "command": "train.start", "success": true, "result": {...}, "error": {}, "timestamp": ...}

// 事件 (Python → C++, 异步推送)
{"type": "event", "event_type": "task.progress", "task_id": "...", "payload": {...}, "timestamp": ...}
```

### 6.2 命令常量（IpcProtocol.h）

| 常量 | 值 | 说明 |
|------|-----|------|
| CMD_ENV_CHECK | `environment.check` | 环境检测 |
| CMD_TRAIN_START | `train.start` | 启动训练 |
| CMD_TRAIN_STOP | `train.stop` | 停止训练 |
| CMD_TRAIN_STATUS | `train.status` | 查询训练状态 |
| CMD_INFERENCE_RUN | `inference.run` | 推理 |
| CMD_EXPORT_RUN | `export.run` | 导出 |
| CMD_ARTIFACT_VERIFY | `artifact.verify` | 产物验证 |
| CMD_ANOMALY_INFER | `anomaly.infer` | 异常检测推理 |

### 6.3 事件类型

| 事件 | 触发时机 |
|------|----------|
| `task.started` | 训练开始 |
| `task.progress` | 每个epoch结束（含loss/mAP50/mAP50-95/precision/recall） |
| `task.log` | 训练日志 |
| `task.warning` | 训练警告 |
| `task.failed` | 训练失败 |
| `task.succeeded` | 训练成功（含best_weight_path/last_weight_path/metrics） |
| `task.stopped` | 用户手动停止 |

### 6.4 IpcClient 关键机制

- 通过 `QProcess` 启动 Python 后端（`C:/A/anaconda/envs/labeltorch/python.exe`）
- 自动重启（最多5次，`m_autoRestart = true`）
- Watchdog 定时器监控连接状态（30秒超时）
- 请求ID格式：`req_{counter}_{timestamp}`
- 待响应命令映射：`m_pendingCommands` (request_id → command)

---

## 7. 数据库 Schema（17张核心表，schema V4）

| 表名 | 用途 | 关键字段 |
|------|------|----------|
| `projects` | 项目信息 | id, name, root_path, task_type, default_device, default_model_family |
| `taxonomies` | 类别体系定义 | id, project_id, name, version, class_definitions_json |
| `datasets` | 数据集元信息 | id, project_id, name, image_root, label_root, format, sample_count, import_status |
| `dataset_samples` | 样本记录 | id, dataset_id, image_path, label_path, width, height, hash, validation_status, split, error_code |
| `imported_label_schemas` | 导入时原始标签schema | id, dataset_id, raw_class_names_json, raw_class_order_json, source_format |
| `class_mapping_revisions` | 类别映射修订记录 | id, dataset_id, source_schema_id, target_taxonomy_id, mapping_rules_json |
| `annotation_revisions` | 标注修订记录 | id, dataset_id, sample_id, source_type, before_snapshot_json, after_snapshot_json |
| `dataset_snapshots` | 数据冻结版（快照，不可变） | id, dataset_id, sample_manifest_json, split_manifest_json, taxonomy_version, annotation_revision_boundary |
| `training_runs` | 训练运行记录 | id, project_id, snapshot_id, config_snapshot_json, runtime_env_snapshot_json, status, log_uri, **failure_info_json** |
| `model_versions` | 模型版本（训练产出/外部导入） | id, run_id, parent_model_version_id, best_weight_path, last_weight_path, metrics_snapshot_json, source, project_id, import_source_json |
| `assisted_label_batches` | 辅助标注批次 | id, model_version_id, dataset_id, target_sample_scope, conf_threshold, iou_threshold, candidate_snapshot_json |
| `export_artifacts` | 导出产物记录 | id, model_version_id, format, options_snapshot_json, output_path, validation_result, status |
| `task_events` | 任务事件审计日志 | id, task_type, task_id, event_type, payload_json |
| `run_metrics` | 训练指标（每epoch） | id, run_id, epoch, metric_name, metric_value |
| `testing_runs` | 测试运行记录 | id, project_id, model_version_id, snapshot_id, config_json, status, metrics_json, confusion_matrix_json, pr_curve_json |
| `dataset_tags` | 数据集标签（分类筛选） | id, dataset_id, name, shortcut |
| `active_learning_items` | 难例挖掘队列（重启不丢） | id, queue_type, sample_path, sample_id, dataset_id, project_id, reason, priority, confidence, class_index, class_name, payload_json, status |

### 7.1 迁移版本

| 版本 | 内容 |
|------|------|
| V1 | 基线 schema |
| V2 | model_versions 新增 source / import_source_json，run_id 放宽为可空（支持外部导入模型） |
| V3 | training_runs 新增 failure_info_json（训练失败日志尾部 + 结构化诊断） |
| V4 | 核心查询路径性能索引（dataset_samples / run_metrics / task_events / training_runs / model_versions 等） |

### 7.2 审计（P2-12）

`AuditLog::record(taskType, taskId, eventType, payload)` 统一写入 `task_events`，已接入：

- ProjectService::deleteProject（project/deleted，含删除前级联摘要）
- DatasetService::deleteDataset（dataset/deleted）、importDataset（dataset/imported）
- TrainingService::startTraining / stopTraining / deleteRun（train_start / train_stop / deleted）
- ExportService::exportModel（export/export_start）

`MetricService::getMetricHistory` 双路兼容：优先查 `run_metrics`，无数据时回退 `task_events.epoch_complete`。

---

## 8. 项目文件系统约定

`ProjectFs` 为每个项目创建以下目录结构：

```
{projectRoot}/
├── project.json           # 项目元数据
├── data/                  # 数据根目录
├── datasets/              # 数据集存储
├── snapshots/             # 数据快照（不可变）
├── taxonomy/              # 类别体系文件
├── revisions/             # 修订记录
├── models/                # 模型存储
├── versions/              # 模型版本
├── runs/                  # 训练运行目录
├── exports/               # 导出产物
├── cache/                 # 缓存
├── thumbnails/            # 缩略图缓存
└── logs/                  # 日志
```

---

## 9. QML 界面架构

### 9.1 主窗口布局

`Main.qml` 采用三栏布局：
1. **左侧可折叠导航栏**（200px展开/64px折叠）：8个主导航项（按任务类型裁剪）+ GPU状态 + Python后端连接状态
2. **中间内容区**（StackLayout）：主 8 页 + 孤 6 页，共 14 个 Loader 按需加载
3. **底部日志面板**（可折叠，使用 Theme.logPanelHeight）

### 9.2 主导航页面（8 页）

| 索引 | pageId | QML页面 | 分组 | 需要项目 |
|------|--------|---------|------|----------|
| 0 | project | ProjectPage.qml | 数据 | 否 |
| 1 | dataset | DatasetPage.qml | 数据 | 是 |
| 2 | check | CheckPage.qml | 数据 | 是 |
| 3 | annotation | AnnotationPage.qml | 数据 | 是 |
| 4 | training | TrainingPage.qml | 训练评估 | 是 |
| 5 | test | TestingPage.qml | 训练评估 | 是 |
| 6 | assist | AssistPage.qml | 交付 | 是 |
| 7 | export | ExportPage.qml | 交付 | 是 |

### 9.3 孤页入口（不占导航，宿主页面跳转，标题栏显示「返回」面包屑）

| pageId | QML页面 | 宿主 | 中文别名 |
|--------|---------|------|----------|
| import | ImportPage.qml | dataset | 数据导入向导 |
| classmap | ClassMappingPage.qml | dataset | 类别映射 |
| taxonomy | TaxonomyPage.qml | project | 类别体系 |
| snapshot | SnapshotPage.qml | training | 数据冻结版 |
| model | ModelPage.qml | export | 模型中心 |
| compare | ComparePage.qml | export | 指标对比 |

### 9.4 Theme.qml 设计系统

- **配色**: 深靛蓝背景（#0D0E15 → #1C1F30）+ 粉红强调色（#FF4A70）+ 紫色辅助（#8B5CF6）
- **字体**: Segoe UI / Microsoft YaHei，等宽字体 Cascadia Code / Consolas
- **类别配色**: 10色循环数组 classColors
- **所有颜色/字体/间距/圆角/动画时长/阴影**通过 Theme.qml Singleton 统一管理
- **安全数值**: `safeNum` / `safeSize` 钳制 NaN（Qt 6.11 Debug 规避）

### 9.5 通用组件（LabelTorch.Components）

| 组件 | 用途 |
|------|------|
| Toast / ToastBus | 全局通知：成功 2s 自动消失，错误常驻 |
| EmptyState | 空态引导（图标 + 文案 + 主按钮） |
| ConfirmDialog | 危险操作二次确认，默认焦点「取消」，列明连带项 |
| FormField | 表单内联校验（红框 + 底部错误文案） |
| ModalDialog / Splitter / ToggleSwitch / Stepper / FilterBar 等 | 通用交互控件 |

### 9.6 快捷键

| 快捷键 | 作用 |
|--------|------|
| F1 | 打开使用帮助 |
| ? | 快捷键卡片 |
| Ctrl+1 ~ Ctrl+8 | 切换主导航页面 |
| Esc | 关闭弹窗 / 取消绘制 |
| Ctrl+S / Delete / Ctrl+Z / Ctrl+Y | 标注保存 / 删除 / 撤销 / 重做 |
| 1~9 / [ ] / 方向键 | 标注页类别切换、尺寸微调、选框移动 |

### 9.7 QML上下文注入

`main.cpp` 中通过 `engine.rootContext()->setContextProperty()` 注入所有 Service/Model 实例，QML 中直接通过属性名访问。

---

## 10. 构建与测试命令

### 10.1 构建

```powershell
# 推荐：使用 CMake Preset
cmake --preset msvc2022-release
cmake --build --preset msvc2022-release

# Debug 构建
cmake --preset msvc2022-debug
cmake --build --preset msvc2022-debug
```

构建输出目录：`out/build/msvc2022-release/` 或 `out/build/msvc2022-debug/`

可用预设：`msvc2022-debug`, `msvc2022-release`, `x64-debug`, `x64-release`, `mingw-debug`, `mingw-release`

### 10.2 测试

```powershell
# C++ 测试
ctest --preset msvc2022-release

# Python 测试
cd backend && python -m pytest tests/
```

C++ 测试目标：test_database, test_labelio, test_geometry, test_ipc, test_taxonomy, test_snapshot, test_training, test_model, test_inference, test_export, test_classify, test_audit_log, 以及 P0/P1 回归（test_snapshot_hash, test_taxonomy_delete, test_export_verify, test_labelio_atomic, test_project_cascade, test_dataset_hash, test_ipc_restart, test_thumbnail_lifecycle, test_p1_workflow, test_thumbnail_cache, test_perf_indexes）

### 10.3 Lint

```powershell
cd backend && python -m ruff check .
```

---

## 11. 代码规范

### 11.1 通用规范

- C++/Python/QML 代码**必须加注释**（除非用户明确要求不加）
- 界面语言：**仅中文**
- 提交信息：**详细中文注释**
- **交互与文档语言**：与用户的交互、生成的所有计划（如 `implementation_plan.md`）、任务列表（如 `task.md`）和总结文档（如 `walkthrough.md`）**必须全部使用中文**。
- **禁止占位符**：绝不写 dummy 路径或占位符实现
- **禁止硬编码路径**：项目路径通过 `ProjectFs` 管理，Python路径通过 `AppSettings` 管理

### 11.2 C++ 规范

- 使用 C++17 标准
- Service 类使用 `Q_INVOKABLE` 暴露方法给 QML
- 数据库操作使用 `Database::instance().database()` 获取连接
- 日志使用 `ltInfo(LT_LOG_XXX())` 宏，不使用 `qDebug()` 直接输出
- 所有 ID 使用 UUID（`Id::generate()`）
- Service 间依赖通过 `setXxx()` 方法注入，不使用全局变量

### 11.3 Python 规范

- 使用 `async/await` 异步模式
- Handler 函数签名：`async def handle_xxx(payload: dict) -> dict`
- 事件推送：`server.send_event(event_type, task_id, payload)`
- 适配器必须继承 `TrainingAdapter` 抽象基类
- 可选依赖使用 `try/except ImportError` 处理
- 日志使用 `logging.getLogger(__name__)`

### 11.4 QML 规范

- 所有颜色/字体/间距使用 `Theme.xxx` 引用，禁止硬编码
- 页面通过 `Loader` 按需加载
- Service 调用使用 `xxxService.method()` 格式
- 信号监听使用 `Connections { target: xxx }` 模式
- 界面文字使用中文

### 11.5 日志分类

| 宏 | 分类 | 用途 |
|----|------|------|
| `LT_LOG_CORE()` | lt.core | 核心基础设施 |
| `LT_LOG_DB()` | lt.db | 数据库操作 |
| `LT_LOG_FS()` | lt.fs | 文件系统操作 |
| `LT_LOG_IPC()` | lt.ipc | IPC通信 |
| `LT_LOG_PROJECT()` | lt.project | 项目管理 |
| `LT_LOG_TAXONOMY()` | lt.taxonomy | 类别体系 |
| `LT_LOG_DATASET()` | lt.dataset | 数据集操作 |
| `LT_LOG_ANNOTATION()` | lt.annotation | 标注操作 |
| `LT_LOG_TRAINING()` | lt.training | 训练任务 |
| `LT_LOG_MODEL()` | lt.model | 模型管理 |
| `LT_LOG_INFERENCE()` | lt.inference | 推理操作 |
| `LT_LOG_EXPORT()` | lt.export | 导出操作 |
| `LT_LOG_APP()` | lt.app | 应用生命周期 |

---

## 12. 架构约束

1. **坚持 Qt+C++ + Python 混合架构**，不使用纯Python方案
2. **训练永远依赖数据快照**，不直接依赖数据集实时状态（`dataset_snapshots` 不可变）
3. **模型版本必须可追溯到训练任务和数据快照**（`model_versions.run_id → training_runs.snapshot_id → dataset_snapshots`）
4. **不可逆操作必须可审计**（`annotation_revisions` / `task_events`）
5. **离线优先**，不依赖云端服务
6. **IPC 进程隔离**：Python后端通过 QProcess 独立运行，JSON-RPC 通信，崩溃不影响主进程
7. **数据快照不可变性**：`SnapshotService::createSnapshot()` 创建后不可修改，训练必须基于快照
8. **标注修订追踪**：每次保存标注自动创建 `annotation_revisions` 记录，支持 undo/audit
9. **导出产物验证**：导出后必须经过 `artifact.verify` 验证才能使用

---

## 13. Git 规范

- 每次修改必须提交并推送到双平台（GitHub + Gitee）
- 提交说明使用详细中文注释
- 版本发布使用 git tag + release
- tag 格式：`v{major}.{minor}.{patch}`，如 `v0.1.0`
- 主版本号：重大架构变更或里程碑
- 次版本号：新增功能
- 修订号：Bug 修复

---

## 14. MVP 边界

**MVP 包含**：导入数据集（图片+txt标签）→ 训练 → 导出模型（pt/onnx）

**MVP 不包含**：标注功能、辅助标注、数据治理、多模型支持

---

## 15. 常用操作速查

| 操作 | 命令/位置 |
|------|----------|
| 查看版本号 | `CMakeLists.txt` → `project(LabelTorchV VERSION 0.1.0)` |
| 修改Python路径 | `main.cpp` → `pythonExec` 变量 / `cmake/FindPythonEnv.cmake` |
| 添加新Service | `src/features/xxx/` → 创建Service类 → `main.cpp`中实例化并注入QML上下文 |
| 添加新QML页面 | `src/features/xxx/qml/` → 在 `Main.qml` StackLayout 中添加 Loader |
| 添加新IPC命令 | `IpcProtocol.h` 添加常量 → `backend/server.py` 注册handler → 创建handler函数 |
| 添加新训练适配器 | `backend/adapters/` → 继承 `TrainingAdapter` → 在 `registry.py` 中注册 |
| 修改数据库Schema | `src/core/database/Schema.cpp` → 添加DDL → 更新 `Schema.h` 表名常量 |
| 修改主题配色 | `src/shell/qml/Theme.qml` |
| 修改导航项 | `src/shell/qml/Main.qml` → `navModel` ListModel |
| 数据库位置 | `QStandardPaths::AppDataLocation/labeltorch.db` |
| 日志配置 | `Log::init()` / `Log::setLevel()` / `Log::setCategory()` |

---

## 16. 已知问题与特殊处理

### 16.1 Qt 6.11 NaN ASSERT 问题

Qt 6.11 Debug模式下 `qCheckedFPConversionToInteger` 检测到 NaN 会调用 `qFatal` 导致程序退出。NaN 主要来自 Qt Quick 布局引擎初始化竞态（字体度量 / Image sourceSize / 绑定除零 / 缩略图网格 cellWidth 未钳制）。

**处理方式**（`main.cpp` / `AppController` / QML 规避）：
1. MSVC CRT 报告钩子 `msvcReportHook` 拦截 NaN 相关的 `_CRT_ERROR` / `_CRT_ASSERT`（保留，否则 Debug 直接崩）
2. Qt 消息处理器 `customMessageHandler` 将 NaN 相关 `QtFatalMsg` 降级为 `QtWarningMsg`
3. 两路径统一走 `noteNanAssert`：80ms 防抖去重 + 计数 + 队列上报 `AppController::reportNanAssert`；前 5 次打印详情与调用栈，之后静默
4. 累计 ≥10 次时 `nanRestartRecommended` 为 true，状态栏红条提示「建议保存后重启」（只提示一次）
5. DPI 缩放策略设为 `PassThrough` 防止字体度量为 NaN
6. 规避：root contentItem 启动后 120ms 内 `setEnabled(false)`；页面预加载间隔 300→600ms；CheckPage `Image.sourceSize`/网格 `cellWidth`、SvgIcon `scale`、进度条宽度做 `isFinite` 钳制；Theme 提供 `safeNum`/`safeSize`

### 16.2 QML 模块输出目录

启用 QTP0004 NEW 策略，QML 模块输出目录自动派生为 `${CMAKE_BINARY_DIR}/${target_path}`。例如 `LabelTorch.Shell` → `${CMAKE_BINARY_DIR}/LabelTorch/Shell`。

---

## 17. AI 共享记忆策略

本文件（`CLAUDE.md`）是 Claude Code 专用记忆文件。项目还维护 `.ai_context.md` 作为多 IDE 共享记忆主控。

### 17.1 各 IDE 读取规则

| IDE | 读取文件 | 说明 |
|-----|---------|------|
| Claude Code | `CLAUDE.md` 或 `.claude/` | 本文件 |
| Trae | `.trae/rules/project_rules.md` | 独立文件 |
| Antigravity | `.antigravityrules` | 可软链接指向 `.ai_context.md` |
| Cursor | `.cursorrules` | 可软链接指向 `.ai_context.md` |
| Comate | `.comaterules` | 可软链接指向 `.ai_context.md` |

### 17.2 记忆更新规则

- 新增模块/目录/依赖时，及时更新本文件对应章节
- 架构决策变更时，更新第5章和第12章
- 数据库Schema变更时，更新第7章
- IPC协议变更时，更新第6章
