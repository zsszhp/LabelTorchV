// CollapsibleSection.qml - 可折叠区块（对标参考UI）
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme

Item {
    id: root
    width: parent ? parent.width : 200
    implicitHeight: column.height

    property string title: ""
    property bool expanded: true
    property alias content: contentArea.children
    default property alias contentData: contentArea.data

    Column {
        id: column
        width: parent.width
        spacing: 0

        Rectangle {
            width: parent.width
            height: 36
            color: Qt.rgba(1, 1, 1, 0.01)

            Row {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 10
                anchors.bottomMargin: 10
                spacing: Theme.spacingSmall

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.expanded ? "▾" : "▸"
                    font.pixelSize: 12
                    color: Theme.textMuted
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.title
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: Theme.textMuted
                }
            }

            // 底部分割线
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.borderColor
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.expanded = !root.expanded
            }
        }

        Item {
            id: contentArea
            width: parent.width
            height: root.expanded ? childrenRect.height : 0
            visible: root.expanded
            clip: true
            Behavior on height { NumberAnimation { duration: Theme.animDuration; easing.type: Easing.InOutQuad } }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.dividerColor
            visible: !root.expanded
        }
    }
}
