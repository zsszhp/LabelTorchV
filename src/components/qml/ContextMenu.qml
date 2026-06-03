// ContextMenu.qml - 右键菜单（对标参考UI）
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme

Menu {
    id: root
    padding: Theme.spacingSmall

    background: Rectangle {
        implicitWidth: 180
        color: Theme.bgCard
        border.color: Theme.borderColor
        border.width: 1
        radius: Theme.radiusNormal
    }

    delegate: MenuItem {
        id: menuItem
        implicitWidth: 180
        implicitHeight: 32

        contentItem: Text {
            text: menuItem.text
            font.pixelSize: Theme.fontSizeSmall
            color: menuItem.hovered ? Theme.textMain : Theme.textSecondary
            verticalAlignment: Text.AlignVCenter
            leftPadding: Theme.spacingLarge
        }

        background: Rectangle {
            color: menuItem.hovered ? Theme.bgHover : "transparent"
            radius: Theme.radiusSmall
            anchors.fill: parent
            anchors.margins: 2
        }
    }
}
