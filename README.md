# 标炬 (LabelTorch)

工业缺陷检测桌面软件 — 标注、训练、推理、导出一体化平台。

**当前版本**: 0.2.0

> 界面截图：见用户手册（`docs/用户手册/`）。

## 硬件与系统要求

| 项目 | 最低配置 | 推荐配置 |
|------|----------|----------|
| 操作系统 | Windows 10 64 位 | Windows 11 64 位 |
| 处理器 | x64 四核 | x64 八核及以上 |
| 内存 | 8 GB | 16 GB 及以上 |
| 显卡 | 无（CPU 训练可用） | NVIDIA GPU，显存 ≥ 6 GB（RTX 30/40/50 系列，CUDA 12.1） |
| 磁盘空间 | 20 GB | 50 GB 及以上 |

## 安装方式

### 方式一：绿色包（推荐，解压即用）

1. 获取 `LabelTorch-Green-v0.2.0.zip`
2. 解压到任意目录（路径建议不含中文与空格）
3. 双击 `start_LabelTorchV.bat` 启动

无需预装 Python / Qt / conda，内嵌 Python 运行时。GPU 训练需 NVIDIA 驱动，GPU 版 PyTorch 请替换 `python\torch`。

### 方式二：源码构建

见下文「构建说明」。日常使用者建议直接使用绿色包。

> 详细安装、数据存储位置与卸载说明见 [docs/用户手册/02-安装与使用.md](docs/用户手册/02-安装与使用.md)。

## 功能特性

- **项目管理**: 创建/打开项目，最近项目列表，任务类型切换（detect/obb/classify/anomaly）
- **数据导入**: YOLO txt 格式数据导入，自动校验，类别自动提取
- **类别映射**: 类别重排、合并、拆分，映射历史追溯
- **标注引擎**: HBB/OBB/分类/异常检测四种标注模式，修订追踪
- **数据快照**: 不可变数据快照，train/val 自动划分
- **训练工作台**: Ultralytics YOLO 集成，多模型家族支持（v5/v8/v8-obb/v8-cls/v10/v11）
- **模型版本**: 版本注册、指标追踪、标签管理、谱系链
- **辅助标注**: 推理候选框审核，批量确认/拒绝
- **实验对比**: 多版本横向/纵向指标对比
- **数据质量**: 样本统计、类别分布、异常检测
- **主动学习**: 低置信回流、漏检误检队列、难例优先审核
- **多任务平台**: 检测/OBB/分类/异常统一工作台
- **模型导出**: pt/onnx 格式导出，ONNX 配置面板，产物验证
- **插件化训练器**: TrainingAdapter 注册机制，第三方适配器集成

## 技术架构

| 层 | 技术 |
|---|---|
| 桌面前端 | Qt 6 + QML + C++17 |
| Python 后端 | Python 3.11 + asyncio JSON-RPC |
| 训练引擎 | Ultralytics YOLO (v5/v8/v8-obb/v8-cls/v10/v11) |
| 数据库 | SQLite 3 (17 张核心表) |
| IPC | stdin/stdout JSON-RPC 协议 |
| 构建 | CMake + Ninja + MSVC 2022 |

## 构建说明

### 前置条件

- Qt 6.11+ (MSVC 2022 64-bit)
- Visual Studio 2022 (MSVC x64)
- CMake 3.22+
- Ninja
- Python 3.11+ (conda 环境: labeltorch)

### 编译

```bash
# 配置
cmake --preset msvc2022-release

# 构建
cmake --build --preset msvc2022-release

# 测试
ctest --preset msvc2022-release
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
│   ├── shell/         # 主窗口、导航、主题
│   ├── core/          # 数据库、IPC、文件系统、缓存、日志
│   └── features/      # 功能模块
│       ├── project/   # 项目管理、类别体系
│       ├── dataset/   # 数据导入、类别映射
│       ├── annotation/# 标注引擎
│       ├── training/  # 训练工作台、数据快照
│       ├── model/     # 模型版本、指标
│       ├── inference/ # 推理、辅助标注、主动学习
│       └── export/    # 模型导出
├── backend/           # Python 后端
│   └── labeltorch_backend/
│       ├── adapters/  # 训练适配器
│       ├── handlers/  # IPC 命令处理
│       └── tools/     # 数据划分工具
├── tests/             # C++ 单元测试
├── scripts/           # 打包部署脚本
└── docs/              # 设计文档
```

## 使用指南

主工作流（详见 [docs/用户手册/03-快速上手.md](docs/用户手册/03-快速上手.md)）：

1. **创建项目**: 启动后点击"新建项目"，选择项目路径和任务类型
2. **导入数据**: 在数据集页面进入导入向导，选择 YOLO txt / COCO json / 仅图片
3. **类别映射**: 如需调整类别 ID，在映射页面建立映射规则
4. **创建数据冻结版**: 在训练页面创建不可变数据快照（哈希冻结）
5. **训练**: 配置参数并启动训练（支持早停、AMP、断点续训）
6. **测试评估**: PR 曲线、混淆矩阵、阈值推荐与 Go/No-Go 结论
7. **智能辅助**: 辅助标注、难例挖掘、异常检测
8. **导出**: 在导出页面选择模型版本和格式（pt/onnx），导出后自动验证并生成交付目录

主界面为 8 页导航（项目/数据集/检查/标注/训练/测试/智能辅助/导出），另有 6 个子入口页面（数据导入向导、类别映射、类别体系、数据冻结版、模型中心、指标对比）。

### 用户手册

| 文档 | 内容 |
|------|------|
| [01-简介与系统要求](docs/用户手册/01-简介与系统要求.md) | 产品简介、硬件要求、界面概览 |
| [02-安装与使用](docs/用户手册/02-安装与使用.md) | 绿色包/源码构建、设置、卸载 |
| [03-快速上手](docs/用户手册/03-快速上手.md) | 建项目→导入→数据冻结版→训练→导出 |
| [04-快捷键速查表](docs/用户手册/04-快捷键速查表.md) | 全局与标注页快捷键 |
| [05-故障排查](docs/用户手册/05-故障排查.md) | 常见问题与错误码 |
| [06-术语表](docs/用户手册/06-术语表.md) | 中文别名与术语解释 |

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
| **cvat** | Web/Docker | 复杂标注流程组织、标注审核、智能辅助标注思路 |

### 第三优先（OBB 与特殊场景）

| 项目 | 技术栈 | 参考价值 |
|---|---|---|
| **roLabelImg** | PyQt5 | OBB 旋转框标注交互、旋转框数据格式 |
| **mmrotate** | PyTorch | OBB 训练框架设计、旋转检测 pipeline |
| **DOTA_devkit** | Python | OBB 数据格式规范（DOTA 格式） |

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

> **注意**：本项目使用 MIT 许可证。以上参考项目仅作架构设计参考，标炬的所有代码均为独立实现。
> 如参考了特定项目的具体实现模式，将在对应源文件头部注释中标注来源。

## 许可证

MIT License，详见 [LICENSE](LICENSE)。
