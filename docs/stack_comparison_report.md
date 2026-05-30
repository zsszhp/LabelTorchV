# 标炬（LabelTorch）技术栈抉择报告：基于终极目标的评估与规划

> 版本：1.0 | 更新日期：2026-05-30
> 状态：选型决策依据

为了实现您的终极目标——**“开发出一款稳定、美观、具备高级标注/辅助打标/训练一体化的工业缺陷检测客户端，并能让 AI 快速、无偏差地完全实现出来”**，我们对 **C++ Qt 6 / QML** 与 **React + Electron / Tauri** 两套技术栈进行了深度多维度对比。

结论是：**React + Electron + Python 是实现您终极目标的最优解。**

---

## 一、 基于终极目标的核心维度对比

### 1. AI 自动编写的成功率 (AI-Driven Implementation) — 决胜点
* **您的目标**：您明确要求“规划和文档要给其他的 AI 来实现，能够完整无偏差地实现出来”。
* **C++ Qt / QML 的困境**：AI 对 C++ 的底层编译报错（如 MSVC 链接错误、CMake 宏缺失）和 QML 的运行时黑盒报错（如 QML 引擎里的 NaN 崩溃）处理能力较弱。AI 编写复杂的 C++ 线程模型和 C++ 与 QML 交互的元对象系统时，出错率极高。
* **React + Electron 的优势**：React、TypeScript 和 Node.js 是全球 AI 训练集中**占比最大、数据最成熟**的领域。AI 编写 React 界面、Zustand 状态管理以及 Electron 的主进程逻辑，准确度接近 $95\%$。一旦遇到报错，AI 能够根据控制台的 stack trace 快速自行定位并修复。

### 2. 核心标注画布开发效率 (Canvas Engine for HBB/OBB/Polygon) — 核心瓶颈
* **您的目标**：后续版本必须支持水平矩形 (HBB)、旋转矩形 (OBB)、多边形 (Polygon) 标注，以及异常热力图渲染。
* **C++ Qt / QML 的困境**：QML 自带的 `Canvas` 组件性能较差，绘制 4K 工业图像时易卡顿；若用 C++ 的 `QGraphicsView` 或原生 OpenGL 编写，需要手动写大量复杂的旋转坐标系变换、碰撞检测、鼠标拖拽控制点缩放算法。AI 极难一次性写对，极易出现拖动变形、选框漂移的问题。
* **React + Electron 的优势**：前端生态拥有工业级开源 Canvas 引擎 **Fabric.js**。它原生自带：
  * 图片与选框的缩放、平移。
  * **旋转矩形 (OBB) 的拖拽与角度控制**（开箱即用，无需手写三角函数旋转矩阵）。
  * 丰富的事件监听（如选中、修改、碰撞检测）。
  * AI 对 Fabric.js 的 API 极其熟悉，能在极短时间内生成平滑无卡顿的标注画布。

### 3. UI 界面美学与交互顺畅度 (Industrial Aesthetics & Slider Control)
* **您的目标**：界面美观、全中文、操作逻辑顺畅的深色工业风。
* **C++ Qt / QML 的困境**：QML 想要做出丝滑的暗黑现代风、玻璃质感或渐变微动画，需要针对每一个按钮、滑动条（Slider）、表格进行底层的重绘（Canvas/Style 重构），开发周期长，代码冗长。
* **React + Electron 的优势**：结合 **Tailwind CSS + Shadcn UI / Ant Design (暗黑主题)**，可以直接套用现代扁平化的 UI 组件。AI 可以利用 Tailwind 几秒钟生成极具科技感的暗黑渐变背景、流线型仪表盘和直观的折线图（基于 Echarts/Recharts），达到“惊艳”的第一印象。

