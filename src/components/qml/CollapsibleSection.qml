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
            height: 28
            color: "transparent"

            Row {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingNormal
                spacing: Theme.spacingSmall

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.expanded ? "▾" : "▸"
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textMuted
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.title
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: Font.DemiBold
                    color: Theme.textMuted
                    font.letterSpacing: 0.5
                }
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
