// EmptyState.qml - 统一空态占位：图标 + 主文案 + 次级说明 + 下一步主按钮
// 替换各页面纯文字空态，给用户明确的下一步引导
// 图标通过 qrc 路径加载 Shell 层 SvgIcon，避免 components↔shell 模块循环依赖
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme

Item {
    id: root

    // 图标名（SvgIcon 支持的图标名，如 folder / images / brain / export）
    property string icon: "folder"
    // 主文案
    property string title: "暂无数据"
    // 次级说明
    property string description: ""
    // 主按钮文案；为空则不显示按钮
    property string actionText: ""
    // 次按钮文案；为空则不显示
    property string secondaryText: ""
    // 按钮是否可用（例如未打开项目时禁用）
    property bool actionEnabled: true
    // 图标底色（默认主色微光）
    property color iconColor: Theme.primaryGlow

    signal actionClicked()
    signal secondaryClicked()

    implicitWidth: 360
    implicitHeight: contentColumn.implicitHeight

    ColumnLayout {
        id: contentColumn
        anchors.centerIn: parent
        width: parent.width
        spacing: Theme.spacingLarge

        // 图标底盘
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Theme.emptyIconSize + Theme.spacingXLarge
            Layout.preferredHeight: Theme.emptyIconSize + Theme.spacingXLarge
            radius: Theme.radiusLarge
            color: Qt.alpha(root.iconColor, 0.08)
            border.color: Qt.alpha(root.iconColor, 0.25)
            border.width: 1

            // 经 qrc 加载 Shell 的 SvgIcon，保持图标风格统一且不引入模块依赖
            Loader {
                anchors.centerIn: parent
                width: Theme.emptyIconSize
                height: Theme.emptyIconSize
                source: "qrc:/qt/qml/LabelTorch/Shell/qml/SvgIcon.qml"
                onLoaded: {
                    if (item) {
                        item.icon = Qt.binding(function () { return root.icon })
                        item.color = Qt.binding(function () { return root.iconColor })
                        item.width = Qt.binding(function () { return Theme.emptyIconSize })
                        item.height = Qt.binding(function () { return Theme.emptyIconSize })
                        item.opacity = 0.9
                    }
                }
            }
        }

        // 主文案
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            text: root.title
            color: Theme.textMain
            font.pixelSize: Theme.fontSizeSubheading
            font.weight: Font.DemiBold
            font.family: Theme.fontFamily
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        // 次级说明
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            visible: root.description !== ""
            text: root.description
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeNormal
            font.family: Theme.fontFamily
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        // 主按钮
        Button {
            Layout.alignment: Qt.AlignHCenter
            visible: root.actionText !== ""
            enabled: root.actionEnabled
            text: root.actionText
            implicitHeight: 34
            leftPadding: Theme.spacingXLarge
            rightPadding: Theme.spacingXLarge

            background: Rectangle {
                radius: Theme.radiusSmall
                color: {
                    if (!parent.enabled)
                        return Theme.bgCard
                    return parent.pressed ? Qt.darker(Theme.primary, 1.2) : (parent.hovered ? Theme.primaryDark : Theme.primary)
                }
            }
            contentItem: Text {
                text: parent.text
                color: parent.enabled ? Theme.textMain : Theme.textDisabled
                font.pixelSize: Theme.fontSizeNormal
                font.family: Theme.fontFamily
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: root.actionClicked()
        }

        // 次按钮
        Button {
            Layout.alignment: Qt.AlignHCenter
            visible: root.secondaryText !== ""
            text: root.secondaryText
            implicitHeight: 32
            leftPadding: Theme.spacingLarge
            rightPadding: Theme.spacingLarge

            background: Rectangle {
                radius: Theme.radiusSmall
                color: parent.hovered ? Theme.bgHover : Theme.bgCard
                border.color: Theme.borderColor
                border.width: 1
            }
            contentItem: Text {
                text: parent.text
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeNormal
                font.family: Theme.fontFamily
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: root.secondaryClicked()
        }
    }
}
