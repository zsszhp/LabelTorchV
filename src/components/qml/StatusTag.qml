import QtQuick
import QtQuick.Controls
import LabelTorch.Theme

Rectangle {
    id: root
    implicitWidth: label.implicitWidth + Theme.spacingNormal * 3
    implicitHeight: 24
    radius: Theme.radiusNormal
    border.width: 1

    property string text: "状态"
    property string tone: "neutral"

    readonly property color toneColor: {
        switch (tone) {
            case "success": return Theme.success
            case "warning": return Theme.warning
            case "danger": return Theme.danger
            case "info": return Theme.primaryGlow
            default: return Theme.textMuted
        }
    }

    color: Qt.alpha(toneColor, 0.12)
    border.color: Qt.alpha(toneColor, 0.35)

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        font.pixelSize: Theme.fontSizeCaption
        font.weight: Font.DemiBold
        font.family: Theme.fontFamily
        color: root.toneColor
    }
}
