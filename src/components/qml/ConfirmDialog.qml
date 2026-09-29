// ConfirmDialog.qml - 危险操作统一二次确认弹窗
// 默认焦点落在「取消」；列明连带删除项，避免误操作
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme

ModalDialog {
    id: root

    // 确认标题，如「确认删除项目」
    property string confirmTitle: "确认操作"
    // 描述文案
    property string message: "此操作不可撤销，是否继续？"
    // 连带影响项文本行，例如 ["数据集 3 个", "样本 1200 张"]
    property var impactItems: []
    // 确认按钮文案
    property string confirmText: "确认删除"
    // 取消按钮文案
    property string cancelText: "取消"
    // 是否显示输入校验（输入项目名确认）
    property string requireTyping: ""

    // 内部状态：用户输入
    property string typedText: ""

    signal confirmed()
    signal cancelled()

    title: root.confirmTitle
    dialogWidth: Theme.confirmDialogWidth

    function openConfirm() {
        typedText = ""
        open()
        // 弹窗打开后把焦点落到取消按钮，防止误触回车直接确认
        Qt.callLater(function () {
            if (cancelBtn)
                cancelBtn.forceActiveFocus()
        })
    }

    ColumnLayout {
        width: parent.width - Theme.spacingLarge * 2
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Theme.spacingNormal

        // 主描述
        Text {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingNormal
            text: root.message
            color: Theme.textMain
            font.pixelSize: Theme.fontSizeNormal
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }

        // 连带影响项
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingNormal
            visible: root.impactItems && root.impactItems.length > 0
            radius: Theme.radiusSmall
            color: Qt.alpha(Theme.danger, 0.08)
            border.color: Qt.alpha(Theme.danger, 0.35)
            border.width: 1
            implicitHeight: impactCol.implicitHeight + Theme.spacingLarge * 2

            ColumnLayout {
                id: impactCol
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacingSmall

                Text {
                    Layout.fillWidth: true
                    text: "将一并删除以下内容："
                    color: Theme.danger
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    font.weight: Font.DemiBold
                }

                Repeater {
                    model: root.impactItems || []

                    delegate: Text {
                        required property var modelData
                        Layout.fillWidth: true
                        text: "· " + modelData
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        // 需输入确认（高危操作）
        ColumnLayout {
            visible: root.requireTyping !== ""
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingNormal
            spacing: Theme.spacingSmall

            Text {
                Layout.fillWidth: true
                text: "请输入「" + root.requireTyping + "」以确认："
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSmall
                font.family: Theme.fontFamily
            }

            TextField {
                id: typingField
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                text: root.typedText
                placeholderText: root.requireTyping
                placeholderTextColor: Theme.textDisabled
                color: Theme.textMain
                font.pixelSize: Theme.fontSizeNormal
                font.family: Theme.fontFamily
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.bgInput
                    border.color: typingField.activeFocus ? Theme.primary : Theme.borderColor
                    border.width: 1
                }
                onTextChanged: root.typedText = text
            }
        }

        // 危险提示
        Text {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingNormal
            Layout.bottomMargin: Theme.spacingNormal
            text: "此操作不可撤销。"
            color: Theme.danger
            font.pixelSize: Theme.fontSizeCaption
            font.family: Theme.fontFamily
        }
    }

    footerContent: Row {
        spacing: Theme.spacingLarge
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        // 取消（左侧，且为默认焦点）
        Button {
            id: cancelBtn
            text: root.cancelText
            width: 90
            // 默认焦点在取消上
            focus: true
            activeFocusOnTab: true
            background: Rectangle {
                color: parent.hovered ? Theme.bgHover : Theme.bgCard
                border.color: parent.activeFocus ? Theme.primary : Theme.borderColor
                border.width: 1
                radius: Theme.radiusSmall
                implicitHeight: 32
            }
            contentItem: Text {
                text: parent.text
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeNormal
                font.family: Theme.fontFamily
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                root.cancelled()
                root.close()
            }
        }

        // 确认（右侧，危险色）
        Button {
            id: confirmBtn
            text: root.confirmText
            width: 100
            // 需输入确认时，未匹配前禁用
            enabled: root.requireTyping === "" || root.typedText === root.requireTyping
            background: Rectangle {
                color: {
                    if (!parent.enabled)
                        return Theme.bgCard
                    return parent.pressed ? Qt.darker(Theme.danger, 1.2) : (parent.hovered ? Qt.lighter(Theme.danger, 1.1) : Theme.danger)
                }
                radius: Theme.radiusSmall
                implicitHeight: 32
            }
            contentItem: Text {
                text: parent.text
                color: parent.enabled ? Theme.textMain : Theme.textDisabled
                font.pixelSize: Theme.fontSizeNormal
                font.bold: true
                font.family: Theme.fontFamily
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                root.confirmed()
                root.close()
            }
        }
    }

    onClosed: {
        typedText = ""
    }
}
