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

## 许可证

MIT License
