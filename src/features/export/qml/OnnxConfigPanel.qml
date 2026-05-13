// OnnxConfigPanel.qml - ONNX export options
import QtQuick
import QtQuick.Controls
import LabelTorch.Shell
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.bgPrimary
    radius: 6
    implicitHeight: layout.implicitHeight + 24

    property alias opsetVersion: opsetSpin.value
    property alias dynamicBatch: dynamicBatchSwitch.checked
    property alias simplify: simplifySwitch.checked

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Label {
            text: "ONNX Export Options"
            color: Theme.accentPrimary
            font.pixelSize: 14
            font.bold: true
        }

        // Opset version
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Label {
                text: "Opset Version:"
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.preferredWidth: 120
            }

            SpinBox {
                id: opsetSpin
                from: 7
                to: 17
                value: 13
                editable: true
                Layout.preferredWidth: 120

                contentItem: Label {
                    text: opsetSpin.textFromValue(opsetSpin.value, opsetSpin.locale)
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: "monospace"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                up.indicator: Rectangle {
                    x: opsetSpin.mirrored ? 0 : parent.width - width
                    height: parent.height / 2
                    width: 32
                    color: opsetSpin.up.pressed ? Theme.borderNormal : Theme.bgInput
                    border.color: Theme.borderNormal
                    radius: 2

                    Label {
                        anchors.centerIn: parent
                        text: "+"
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                down.indicator: Rectangle {
                    x: opsetSpin.mirrored ? 0 : parent.width - width
                    y: parent.height / 2
                    height: parent.height / 2
                    width: 32
                    color: opsetSpin.down.pressed ? Theme.borderNormal : Theme.bgInput
                    border.color: Theme.borderNormal
                    radius: 2

                    Label {
                        anchors.centerIn: parent
                        text: "-"
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                background: Rectangle {
                    color: Theme.bgInput
                    border.color: opsetSpin.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                    radius: 4
                }
            }

            Label {
                text: "(7 - 17)"
                color: Theme.textMuted
                font.pixelSize: 11
            }
        }

        // Dynamic batch
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Label {
                text: "Dynamic Batch:"
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.preferredWidth: 120
            }

            Switch {
                id: dynamicBatchSwitch
                checked: true

                indicator: Rectangle {
                    x: dynamicBatchSwitch.leftPadding
                    y: parent.height / 2 - height / 2
                    width: 40
                    height: 22
                    radius: 11
                    color: dynamicBatchSwitch.checked ? Theme.accentPrimary : Theme.borderNormal
                    border.color: dynamicBatchSwitch.checked ? Theme.accentPrimary : "#585b70"

                    Rectangle {
                        x: dynamicBatchSwitch.checked ? parent.width - width - 3 : 3
                        y: parent.height / 2 - height / 2
                        width: 16
                        height: 16
                        radius: 8
                        color: Theme.textPrimary

                        Behavior on x {
                            NumberAnimation { duration: 150 }
                        }
                    }
                }

                background: Rectangle {
                    color: "transparent"
                }
            }

            Label {
                text: dynamicBatchSwitch.checked ? "Enabled" : "Disabled"
                color: dynamicBatchSwitch.checked ? Theme.accentSuccess : Theme.textMuted
                font.pixelSize: 12
            }
        }

        // Simplify
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Label {
                text: "Simplify:"
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.preferredWidth: 120
            }

            Switch {
                id: simplifySwitch
                checked: true

                indicator: Rectangle {
                    x: simplifySwitch.leftPadding
                    y: parent.height / 2 - height / 2
                    width: 40
                    height: 22
                    radius: 11
                    color: simplifySwitch.checked ? Theme.accentPrimary : Theme.borderNormal
                    border.color: simplifySwitch.checked ? Theme.accentPrimary : "#585b70"

                    Rectangle {
                        x: simplifySwitch.checked ? parent.width - width - 3 : 3
                        y: parent.height / 2 - height / 2
                        width: 16
                        height: 16
                        radius: 8
                        color: Theme.textPrimary

                        Behavior on x {
                            NumberAnimation { duration: 150 }
                        }
                    }
                }

                background: Rectangle {
                    color: "transparent"
                }
            }

            Label {
                text: simplifySwitch.checked ? "Enabled" : "Disabled"
                color: simplifySwitch.checked ? Theme.accentSuccess : Theme.textMuted
                font.pixelSize: 12
            }
        }
    }

    function getConfigJson() {
        return JSON.stringify({
            "opset_version": opsetSpin.value,
            "dynamic_batch": dynamicBatchSwitch.checked,
            "simplify": simplifySwitch.checked
        });
    }
}
