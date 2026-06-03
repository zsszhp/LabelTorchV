// ToggleSwitch.qml - 开关控件（34x20px，对标参考UI）
import QtQuick
import LabelTorch.Theme

Item {
    id: root
    width: Theme.toggleWidth
    height: Theme.toggleHeight
    implicitWidth: Theme.toggleWidth
    implicitHeight: Theme.toggleHeight

    property bool checked: false
    property bool small: false

    signal toggled()

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? Theme.primary : Theme.borderColor
        Behavior on color { ColorAnimation { duration: Theme.animDuration } }

        Rectangle {
            id: thumb
            width: root.small ? Theme.toggleSmallHeight - 4 : Theme.toggleHeight - 4
            height: width
            radius: width / 2
            color: "#FFFFFF"
            y: (parent.height - height) / 2
            x: root.checked ? parent.width - width - 2 : 2
            Behavior on x { NumberAnimation { duration: Theme.animDuration; easing.type: Easing.InOutQuad } }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.checked = !root.checked
            root.toggled()
        }
    }
}
