// Splitter.qml - 可拖拽分割线（对标参考UI）
import QtQuick
import LabelTorch.Theme

MouseArea {
    id: root
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
        
        Behavior on width { NumberAnimation { duration: 150 } }
        Behavior on height { NumberAnimation { duration: 150 } }
        Behavior on color { ColorAnimation { duration: 150 } }
    }
}
