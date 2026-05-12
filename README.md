# 标炬 (LabelTorch)

工业缺陷检测桌面软件 — 标注、训练、推理、导出一体化平台。

## 功能特性

- **项目管理**: 创建/打开项目，最近项目列表
- **数据导入**: YOLO txt 格式数据导入，自动校验
- **类别映射**: 类别重排、合并、拆分，映射历史追溯
- **标注引擎**: HBB/OBB/分类/异常检测四种标注模式
- **数据快照**: 不可变数据快照，train/val 自动划分
- **训练工作台**: Ultralytics YOLO 集成，多模型家族支持
- **模型版本**: 版本注册、指标追踪、标签管理、谱系链
- **辅助标注**: 推理候选框审核，批量确认/拒绝
- **实验对比**: 多版本横向/纵向指标对比
- **数据质量**: 样本统计、类别分布、异常检测
- **主动学习**: 低置信回流、漏检误检队列、难例优先审核
- **多任务平台**: 检测/OBB/分类/异常统一工作台
- **模型导出**: pt/onnx 格式导出，ONNX 配置面板
- **插件化训练器**: TrainingAdapter 注册机制，第三方适配器集成

## 技术架构

| 层 | 技术 |
|---|---|
| 桌面前端 | Qt 6 + QML + C++17 |
| Python 后端 | Python 3.11 + asyncio JSON-RPC |
| 训练引擎 | Ultralytics YOLO (v5/v8/v8-obb/v8-cls/v10/v11) |
| 数据库 | SQLite 3 (13 张核心表) |
| IPC | stdin/stdout JSON-RPC 协议 |
| 构建 | CMake + Ninja + MSVC 2022 |

## 构建说明

### 前置条件

- Qt 6.11+ (MSVC 2022 64-bit)
- Visual Studio 2022 (MSVC x64)
- CMake 3.21+
- Ninja
- Python 3.11+ (conda 环境: labeltorch)

### 编译

```bash
# 配置
cmake --preset msvc2022-debug

# 构建
cmake --build --preset msvc2022-debug

# 测试
cd out/build/msvc2022-debug && ctest --output-on-failure
```

### Python 后端

```bash
cd backend
pip install -r requirements.txt
```

## 项目结构

```
LabelTorchV/
├── src/
│   ├── app/           # 主程序入口
│   ├── shell/         # 主窗口、导航
│   ├── core/          # 数据库、IPC、文件系统
│   └── features/      # 功能模块
│       ├── project/   # 项目管理
│       ├── dataset/   # 数据导入、类别映射
│       ├── annotation/# 标注引擎
│       ├── training/  # 训练工作台
│       ├── model/     # 模型版本、指标
│       ├── inference/ # 推理、辅助标注
│       └── export/    # 模型导出
├── backend/           # Python 后端
│   └── labeltorch_backend/
│       ├── adapters/  # 训练适配器
│       ├── handlers/  # IPC 命令处理
│       └── protocol.py
├── tests/             # C++ 单元测试
├── scripts/           # 打包部署脚本
└── reference/         # 参考项目
```

## 使用指南

1. **创建项目**: 启动后点击"新建项目"，选择项目路径
2. **导入数据**: 在数据集页面选择 YOLO txt 格式的图片和标签目录
3. **类别映射**: 如需调整类别 ID，在映射页面建立映射规则
4. **标注**: 在标注页面绘制 HBB/OBB 框，或切换到分类/异常模式
5. **创建快照**: 在训练页面选择数据集并创建快照
6. **训练**: 配置参数并启动训练（支持 AMP、断点续训、增量训练）
7. **辅助标注**: 训练完成后，用模型进行推理并审核候选框
8. **导出**: 在导出页面选择模型版本和格式（pt/onnx）

## 参考项目

标炬（LabelTorch）的架构设计和交互逻辑参考了以下优秀开源项目。我们仅学习其设计思路和架构模式，不直接复制代码。特此致谢：

### 第一优先（核心参考）

| 项目 | 技术栈 | 参考价值 |
|---|---|---|
| **X-AnyLabeling** | PySide6 + YOLO | YOLO 生态标注集成、模型推理集成、标注交互架构 |
| **ultralytics** | Python | 训练 API 设计、模型导出流程、配置参数体系 |
| **labelme** | PyQt5 | QGraphicsView 标注画布实现、轻量标注交互 |
| **ImageViewer-Qt6** | Qt6 + C++ | Qt6 高性能图像浏览器、缩放平移架构参考 |

### 第二优先（训练与格式参考）

| 项目 | 技术栈 | 参考价值 |
|---|---|---|
| **yolov5** | Python | YOLOv5 训练配置组织方式、数据加载 |
| **JIETStudio** | Python GUI | 端到端 YOLO 训练桌面 GUI 工作流参考 |
| **cvat** | Web/Docker | 复杂标注流程组织、标注审核、AI辅助标注思路 |

### 第三优先（OBB 与特殊场景）

| 项目 | 技术栈 | 参考价值 |
|---|---|---|
| **roLabelImg** | PyQt5 | OBB 旋转框标注交互、旋转框数据格式 |
| **mmrotate** | PyTorch | OBB 训练框架设计、旋转检测 pipeline |
| **DOTA_devkit** | Python | OBB 数据格式规范（DOTA 格式） |
| **anylabeling** | PySide6 | 标注与推理集成思路 |

### 参考使用建议

1. 标注画布与交互：重点参考 **X-AnyLabeling**、**labelme**、**ImageViewer-Qt6**
2. 训练编排与 YOLO API：重点参考 **ultralytics**、**yolov5**
3. OBB 旋转框：Phase 3 阶段重点参考 **roLabelImg**、**mmrotate**、**DOTA_devkit**
4. 工程管理与流程设计：参考 **cvat**（标注审核流程）、**JIETStudio**（训练 GUI 工作流）
5. 参考技术路线和架构模式，不直接复制功能代码

### 开源致谢

| 项目 | 仓库地址 | 许可证 |
|---|---|---|
| X-AnyLabeling | https://github.com/CVHub520/X-AnyLabeling | GPL-3.0 |
| Ultralytics YOLO | https://github.com/ultralytics/ultralytics | AGPL-3.0 |
| labelme | https://github.com/wkentaro/labelme | GPL-3.0 |
| ImageViewer-Qt6 | https://github.com/p-ranav/ImageViewer-Qt6 | MIT |
| YOLOv5 | https://github.com/ultralytics/yolov5 | AGPL-3.0 |
| JIETStudio | https://github.com/hazegreleases/JIETStudio | — |
| CVAT | https://github.com/cvat-ai/cvat | MIT |
| roLabelImg | https://github.com/cgvict/roLabelImg | MIT |
| mmrotate | https://github.com/open-mmlab/mmrotate | Apache-2.0 |
| DOTA_devkit | https://github.com/CAPTAIN-WHU/DOTA_devkit | GPL-3.0 |
| AnyLabeling | https://github.com/vietanhdev/anylabeling | GPL-3.0 |

> **注意**：本项目使用 MIT 许可证。以上参考项目仅作架构设计参考，标炬的所有代码均为独立实现。
> 如参考了特定项目的具体实现模式，将在对应源文件头部注释中标注来源。

## 许可证

MIT License
