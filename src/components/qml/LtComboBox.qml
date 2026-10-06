// LtComboBox.qml - 统一深色下拉（Basic 默认样式在深底上是白块，全站统一走此组件）
// 用法与 ComboBox 一致；需要自定义弹层文案时仍可内联覆盖 delegate
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme

ComboBox {
    id: control

    implicitHeight: 32
    // 空内容时不塌缩，保持可点击的合理宽度
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding,
                            140)
    font.pixelSize: Theme.fontSizeSmall
    font.family: Theme.fontFamily

    background: Rectangle {
        color: control.hovered ? Theme.bgInputDropdown : Theme.bgInput
        radius: Theme.radiusSmall
        border.color: control.activeFocus ? Theme.primaryGlow : Theme.borderColor
        border.width: 1
    }

    contentItem: Loader {
        // Control 只定位 contentItem 不拉伸，需显式绑定尺寸（否则 Loader 塌缩为 0）
        width: control.availableWidth
        height: control.availableHeight
        sourceComponent: control.editable ? textInputItem : textItem
    }

    Component {
        id: textItem
        Text {
            anchors.fill: parent
            text: control.displayText
            font: control.font
            color: Theme.textMain
            leftPadding: Theme.spacingNormal
            rightPadding: Theme.spacingLarge
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    // editable 模式需要可输入的 contentItem（Basic 样式约定）
    Component {
        id: textInputItem
        TextInput {
            anchors.fill: parent
            text: control.editText
            font: control.font
            color: Theme.textMain
            selectionColor: Theme.focusRing
            leftPadding: Theme.spacingNormal
            rightPadding: Theme.spacingLarge
            verticalAlignment: Text.AlignVCenter
            readOnly: !control.editable
            autoScroll: control.editable
            onTextEdited: control.editText = text
            onAccepted: control.editText = text
        }
    }

    indicator: Text {
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacingSmall
        anchors.verticalCenter: parent.verticalCenter
        text: "\u25BE"
        font.pixelSize: 12
        color: Theme.textMuted
    }

    delegate: ItemDelegate {
        id: ltComboDelegate
        width: control.width
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            // 兼容两种模型：textRole 指定的角色模型 / JS 数组的 modelData
            text: control.textRole !== ""
                  ? (model[control.textRole] !== undefined ? model[control.textRole] : "")
                  : (modelData !== undefined ? modelData : "")
            font.pixelSize: Theme.fontSizeSmall
            font.family: Theme.fontFamily
            color: ltComboDelegate.highlighted ? Theme.textMain : Theme.textSecondary
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: ltComboDelegate.hovered ? Theme.bgHover : Theme.bgInputDropdown
        }
    }

    popup: Popup {
        y: control.height + 2
        width: control.width
        padding: 1
        background: Rectangle {
            color: Theme.bgInputDropdown
            border.color: Theme.borderColor
            radius: Theme.radiusSmall
        }
        contentItem: ListView {
            clip: true
            // 无上限时长列表（几十条）的下拉层会超出窗口被截断
            implicitHeight: Math.min(contentHeight, 300)
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
        }
    }
}
