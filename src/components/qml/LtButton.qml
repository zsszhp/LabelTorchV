// LtButton.qml - 统一按钮基元
// 变体：primary / secondary / danger / ghost；尺寸：常规 36、紧凑 32
// 背景与文字色全部走 Theme token，业务侧禁止再手写按钮背景
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme

Button {
    id: root

    // 视觉变体：primary 主操作 / secondary 次操作 / danger 危险 / ghost 轻量
    property string variant: "primary"
    // 紧凑模式：高度 32，常规 36
    property bool compact: false
    // 加载中：内容降权 + 转圈 + 吞掉点击，防止重复提交
    property bool loading: false
    // 可选前置图标（SvgIcon 图标名，空串则不显示）
    property string icon: ""

    // 统一高度，避免业务侧各写各的
    readonly property int controlHeight: compact ? Theme.buttonHeightCompact : Theme.buttonHeight

    // 变体配色（内部集中定义，禁止外部再拼背景）
    readonly property color fillColor: {
        if (!enabled)
            return Theme.bgHover
        if (variant === "primary")
            return pressed ? Theme.primaryDark : (hovered ? Theme.primaryGlow : Theme.primary)
        if (variant === "danger")
            return pressed ? Qt.darker(Theme.danger, 1.15) : Theme.danger
        if (variant === "secondary")
            return pressed ? Theme.bgSelected : (hovered ? Theme.bgHover : Theme.bgCard)
        // ghost
        return pressed ? Theme.bgSelected : (hovered ? Theme.bgHover : "transparent")
    }

    readonly property color textColor: {
        if (!enabled)
            return Theme.textDisabled
        if (variant === "primary")
            // 主色为亮青，配深色字对比更清晰
            return Theme.bgMain
        if (variant === "danger")
            return Theme.bgMain
        if (variant === "secondary")
            return Theme.textMain
        // ghost
        return Theme.textSecondary
    }

    readonly property color borderColor: {
        if (!enabled)
            return Theme.borderColor
        if (variant === "secondary")
            return hovered ? Theme.borderHover : Theme.borderColor
        if (variant === "ghost")
            return "transparent"
        return "transparent"
    }

    implicitWidth: Math.max(contentRow.implicitWidth + Theme.spacingLarge * 2, 72)
    implicitHeight: Theme.safeSize(controlHeight, 36)
    padding: 0
    leftPadding: Theme.spacingLarge
    rightPadding: Theme.spacingLarge
    focusPolicy: Qt.StrongFocus

    // 加载中在最上层吞掉点击，不改写外部 enabled，便于恢复交互
    MouseArea {
        anchors.fill: parent
        enabled: root.loading
        hoverEnabled: true
        z: 10
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: root.fillColor
        border.color: root.borderColor
        border.width: (root.variant === "secondary" || root.variant === "ghost") ? 1 : 0

        // 键盘焦点环
        Rectangle {
            anchors.fill: parent
            radius: parent.radius + 2
            color: "transparent"
            border.color: Theme.focusRing
            border.width: Theme.focusRingWidth
            visible: root.activeFocus && !root.loading
            z: -1
        }
    }

    contentItem: RowLayout {
        id: contentRow
        spacing: Theme.spacingSmall
        // 加载中内容降权，转圈提示
        opacity: root.loading ? 0.45 : 1.0

        // 加载转圈（Canvas 圆弧，避免引入额外控件依赖）
        Canvas {
            id: spinner
            visible: root.loading
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            Layout.alignment: Qt.AlignVCenter
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.lineWidth = 2
                ctx.lineCap = "round"
                ctx.strokeStyle = root.textColor
                ctx.beginPath()
                ctx.arc(width / 2, height / 2, width / 2 - 2, 0, Math.PI * 1.35)
                ctx.stroke()
            }
            RotationAnimation on rotation {
                running: root.loading
                loops: Animation.Infinite
                from: 0
                to: 360
                duration: 720
            }
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }

        // 可选前置图标：经 qrc 加载 Shell 的 SvgIcon，保持图标风格统一
        Loader {
            visible: root.icon !== ""
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            Layout.alignment: Qt.AlignVCenter
            source: root.icon !== "" ? "qrc:/qt/qml/LabelTorch/Shell/qml/SvgIcon.qml" : ""
            onLoaded: {
                if (item) {
                    item.icon = Qt.binding(function () { return root.icon })
                    item.color = Qt.binding(function () { return root.textColor })
                    item.width = Qt.binding(function () { return 14 })
                    item.height = Qt.binding(function () { return 14 })
                }
            }
        }

        Text {
            Layout.alignment: Qt.AlignVCenter
            text: root.text
            color: root.textColor
            font.pixelSize: root.compact ? Theme.fontSizeSmall : Theme.fontSizeNormal
            font.family: Theme.fontFamily
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
}
