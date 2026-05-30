# 标炬（LabelTorch）技术栈抉择与代码迁移指南 (Stack Decision & Migration Guide)

> 版本：1.0 | 更新日期：2026-05-30
> 状态：开发启动前核心决策与对齐

在您让 AI 动笔修改代码前，**这是最关键、也是最容易被遗漏的决策点**。您的项目中目前存在两套方案的冲突：
1. **现有代码库**：基于 **C++ (Qt 6 / QML) + Python**，已完成了项目管理、数据库 DDL、IPC 通信协议的大部分底层 C++ 逻辑。
2. **您提供的新 PRD 建议**：基于 **React + Electron / Tauri + Python**，这是一个全新的前端技术栈。

为了确保您能够完整实施并成功做出这个项目，本指南将对比这两条路径，提供**“复用/重构”方案**，并给出**防止 AI 搞乱现有代码库的隔离步骤**。

---

## 一、 技术栈双路径对比与选型建议

| 评估维度 | 路径 A：继续使用 C++ / Qt 6 QML（现有技术栈） | 路径 B：重构成 Electron + React + Tailwind（推荐技术栈） |
| :--- | :--- | :--- |
| **现有资产** | 已有项目、数据集导入、模型管理、IPC 的 C++ 服务层及界面骨架。 | 仅能复用 `backend/` 下的 Python 后端，前端代码需全部重新编写。 |
| **画布开发难度** | 高。QML 的 Canvas 绘图或 C++ QQuickItem 绘制旋转框与缩放较繁琐，开源参考库较少。 | 低。拥有极其成熟的 **Fabric.js** 或 **Konva.js**，支持滚轮缩放、拖拽、选框变形，AI 对 React 画布组件的编写能力极强。 |
| **UI 美化与布局效率** | 一般。QML 虽能实现精美界面，但缺乏 Tailwind CSS / Shadcn UI 等高度成熟的开箱即用美化生态。 | 极高。配合 Tailwind CSS 和 Ant Design / Shadcn UI，AI 几分钟即可生成惊艳的工业级暗黑界面。 |
| **AI 编程友好度** | 中等。AI 有时会混淆 Qt 5 与 Qt 6 的 QML 语法，容易出现编译或连接链接错误。 | 极高。React + TS 是 AI 最擅长的领域，代码生成准确度远超 QML。 |
| **打包部署体积** | 较小（约数百MB，主要体积在 Python 后端）。 | 较大（Electron 壳体本身约 100MB-200MB，加上 Python 后端）。 |

### 💡 决策建议
如果您希望**开发速度快、界面惊艳、标注画布开发顺利、且主要依赖 AI 编写代码**：
👉 **强烈建议选择【路径 B：Electron + React】**。
虽然抛弃了已写的 C++ 代码，但因为底层 **Python 后端 (JSON-RPC) 架构极其通用**，它可以在不改动任何 Python 代码的情况下，直接作为 Electron 的计算进程运行。

---

## 二、 若您选择【路径 B：Electron + React】重构的隔离步骤

如果您决定启用新栈，**千万不要让 AI 直接在当前根目录下生成 React 代码**，否则 CMake 和 Node.js 配置文件会混在一起，导致编译彻底报废。请执行以下步骤进行代码库隔离：

### 1. 新建分支隔离现有 C++ 代码
在根目录下打开终端，运行：
```bash
# 保存现有 C++ 状态到新分支
git checkout -b archive/cpp-qt-version
git add .
git commit -m "Archive C++ Qt version before React migration"
git push

# 回到主分支，清理 C++ 文件，仅保留 Python 后端和文档
git checkout main
```

### 2. 清理与整理目录结构
只保留以下结构，删除所有的 `.cpp`, `.h`, `CMakeLists.txt` 等 C++ 文件，为 React + Electron 腾出干净的空间：
```
LabelTorchV/
├── backend/                       # 核心复用资产：Python 后端与训练适配器 (一字不改)
│   ├── labeltorch_backend/
│   ├── requirements.txt
│   └── pyproject.toml
├── docs/                          # 设计规划文档
│   ├── blueprint.md
│   ├── prd-mvp.md
│   ├── architecture.md
│   ├── industry_standards.md
│   ├── preflight_checklist.md
│   └── stack_decision.md          # 本文档
└── README.md
```

### 3. 创建 `frontend` 独立项目
在根目录下运行 `npm create vite@latest frontend -- --template react-ts`。所有的前端代码、Fabric.js 画布均在 `frontend/` 目录下开发，与 `backend/` 彻底解耦。

---

## 三、 若您选择【路径 A：继续使用 C++ Qt/QML】的开发约束

如果您决定保留现有 C++ 代码继续开发，您需要对 AI 施加以下硬性约束，防止其生成无用的 Node.js/React 代码：

1. **框架限定**：在 Prompt 中明确说明：*“本项目是 C++17 + Qt 6.11 QML 的桌面程序，数据库为 QtSql (SQLite)，IPC 通信使用 QProcess。禁止生成任何 Node.js、React、Vue 或 Electron 依赖。”*
2. **QML 模块约束**：界面组件必须在 `src/features/` 下对应的 `qml/` 目录中修改，并确保每次修改后都在 CMake 中注册了对应的 QML 模块。

---

## 四、 后续开发防遗漏核对单 (AI 实施检查)

无论选择哪个技术栈，当您让 AI 开发具体功能时，请监督其避开以下常见工业算法工具的“暗坑”：

* [ ] **路径空格与中文**：YOLO 训练引擎（Ultralytics）底层是 PyTorch/C++，遇到中文路径经常报错。前台在导入数据集和创建项目时，必须正则校验路径，若有中文/空格需强制拦截提示。
* [ ] **训练卡死检测**：如果 Python 进程崩溃，前端能否检测到？Electron 或 Qt 必须对 Python 子进程注册 `exit` 监听器，若非正常退出，必须立刻将页面上的“训练状态”置为“已中断/失败”，并弹出报错日志，不能让界面无限期处于“训练中...”的假死状态。
* [ ] **多张卡并发抢占**：在工控机上可能有多个显卡，或者有其他生产软件在占用 GPU。在配置界面中，必须让用户显式选择设备（如 `cpu`, `cuda:0`, `cuda:1`），而不是在后台写死 `cuda`。
* [ ] **大图片（4K+）标注卡顿**：工业相机拍摄的图片多为高分辨率。如果直接将原图加载到 Canvas 中，拖拽会极其卡顿。Fabric.js 在加载大图时，应在初始化时将图片绘制在离屏 Canvas 中，或使用缩略图渲染，配合原图坐标缩放映射，确保拖拽帧率 $\ge 60fps$。
