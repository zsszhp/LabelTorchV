# CHANGELOG

本文件记录标炬（LabelTorch）各版本变更。版本号以 `CMakeLists.txt` 的
`project(VERSION)` 为单一来源，由 `scripts/sync_version.ps1` 同步至安装器与后端包元数据。

---

## v0.2.0 (2026-09-29)

P0 稳定性收口 + P1 体验与功能闭环后的首个稳定版本。相对 v0.1.0 开发基线，
重点补齐数据安全、假实现清理、主工作流通环与开箱交付能力。

### P0 稳定性与数据安全

- 快照哈希冻结：创建时逐样本写入 imageHash/labelHash，训练前校验漂移
  （`E_SOURCE_DRIFT`），支持物化冻结副本到 `snapshots/{id}/`
- 类别删除防错位：废弃占位保留 class_id，不再物理前移；有引用时拒绝物理删除
  并写入 `class_mapping_revisions` 审计
- 导出验证闭环：`artifact.verify` 失败置 failed，pt 对齐 torchscript 校验，
  tflite/engine 明确标记 `verified:false`
- 标签原子写入：YOLO txt 改为 `MoveFileEx(MOVEFILE_REPLACE_EXISTING)` 原子替换
- 项目删除级联：补全子表清理与目录清理
- 样本 hash 落库与查重：导入时计算文件 hash，支持重复图与跨 split 泄漏检测
- 线程安全 DB：工作线程统一走 `ThreadDbGuard`，`busy_timeout=5000`
- 后端进程治理：稳定 30s 后才清零重启计数，指数退避，Windows Job Object 杀进程树
- 崩溃捕获：`MiniDumpWriteDump` + `set_terminate`，打包最近日志
- Qt 6.11 NaN 防护：hook 保留并计数告警，布局竞态源头钳制，超阈值建议重启
- ThumbnailGenerator 生命周期：析构等待线程池，消除 use-after-free
- 配置原子写入：全库排查直写改 tmp+rename

### P0 假实现清理与安全加固

- 增量训练 resume 真实现：加载 `last.pt`，无断点明确报错
- 异常检测批量推理重写：结果与输入一一对应，去掉 O(N²) 全目录扫描
- `train.start` 真调用 `validate_config`，非法配置返回结构化错误
- torch.load 反序列化加固：`safe_load_weight` 优先 `weights_only=True` + 白名单
- IPC 路径白名单：`require_path_within` 限制在项目根 / 快照目录 / 系统临时目录
- 日志面板挂载：后端错误与任务日志用户可见
- 全局筛选接线：数据集/类别筛选改为真实过滤，移除假交互

### P1 体验、导航与功能闭环

- 导航收敛为 8 页三组（数据 / 训练评估 / 交付），孤页并入宿主入口
- 术语中文别名：snapshot→数据冻结版、active learning→难例挖掘、taxonomy→类别体系
- 全局 Toast 总线（成功 2s / 错误常驻）+ 统一危险操作确认弹窗（默认焦点「取消」）
- 空状态引导组件 EmptyState、表单内联校验 FormField、加载反馈补全
- 后端断连恢复入口、设置真实生效（日志级别 / Python 路径）
- 快捷键体系：F1 帮助、`?` 快捷键卡、Ctrl+1~8 切页
- 辅助标注确认回写 YOLO txt + `annotation_revisions`，一键「用本批数据再训练」
- 主动学习队列落库 `active_learning_items`，重启不丢
- PR 曲线真实采集、部署阈值推荐 + Go/No-Go 结论卡、逐类 AP/P/R 与漏检排序
- 训练失败可诊断：保留日志尾部 + 降 batch/imgsz 建议
- 导出交付目录：模型 + classes.yaml + thresholds.json + warmup.py + 推理样例
- 性能底线：缩略图管线 256px + 字节限容 128MB、样本列表分页、数据库索引迁移、
  画布单次解码、日志环形缓冲 2000 行 + 批量推送
- UI 视觉收口：Theme 令牌归一、中文字体回退、空态/过渡动效

### P2-A 开箱交付

- 修复绿色包构建：对齐真实 CMake 预设（x64-release），补拷 conda `Library/bin`
  运行时 DLL 与 `python311.dll` 依赖链，路径参数化（QT_ROOT / LABELTORCH_PYTHON_ENV）
- Python 依赖精简：trim 白名单仅保留 ultralytics/torch/onnx 运行时链（约 2334 MB
  → 约 1.2 GB），支持 full / conda-pack 模式，脚本注释含离线 wheel 源与体积说明