### 4. 离线绿色打包与显卡隔离 (Sandboxed Python Engine)
* **您的目标**：免配置环境，解压即用；显卡不闪退，支持 30/40/50 系列。
* **C++ Qt / QML 的困境**：C++ 前端通过 `QProcess` 与 Python 交互，虽然可行，但若 Python 挂掉，C++ 进行异常拦截和崩溃恢复的代码比较繁琐。
* **React + Electron 的优势**：Electron 运行于 Node.js 环境，其 `child_process` 具有极其完善的进程守护（Process Watchdog）与标准流处理。同时，Electron 配合 `electron-builder` 可以方便地将 `frontend` 的 HTML/JS 静态包与 `backend` 的绿色 Python 文件夹统一打包，制作出解压即用的免安装包。

---

## 二、 围绕 React + Electron 技术栈的终极开发路径规划

为了实现这个终极目标，我们将整个开发规划拆解为 **“三步走”** 的闭环，并以文件夹中的规范文档为指挥棒，分派给其他 AI：

```
                    ┌───────────────────────────────┐
                    │       第一阶段：骨架与通信      │
                    │   - 初始化 Vite + Electron    │
                    │   - 搭建暗黑三栏布局页面       │
                    │   - 实现 Python 后端自动拉起   │
                    └───────────────┬───────────────┘
                                    │
                                    ▼
                    ┌───────────────────────────────┐
                    │      第二阶段：数据与训练闭环   │
                    │   - 导入 YOLO 文件夹并解析     │
                    │   - SQLite 初始化与快照生成   │
                    │   - 参数配置面板与训练实时日志 │
                    └───────────────┬───────────────┘
                                    │
                                    ▼
                    ┌───────────────────────────────┐
                    │      第三阶段：标注与智能打标   │
                    │   - Fabric.js 图像与选框渲染  │
                    │   - 辅助标注 (AI生成候选框)    │
                    │   - 自动生成打包与自检报告     │
                    └───────────────────────────────┘
```

### 给其他 AI 的发令枪 Prompt 模版 (按阶段执行)

#### 🚩 任务一：搭建骨架与进程看护（第1天）
> “我需要开发一个工业视觉标注与训练桌面端应用。请阅读 [docs/architecture.md](file:///e:/z/project/my/LabelTorchV/docs/architecture.md)，帮助我初始化一个 React (TypeScript) + Vite + Tailwind CSS + Electron 的项目。要求：
> 1. 主窗口为深色工业风格，顶部为导航，中央根据路由显示 7 个功能页面。
> 2. 实现 Electron 在冷启动时自动读取环境变量或配置，通过子进程拉起运行在 `backend/` 下的 Python JSON-RPC 服务。
> 3. 必须包含 Python 崩溃重启看护机制，并能在前端状态栏实时显示后台连接状态（在线/离线/检测中）。”

#### 🚩 任务二：导入、元数据与训练黑盒（第2-3天）
> “请阅读 [docs/preflight_checklist.md](file:///e:/z/project/my/LabelTorchV/docs/preflight_checklist.md) 中的建表 DDL，编写 Electron 主进程中的 SQLite 3 数据库管理服务。并实现数据集导入页面：
> 1. 用户选择文件夹后，扫描 `images/` 和 `labels/`，校验 YOLO 数据集有效性。
> 2. 将数据记录写入 SQLite，并在主界面以缩略图网格流畅展示。
> 3. 支持选择模型架构、Batch、Epoch 等，点击‘开始训练’，通过 JSON-RPC 通信启动 Python 计算，并在前端控制台实时流式刷新打印训练日志。”

#### 🚩 任务三：Fabric.js 智能画布与辅助标注（第4-5天）
> “请阅读 [docs/industry_standards.md](file:///e:/z/project/my/LabelTorchV/docs/industry_standards.md) 中的画布交互与快捷键规范，使用 Fabric.js 实现中央工作区的标注画布：
> 1. 支持高分辨率图片居中、缩放、空格拖拽平移。
> 2. 支持在图片上进行矩形框 (HBB) 与旋转矩形 (OBB) 的拉框绘制、拖拽、修改类别，并自动写回 YOLO txt 文件。
> 3. 实现‘辅助标注’功能：选择上一版模型，对未打标图像自动预测出虚线候选框，人工按 Y 确认或按 N 拒绝。”
