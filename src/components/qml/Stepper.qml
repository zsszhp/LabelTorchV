// Stepper.qml - 步进器控件（—/值/+ 模式，对标参考UI）
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme

Control {
    id: root
    implicitWidth: Theme.stepperButtonWidth * 2 + Theme.stepperValueWidth
    implicitHeight: Theme.stepperHeight

    property real value: 0
    property real minValue: -Infinity
    property real maxValue: Infinity
    property real stepSize: 1
    property int decimals: 0
    property string suffix: ""

    signal valueModified()

    background: Rectangle {
        color: Theme.bgInput
        radius: Theme.radiusSmall
        border.color: root.activeFocus ? Theme.primaryGlow : Theme.borderColor
        border.width: 1
    }

    contentItem: Row {
        spacing: 0

        Button {
            width: Theme.stepperButtonWidth
            height: Theme.stepperHeight
            text: "—"
            font.pixelSize: Theme.fontSizeSmall
            font.family: Theme.fontFamilyMono
            palette.buttonText: Theme.textMuted
            palette.button: Theme.bgInput
            flat: true
            onClicked: {
                var newVal = root.value - root.stepSize
                if (newVal >= root.minValue) {
                    root.value = newVal
                    root.valueModified()
                }
            }
            background: Rectangle {
                color: parent.hovered ? Theme.bgHover : "transparent"
                radius: Theme.radiusSmall
            }
        }

        Rectangle {
            width: 1
            height: Theme.stepperHeight - 8
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.borderColor
        }

        Text {
            width: Theme.stepperValueWidth
            height: Theme.stepperHeight
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: root.value.toFixed(root.decimals) + root.suffix
            font.pixelSize: Theme.fontSizeSmall
            font.family: Theme.fontFamilyMono
            color: Theme.textMain
        }

        Rectangle {
            width: 1
            height: Theme.stepperHeight - 8
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.borderColor
        }

        Button {
            width: Theme.stepperButtonWidth
            height: Theme.stepperHeight
            text: "+"
            font.pixelSize: Theme.fontSizeSmall
            font.family: Theme.fontFamilyMono
            palette.buttonText: Theme.textMuted
            palette.button: Theme.bgInput
            flat: true
            onClicked: {
                var newVal = root.value + root.stepSize
                if (newVal <= root.maxValue) {
                    root.value = newVal
                    root.valueModified()
                }
            }
            background: Rectangle {
                color: parent.hovered ? Theme.bgHover : "transparent"
                radius: Theme.radiusSmall
            }
        }
    }
}