- 打包后冒烟：关键文件清单 + 打包内 Python 导入自检 + 主程序启动存活检测，失败即中止
- 版本单一源：CMake `project(VERSION)` 权威源，`scripts/sync_version.ps1` 同步
  安装器 / 后端包 / README / CHANGELOG
- 安装器修复：RunProgram 指向 `LabelTorchV.exe`，补齐 MIT LICENSE.txt，
  卸载仅移除安装目录（保留 AppData 用户项目数据）
- 路径参数化：CMakePresets / FindPythonEnv / 打包脚本均支持环境变量或缓存变量覆盖

### P2-C 审计与文档治理

- 统一审计落地：新增 `AuditLog::record()` 写入 `task_events`，覆盖项目删除、
  数据集删除/导入、训练启停/删除、导出创建，payload 含删除前级联摘要
- 指标历史双路兼容：`MetricService::getMetricHistory` 优先读 `run_metrics`，
  空结果回退 `task_events.epoch_complete`（历史兼容路径）
- 文档同步：blueprint 页面清单更新为 8 主页 + 6 孤页；项目上下文文档架构章节对齐
  （导航分组、active_learning_items、failure_info_json、schema V4 索引、
  Toast/EmptyState 等通用组件）
- 新增用户手册 `docs/用户手册/`（01 简介与系统要求、02 安装与使用、03 快速上手、
  04 快捷键速查表、05 故障排查、06 术语表）
- 新增 MIT LICENSE；README 补安装方式、硬件需求与截图占位说明

---

## v0.1.0 (开发基线)

### 基础设施
- SQLite 数据库单例，核心表建模，WAL 模式，自动迁移机制
- IPC 通信层：QProcess 管理 Python 后端，stdin/stdout JSON-RPC 协议
- 日志系统：分模块日志（lt.core/lt.ipc/lt.db/lt.project/...）
- 项目文件系统管理（ProjectFs）
- 应用设置封装（AppSettings）
- UUID 生成工具（Id）
- JSON 序列化工具（JsonHelper）
- 缩略图缓存与多线程生成（ThumbnailCache + ThumbnailGenerator）

### 项目管理
- 项目 CRUD（创建/打开/删除/最近项目列表）
- 任务类型支持（detect/obb/classify/anomaly）
- 类别体系版本管理（TaxonomyService + TaxonomyModel）

### 数据集
- YOLO txt 格式数据导入，自动扫描匹配
- 数据集浏览与统计（缩略图网格、类别分布）
- 类别映射服务（源schema→目标taxonomy映射）
- 导入扫描器（文件夹扫描、格式自动探测）

### 标注引擎（框架已搭建，MVP 阶段不启用）
- 几何内核（AxisAlignedBox / RotatedBox / Polygon）
- YOLO txt 标签读写（YoloTxtReader / YoloTxtWriter，原子写入）
- 画布控制器框架（CanvasController / InteractionManager / RenderLayer）
- 标注修订追踪框架

### 训练
- 数据快照服务（不可变快照、train/val 划分、物理目录准备）
- 训练任务生命周期管理（draft→running→succeeded/failed/stopped）
- Ultralytics 训练适配器（yolov5/yolov8/yolov8_obb/yolov8_cls/yolov10/yolov11）
- 训练适配器插件注册机制（TrainingAdapterRegistry）
- 训练配置面板与实时日志查看

### 模型管理
- 模型版本注册与血缘追踪
- 版本标签管理（baseline/best/production）
- 指标查询与对比服务

### 推理
- 批量推理服务（YOLO 单张/批量）
- 异常检测推理服务与检测器封装
- 辅助标注审核服务框架
- 主动学习服务框架（低置信/误检/漏检/难例队列）

### 导出
- 模型导出服务（pt/onnx/tflite/engine）
- 导出产物验证（ONNX Runtime / TorchScript 校验）

### Python 后端
- IpcServer 主循环（stdin/stdout JSON-RPC, asyncio）
- 命令处理器：environment / training / inference / export / anomaly / active_learning
- 训练适配器：UltralyticsAdapter + AnomalibAdapter（可选依赖）
- 数据集划分工具

### QML 界面
- 主窗口：可折叠导航栏 + StackLayout + 日志面板
- 深靛蓝+粉红强调色主题系统（Theme.qml）
- 7 个功能页面（项目/类别/数据集/标注/训练/模型/导出）
- 状态栏、任务面板、日志面板
