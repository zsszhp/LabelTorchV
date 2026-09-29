// StatusTag.qml - 状态胶囊：实底浅色底 + 深色字，圆角胶囊形
// 语义色经 Theme 发放，浅底/深字在此配对，保证任意底色上对比度足够
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme

Rectangle {
    id: root
    implicitWidth: label.implicitWidth + Theme.spacingMedium * 2
    implicitHeight: 22
    // 胶囊：圆角为高度一半
    radius: height / 2
    border.width: 0

    property string text: "状态"
    property string tone: "neutral"

    // 浅色实底（降饱和，深字保证可读）
    readonly property color toneBg: {
        switch (tone) {
            case "success": return "#A7F3D0"
            case "warning": return "#FDE68A"
            case "danger": return "#FECACA"
            case "info": return "#A5F3FC"
            default: return "#CBD5E1"
        }
    }

    // 深色文字（与浅底配对）
    readonly property color toneText: {
        switch (tone) {
            case "success": return "#065F46"
            case "warning": return "#92400E"
            case "danger": return "#991B1B"
            case "info": return "#0E7490"
            default: return "#334155"
        }
    }

    color: toneBg

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        font.pixelSize: Theme.fontSizeCaption
        font.weight: Font.DemiBold
        font.family: Theme.fontFamily
        color: root.toneText
    }
}
