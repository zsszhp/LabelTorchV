# QML 缺陷与布局警告修复 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 QML 加载期报错以解决项目页面按钮不可见问题，并消除布局警告。

**Architecture:** 通过在 `ProjectPage.qml` 中正确导入 `QtQuick.Effects` 解决 `MultiEffect` 加载失败导致的页面白屏问题；将 `AnnotationPage.qml` 的 Layout 直接子节点上的 `anchors` 属性改造为 `Layout.alignment` 以消除控制台警告。

**Tech Stack:** Qt 6.11.1, QML, C++

---

## 拟定修改 (Proposed Changes)

### Task 1: 修复 ProjectPage.qml 的 MultiEffect 导入

**Files:**
- Modify: `E:/z/project/my/LabelTorchV/src/features/project/qml/ProjectPage.qml:1-12`

- [ ] **Step 1: 修改 ProjectPage.qml 导入语句**
  在 `ProjectPage.qml` 顶部添加 `import QtQuick.Effects`。

- [ ] **Step 2: 验证编译与页面拉起**
  在构建路径重新编译运行，确认“新建项目”和“打开项目”按钮恢复显示。

- [ ] **Step 3: 提交 Git**

---

### Task 2: 消除 AnnotationPage.qml 布局锚点冲突警告

**Files:**
- Modify: `E:/z/project/my/LabelTorchV/src/features/annotation/qml/AnnotationPage.qml`

- [ ] **Step 1: 修复区域1图库选择器行布局中的 SvgIcon**
- [ ] **Step 2: 修复区域1数据集列表标题行布局中的 SvgIcon**
- [ ] **Step 3: 修复区域1数据集列表操作图标布局中的 SvgIcon**
- [ ] **Step 4: 修复区域2图像导航当前文件行布局中的 SvgIcon**
- [ ] **Step 5: 修复区域3标签类别列表标题行布局中的 SvgIcon**
- [ ] **Step 6: 验证布局警告是否消除**
- [ ] **Step 7: 提交 Git**

---

## 验证计划 (Verification Plan)

### 手动验证
1. 打开应用程序，确认左侧 "项目" 页面能够完整渲染，“新建项目”和“打开项目”按钮正常呈现。
2. 确认在 "标注" 页面及其子页面切换时，终端没有新增 `QML Row: Detected anchors...` 类似的警告。
