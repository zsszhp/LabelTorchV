// RingProgress.qml - 环形进度图（对标参考UI测试页）
import QtQuick
import LabelTorch.Theme

Item {
    id: root
    width: 64
    height: 64
    implicitWidth: 64
    implicitHeight: 64

    property real value: 0
    property color ringColor: Theme.primary
    property string centerText: ""
    property string labelText: ""

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()

            var cx = width / 2
            var cy = height / 2
            var r = Math.min(cx, cy) - 4

            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, 2 * Math.PI)
            ctx.strokeStyle = Theme.borderColor
            ctx.lineWidth = 4
            ctx.stroke()

            var startAngle = -Math.PI / 2
            var endAngle = startAngle + (root.value / 100) * 2 * Math.PI

            ctx.beginPath()
            ctx.arc(cx, cy, r, startAngle, endAngle)
            ctx.strokeStyle = root.ringColor
            ctx.lineWidth = 4
            ctx.lineCap = "round"
            ctx.stroke()
        }

        onValueChanged: requestPaint()
    }

    Text {
        anchors.centerIn: parent
        text: root.centerText || Math.round(root.value) + "%"
        font.pixelSize: Theme.fontSizeCaption
        font.family: Theme.fontFamilyMono
        font.weight: Font.Bold
        color: root.ringColor
    }

    Text {
        anchors.top: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 2
        text: root.labelText
        font.pixelSize: Theme.fontSizeCaption
        color: Theme.textMuted
    }
}
