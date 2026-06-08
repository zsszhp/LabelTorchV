# QML 缺陷与布局警告修复设计文档 (qml-bug-fix-design)

## 1. 目标与背景
修复 QML 中已知的阻塞性报错和性能警告，以提升应用稳定性和控制台日志的纯净度：
1. **阻塞报错**：`ProjectPage.qml` 中缺失 `QtQuick.Effects` 导入，导致 `MultiEffect is not a type`。
2. **布局警告**：`AnnotationPage.qml` 等页面的 Layout 直接子项中误用了 `anchors`，导致 Qt 框架抛出无效锚点忽略警告。

---

## 2. 拟修改方案

### 2.1 修复 MultiEffect 导入
- 涉及文件：[ProjectPage.qml](file:///E:/z/project/my/LabelTorchV/src/features/project/qml/ProjectPage.qml)
- 动作：在文件开头增加以下导入语句：
  ```qml
  import QtQuick.Effects
  ```

### 2.2 修复布局子项锚点警告
- 涉及文件：[AnnotationPage.qml](file:///E:/z/project/my/LabelTorchV/src/features/annotation/qml/AnnotationPage.qml)
- 动作：
  1. 查找在 `RowLayout` 或 `ColumnLayout` 的直接子节点（如 `SvgIcon`、`Rectangle`、`Text` 等）中误用 `anchors.verticalCenter: parent.verticalCenter` 的地方。
  2. 将其替换为标准的 `Layout.alignment: Qt.AlignVCenter`，移除所有 `anchors` 相关属性。
  
示例：
```diff
  SvgIcon {
      icon: "images"
      width: 14
      height: 14
      color: Theme.primaryGlow
-     anchors.verticalCenter: parent.verticalCenter
+     Layout.alignment: Qt.AlignVCenter
  }
```

---

## 3. 验证方案
1. 运行 `cmake` 构建，确保 C++ 无编译错误。
2. 运行 `LabelTorchV.exe` 启动程序，切换至 "项目" 和 "标注" 页面。
3. 检查控制台输出，确认不再有 `MultiEffect is not a type` 报错及 `QML Row: Detected anchors...` 布局警告。
