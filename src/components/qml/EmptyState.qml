// EmptyState.qml - 统一空态占位：大图标 + 弱光晕 + 主文案 + 次级说明 + 主按钮
// 无底盘色块，靠径向光晕托起图标；按钮统一走 LtButton
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
    // 图标与光晕主色（Apple 风格：空态默认中性灰，不与主操作争抢注意力）
    property color iconColor: Theme.textMuted

    signal actionClicked()
    signal secondaryClicked()

    implicitWidth: 360
    implicitHeight: contentColumn.implicitHeight

    ColumnLayout {
        id: contentColumn
        anchors.centerIn: parent
        width: parent.width
        spacing: Theme.spacingLarge

        // 大图标 + 弱光晕（径向渐变淡出，无底盘色块）
        Item {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Theme.emptyIconSize + 36
            Layout.preferredHeight: Theme.emptyIconSize + 36

            // 弱光晕：中心微亮、边缘完全透明，只做视觉托起
            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: width
                radius: width / 2
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.alpha(root.iconColor, 0.16) }
                    GradientStop { position: 0.55; color: Qt.alpha(root.iconColor, 0.05) }
                    GradientStop { position: 1.0; color: Qt.alpha(root.iconColor, 0.0) }
                }
            }

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
                        item.opacity = 0.95
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

        // 主按钮（统一 LtButton，primary 变体）
        LtButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Theme.spacingSmall
            visible: root.actionText !== ""
            enabled: root.actionEnabled
            text: root.actionText
            variant: "primary"
            onClicked: root.actionClicked()
        }

        // 次按钮（ghost 变体，弱化存在感）
        LtButton {
            Layout.alignment: Qt.AlignHCenter
            visible: root.secondaryText !== ""
            text: root.secondaryText
            variant: "ghost"
            compact: true
            onClicked: root.secondaryClicked()
        }
    }
}
