// ModalDialog.qml - 一体化弹窗基座
// 视觉：深色玻璃卡片 + 顶部强调条 + 标题区/内容区/底栏三段一体，避免像输入框
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import LabelTorch.Theme

Popup {
    id: root
    modal: true
    closePolicy: Popup.CloseOnEscape
    padding: 0

    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0

    property string title: ""
    property string subtitle: ""
    property int dialogWidth: 480
    property alias content: contentArea.children
    property alias footerContent: footerArea.children
    default property alias contentData: contentArea.data

    background: Item {
        // 外层只做阴影容器，圆角由 contentItem 绘制
        RectShadow {}
    }

    contentItem: Rectangle {
        implicitWidth: root.dialogWidth
        implicitHeight: bodyColumn.height
        color: Theme.dialogBodyBg
        radius: Theme.radiusLarge
        border.color: Theme.borderColor
        border.width: 1

        // 顶部强调条：识别弹窗身份，别像普通输入框
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 3
            radius: 1
            color: Theme.accentBar
        }

        Column {
            id: bodyColumn
            width: parent.width
            spacing: 0

            // 标题区
            Item {
                width: parent.width
                height: titleCol.height + 28

                Column {
                    id: titleCol
                    anchors.left: parent.left
                    anchors.right: closeBtn.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Theme.spacingXLarge
                    anchors.rightMargin: Theme.spacingNormal
                    spacing: 2

                    Text {
                        text: root.title
                        font.pixelSize: Theme.fontSizeLarge
                        font.weight: Font.DemiBold
                        font.family: Theme.fontFamily
                        color: Theme.textMain
                        elide: Text.ElideRight
                        width: parent.width
                    }
                    Text {
                        visible: root.subtitle !== ""
                        text: root.subtitle
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily
                        color: Theme.textMuted
                        elide: Text.ElideRight
                        width: parent.width
                    }
                }

                // 关闭
                Rectangle {
                    id: closeBtn
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.spacingNormal
                    anchors.verticalCenter: parent.verticalCenter
                    width: 30
                    height: 30
                    radius: Theme.radiusSmall
                    color: closeMouse.containsMouse ? Theme.bgHover : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: closeMouse.containsMouse ? Theme.textMain : Theme.textMuted
                        font.pixelSize: 13
                    }
                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }

                // 标题下分割
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.borderColor
                }
            }

            // 内容区
            Item {
                id: contentAreaContainer
                width: parent.width
                height: contentArea.height + Theme.spacingXLarge

                Item {
                    id: contentArea
                    x: Theme.spacingXLarge
                    y: Theme.spacingLarge
                    width: parent.width - Theme.spacingXLarge * 2
                    height: childrenRect.height
                }
            }

            // 底栏
            Rectangle {
                width: parent.width
                height: 56
                color: Theme.dialogFooterBg

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 1
                    color: Theme.borderColor
                }

                Item {
                    id: footerArea
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingXLarge
                    anchors.rightMargin: Theme.spacingXLarge
                }
            }
        }
    }

    // 阴影层
    component RectShadow: Item {
        anchors.fill: parent
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.shadowDialog
            shadowBlur: 0.6
            shadowVerticalOffset: 8
            shadowHorizontalOffset: 0
        }
    }

    Overlay.modal: Rectangle {
        color: Theme.overlayMask
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animDuration }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.animDuration; easing.type: Easing.OutCubic }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.animDurationFast }
    }
}
