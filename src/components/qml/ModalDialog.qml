// ModalDialog.qml - 通用弹窗基类（对标参考UI）
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme

Popup {
    id: root
    modal: true
    closePolicy: Popup.CloseOnEscape
    padding: 0

    property string title: ""
    property int dialogWidth: 500
    property alias content: contentArea.children
    property alias footerContent: footerArea.children
    default property alias contentData: contentArea.data

    background: Rectangle {
        color: "transparent"
    }

    contentItem: Rectangle {
        implicitWidth: root.dialogWidth
        implicitHeight: column.height
        color: Theme.bgMain
        border.color: Theme.borderColor
        border.width: 1
        radius: Theme.radiusLarge

        Column {
            id: column
            width: parent.width
            spacing: 0

            Rectangle {
                width: parent.width
                height: 44
                color: Theme.bgCard
                radius: Theme.radiusLarge

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: parent.radius
                    color: parent.color
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.borderColor
                }

                Text {
                    anchors.centerIn: parent
                    text: root.title
                    font.pixelSize: Theme.fontSizeSubheading
                    font.weight: Font.DemiBold
                    color: Theme.textMain
                }

                Button {
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.spacingNormal
                    anchors.verticalCenter: parent.verticalCenter
                    width: 28
                    height: 28
                    flat: true
                    text: "✕"
                    font.pixelSize: Theme.fontSizeNormal
                    palette.buttonText: Theme.textMuted
                    onClicked: root.close()
                    background: Rectangle {
                        color: parent.hovered ? Theme.bgHover : "transparent"
                        radius: Theme.radiusSmall
                    }
                }
            }

            Item {
                id: contentAreaContainer
                width: parent.width
                height: contentArea.height + Theme.spacingLarge * 2

                Item {
                    id: contentArea
                    x: Theme.spacingLarge
                    y: Theme.spacingLarge
                    width: parent.width - Theme.spacingLarge * 2
                    height: childrenRect.height
                }
            }

            Rectangle {
                width: parent.width
                height: 52
                color: Theme.bgCard
                radius: Theme.radiusLarge

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: parent.radius
                    color: parent.color
                }

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 1
                    color: Theme.borderColor
                }

                Item {
                    id: footerArea
                    anchors.fill: parent
                    anchors.rightMargin: Theme.spacingLarge
                    anchors.leftMargin: Theme.spacingLarge

                    Row {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.spacingNormal
                    }
                }
            }
        }
    }

    Overlay.modeless: Rectangle {
        color: "#B3000000"
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animDuration }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.animDuration }
    }
}
