# CHANGELOG

## v0.1.0 (开发中)

### 基础设施
- SQLite 数据库单例，14 张核心表，WAL 模式，自动迁移机制
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
