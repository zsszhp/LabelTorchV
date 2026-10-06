// Splitter.qml - 可拖拽分割线（对标参考UI）
import QtQuick
import LabelTorch.Theme

MouseArea {
    id: root
    // 在 RowLayout/ColumnLayout 中使用时布局按 implicit 尺寸求解：
    // 缺 implicitWidth/Height 会塌成 0 宽/高，拖拽整体失效（CheckPage 踩过）。
    // 布局内需拉伸时由使用方给 Layout.fillHeight/fillWidth。
    implicitWidth: vertical ? 6 : 0
    implicitHeight: vertical ? 0 : 6
    width: vertical ? 6 : parent ? parent.width : 200
    height: vertical ? (parent ? parent.height : 200) : 6
    hoverEnabled: true
    cursorShape: vertical ? Qt.SplitHCursor : Qt.SplitVCursor

    property bool vertical: true
    property bool reverse: false
    property real minSize: 120
    property real maxSize: 600
    property var targetItem: null
    property bool dragging: root.pressed

    onPositionChanged: {
        if (pressed && targetItem) {
            if (vertical) {
                var newWidth = targetItem.width + (reverse ? -mouseX : mouseX)
                targetItem.width = Math.max(minSize, Math.min(maxSize, newWidth))
            } else {
                var newHeight = targetItem.height + (reverse ? -mouseY : mouseY)
                targetItem.height = Math.max(minSize, Math.min(maxSize, newHeight))
            }
        }
    }

    Rectangle {
        id: splitBg
        anchors.horizontalCenter: root.vertical ? parent.horizontalCenter : undefined
        anchors.verticalCenter: !root.vertical ? parent.verticalCenter : undefined
        
        width: root.vertical ? (root.containsMouse || root.pressed ? 3 : 1) : parent.width
        height: root.vertical ? parent.height : (root.containsMouse || root.pressed ? 3 : 1)

        color: root.pressed ? Theme.primaryGlow : (root.containsMouse ? Theme.primaryGlow : Theme.borderColor)
        
        Behavior on width { NumberAnimation { duration: Theme.animDurationFast } }
        Behavior on height { NumberAnimation { duration: Theme.animDurationFast } }
        Behavior on color { ColorAnimation { duration: Theme.animDurationFast } }
    }
}
