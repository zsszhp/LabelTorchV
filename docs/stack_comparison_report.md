# 标炬（LabelTorch）技术栈评估报告

> 版本：2.0 | 更新日期：2026-05-31
> 状态：选型评估完成，已决策

终极目标：**"开发出一款稳定、美观、具备高级标注/辅助打标/训练一体化的工业缺陷检测客户端，并能让 AI 快速、无偏差地完全实现出来"**

经过代码资产评估与多维度对比后，决策结论为：**继续使用 Qt 6 + C++17 + QML + Python 架构。**

---

## 一、 基于终极目标的核心维度对比

### 1. AI 自动编写的成功率 (AI-Driven Implementation)
* **C++ Qt / QML**：AI 对 C++ 的底层编译报错（如 MSVC 链接错误、CMake 宏缺失）和 QML 的运行时报错处理能力中等。但项目已积累大量可工作的代码模板和模式，AI 可基于现有代码进行增量修改。
* **React + Electron**：React/TS 是 AI 最擅长的领域，但从零重建前端意味着放弃已有的 24,500+ 行代码。

**结论**：基于现有代码的增量 AI 开发效率 > 从零 AI 重写效率。

### 2. 核心标注画布开发效率 (Canvas Engine for HBB/OBB/Polygon)
* **C++ Qt / QML**：需要基于 `QQuickPaintedItem` + OpenGL 手动实现旋转矩形、碰撞检测等。项目已有 `CanvasController`、`AxisAlignedBox`、`RotatedBox`、`Polygon` 几何内核基础。
* **React + Electron**：Fabric.js 开箱即用支持 HBB/OBB 拖拽。

**结论**：画布开发确实是 Qt 方案的弱项，但几何内核已有基础，v1.0 可在此之上扩展。

### 3. UI 界面美学与交互顺畅度
* **C++ Qt / QML**：已迭代到 V4 赛博蓝灰配色方案，`Theme.qml` 全局主题系统已建立。缺少 Tailwind 生态，但 QML 本身支持动画、渐变、阴影等现代 UI 效果。
* **React + Electron**：Tailwind + Shadcn/Ant Design 组件库齐全，快速出效果。

**结论**：主题系统已就位，继续在 QML 上迭代美化。

### 4. 离线绿色打包与显卡隔离
* **C++ Qt / QML**：Qt 通过 `windeployqt` 打包，体积较小（~100-200MB 前端，不含 Python 运行时）。`QProcess` 管理 Python 子进程，崩溃隔离完善。
* **React + Electron**：Electron 壳体自带 ~200MB Chromium，配合 `electron-builder` 打包。`child_process` 管理 Python。

**结论**：Qt 打包体积更小，适合工控机部署。

---

## 二、 基于 Qt 6 技术栈的开发路径规划

基于已有代码资产和版本路线图，开发拆解为以下阶段：

```
                    ┌───────────────────────────────┐
                    │       第一阶段：MVP 收尾       │
                    │   - 修复 P0/P1 级别 Bug       │
                    │   - 集成测试核心闭环           │
                    │   - 绿色版打包发布             │
                    └───────────────┬───────────────┘
                                    │
                                    ▼
                    ┌───────────────────────────────┐
                    │      第二阶段：标注画布引擎     │
                    │   - QQuickPaintedItem 画布     │
                    │   - HBB/OBB 绘制与交互         │
                    │   - YOLO txt 实时写回           │
                    └───────────────┬───────────────┘
                                    │
                                    ▼
                    ┌───────────────────────────────┐
                    │      第三阶段：智能辅助标注     │
                    │   - 模型推理候选框生成          │
                    │   - 人工审核确认工作流          │
                    │   - 增量训练与模型谱系追踪      │
                    └───────────────────────────────┘
```

### 第一阶段：MVP 收尾（v0.1.0 发布）
* 修复路径硬编码、状态名不一致、PYTHONUNBUFFERED 等已知 Bug
* 冷启动任务状态自检逻辑实现
* 导入 → 训练 → 导出端到端集成测试
* Qt IFW (Qt Installer Framework) 或绿色版 7z 打包

### 第二阶段：标注画布引擎（v1.0.0）
* 基于现有 `CanvasController` + `RotatedBox` 几何内核
* `QQuickPaintedItem` 实现大图分层渲染
* 快捷键系统（参照 `industry_standards.md` 规范）
* 撤销/重做（基于 `annotation_revisions` 表）

### 第三阶段：智能辅助标注（v1.1.0+）
* 选择历史模型进行推理预标注
* 虚线候选框显示 + 一键审核
* 增量训练支持
