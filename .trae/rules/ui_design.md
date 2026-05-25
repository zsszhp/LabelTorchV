# UI 设计规范

## Theme.qml 设计系统（强制遵守）
- 所有UI组件必须使用Theme.xxx引用样式令牌
- 禁止硬编码颜色、字体、间距、圆角、动画时长
- 新增组件必须复用现有样式令牌，如需新增必须先在Theme.qml中定义

## 配色系统
- 深靛蓝背景：#0D0E15 → #1C1F30
- 粉红强调色：#FF4A70
- 紫色辅助：#8B5CF6
- 类别配色：10色循环数组classColors

## 字体系统
- UI字体：Segoe UI / Microsoft YaHei
- 等宽字体：Cascadia Code / Consolas
- 字号通过Theme.fontSizeXxx引用

## 组件一致性
- 按钮样式统一使用Theme定义的圆角、内边距、悬停效果
- 输入框样式统一，列表项高度间距统一，对话框风格统一
