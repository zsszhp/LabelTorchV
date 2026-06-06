# 工业缺陷检测智能一体化平台 (LabelTorchV) - 像素级 UI 升级设计规格书

本文档定义了 LabelTorchV 顶栏导航栏、侧栏及全软件核心图标的像素级美化及 Bug 修复设计方案。

## 1. 目标与视觉基准
对标参考设计 `reference/Dihuge DLTools/gemini-code-1780391652706.html`，彻底解决当前 UI 中“卡通 Emoji 图标多”、“顶栏未打开项目时导航坍塌挤压重合”、“高亮发光特效缺失”等严重破坏专业感的视觉 Bug，呈现高品质、前沿暗黑工业级 UI 效果。

## 2. 核心改进方案

### 2.1 矢量自绘制图标组件 (`SvgIcon.qml`) [NEW]
* **技术路线**：在 `LabelTorch.Shell` 模块中新增矢量自绘组件，通过 `QtQuick.Shapes` 的 `PathSvg` 直接解析 24x24 视口的 SVG 路径，支持颜色过渡动效及无级平滑缩放。
* **支持的图标键值**：
  * `folder`: 文件夹 / 项目
  * `images`: 图片 / 数据集
  * `edit`: 贝塞尔曲线 / 钢笔标注
  * `check`: 核对清单 / 数据检查
  * `brain`: 神经网络 CPU 芯片 / 模型训练
  * `flask`: 试管实验 / 模型测试
  * `export`: 文件导出
  * `plus`: 新增/添加按钮 (+)
  * `eye`: 可见性 (👁)
  * `marker`: 定位标签 (📍)
  * `trash`: 删除/垃圾箱 (🗑)
  * `close`: 清除/关闭 (✕)
  * `arrow-down`: 折叠下三角 (▾)
  * `signal`: 设备状态信号
  * `gear`: 设置齿轮
  * `user`: 用户头像

### 2.2 顶栏导航重构与 Bug 修复 (`Main.qml`) [MODIFY]
* **宽度循环计算 Bug 修复**：
  * 移除 `ItemDelegate` 内部 Row 的 `anchors.centerIn: parent`；
  * 移除 `ItemDelegate` 身上的 `implicitWidth: navContentRow.width + 44` 绑定；
  * 设置 `ItemDelegate` 的 `leftPadding: 22` 和 `rightPadding: 22`。使宽度计算自然委托于 QML 引擎内置机制（`leftPadding + contentItem.implicitWidth + rightPadding`），彻底消除 Binding Loop 导致的坍缩。
* **像素级渐变背景复刻**：
  * 将 Active 导航项背景 Rectangle 的 `color` 替换为 `gradient`。
  * 渐变色：从顶部的 `transparent` (位置 0.0) 到中下部的 `transparent` (位置 0.7) 再到最底部的 `rgba(0, 229, 255, 0.05)` (位置 1.0)。
* **微光外发光效果 (Glow Effect)**：
  * 在导航 Row 标签的外层或 Active 标签中引入 `layer.effect: MultiEffect`。
  * 为 Active 标签整体应用发光阴影：`shadowEnabled: true`, `shadowColor: Theme.primaryGlow`, `shadowBlur: 0.3`。
* **Logo 图标边缘发光增强**：
  * 将 Logo 的外层 shadow 效果修改为 `Theme.primaryGlow` 配合 `shadowBlur: 0.3`，提供更醇厚立体的发光感。
* **右侧工具按钮复刻**：
  * 引入 `signal`, `gear`, `user` 三个 `SvgIcon`，对标 HTML 顶栏右侧的工具按钮。同时精致保留现有的 GPU、后端状态灯。

### 2.3 核心功能页面 Emoji 彻底清除 [MODIFY]
* **`ProjectPage.qml` (项目中心)**：
  * 新建项目、打开项目按钮图标及关闭(✕)、编辑(✎)字符替换为精致的 `SvgIcon`。
  * 列表卡片 hover 阴影精度对标。
* **`DatasetPage.qml` (数据集页面)**：
  * 数据集列表、图库、Tag、属性面板里的卡通 Emoji 统一替换为对应的 `SvgIcon`；
  * 右侧缩略图卡片 hover 发光与选中态外光圈微调。
* **`AnnotationPage.qml` (标注工作台)**：
  * 侧栏四大区块、前后翻页按钮、标签类别列表、Tag 网格的 Emoji 和粗糙字符替换为 `SvgIcon`。

---

## 3. 验证与回归规划

### 3.1 编译与静态验证
* 运行 CMake 配置与构建。
* 确认 0 编译错误。

### 3.2 自动化测试
* 运行全量单元测试：`ctest --preset x64-release` 确认 10/10 Passed。

### 3.3 手动运行测试 (由用户配合执行)
1. 运行生成的 `LabelTorchV.exe`。
2. 检查未打开项目时顶栏导航的宽度排布，确认没有任何坍塌挤压重叠。
3. 鼠标悬浮导航标签，检查 Hover 渐变底色和文字高亮过渡是否平滑。
4. 点击“打开项目”或“新建项目”，检查激活态 Tab 底部是否出现亮青色渐变，且文字和图标有精美的外发光微光特效。
5. 检查侧栏“数据集”、“图像导航”、“标签类别”、“Tag”的图标，确认已全部升级为极致清透的矢量图标。
