// Splitter.qml - 可拖拽分割线（对标参考UI）
import QtQuick
import LabelTorch.Theme

MouseArea {
    id: root
    width: vertical ? 4 : parent ? parent.width : 200
    height: vertical ? (parent ? parent.height : 200) : 4
    hoverEnabled: true
    cursorShape: vertical ? Qt.SplitHCursor : Qt.SplitVCursor

    property bool vertical: true
    property real minSize: 120
    property real maxSize: 600
    property var targetItem: null
    property bool dragging: root.pressed

    onPressed: splitBg.color = Theme.primaryGlow
    onReleased: splitBg.color = mouse.containsMouse ? Qt.lighter(Theme.borderColor, 1.2) : Theme.borderColor
    onContainsMouseChanged: {
        if (!pressed) splitBg.color = containsMouse ? Qt.lighter(Theme.borderColor, 1.2) : Theme.borderColor
    }

    onPositionChanged: {
        if (pressed && targetItem) {
            if (vertical) {
                var newWidth = targetItem.width + mouseX
                targetItem.width = Math.max(minSize, Math.min(maxSize, newWidth))
            } else {
                var newHeight = targetItem.height + mouseY
                targetItem.height = Math.max(minSize, Math.min(maxSize, newHeight))
            }
        }
    }

    Rectangle {
        id: splitBg
        anchors.fill: parent
        color: Theme.borderColor
        Behavior on color { ColorAnimation { duration: 150 } }
    }
}
