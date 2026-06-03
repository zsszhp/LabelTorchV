// SectionTitle.qml - 区块标题（青色左边框，对标参考UI）
import QtQuick
import LabelTorch.Theme

Item {
    id: root
    width: parent ? parent.width : 200
    height: 24
    implicitHeight: 24

    property string text: ""
    property alias rightContent: rightArea.children

    Rectangle {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: 3
        height: parent.height - 4
        color: Theme.primaryGlow
        radius: 1
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: root.text
        font.pixelSize: Theme.fontSizeSmall
        font.weight: Font.DemiBold
        color: Theme.textMuted
        font.letterSpacing: 0.5
    }

    Item {
        id: rightArea
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: parent.height
    }
}
