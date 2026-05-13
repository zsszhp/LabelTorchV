// ConfigPanel.qml - Training parameter configuration panel
import QtQuick
import QtQuick.Controls
import LabelTorch.Shell
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.bgCard
    radius: 8
    implicitHeight: layout.implicitHeight + 24
    implicitWidth: 360

    property alias imgSize: imgSizeSpin.value
    property alias batch: batchSpin.value
    property alias epochs: epochsSpin.value
    property alias patience: patienceSpin.value
    property alias workers: workersSpin.value
    property alias amp: ampSwitch.checked
    property alias resume: resumeSwitch.checked
    property alias device: deviceCombo.currentText
    property alias modelFamily: modelFamilyCombo.currentText
    property alias trainingType: trainingTypeCombo.currentIndex
    property alias parentVersionId: parentVersionCombo.currentValue
    property string adapter: "ultralytics"

    function getConfigJson() {
        var config = {
            "adapter": adapter,
            "img_size": imgSizeSpin.value,
            "batch": batchSpin.value,
            "epochs": epochsSpin.value,
            "patience": patienceSpin.value,
            "workers": workersSpin.value,
            "amp": ampSwitch.checked,
            "resume": resumeSwitch.checked,
            "device": deviceCombo.currentText,
            "model_family": modelFamilyCombo.currentText,
            "training_type": ["from_scratch", "pretrained", "incremental"][trainingTypeCombo.currentIndex]
        }
        if (trainingTypeCombo.currentIndex === 2 && parentVersionCombo.currentValue) {
            config["parent_model_version_id"] = parentVersionCombo.currentValue
        }
        return JSON.stringify(config)
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Label {
            text: "Training Parameters"
            color: Theme.accentPrimary
            font.pixelSize: 14
            font.bold: true
        }

        // Model family selector
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Model:"
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.preferredWidth: 80
            }

            ComboBox {
                id: modelFamilyCombo
                model: ["yolov5", "yolov8", "yolov8_obb", "yolov8_cls", "yolov10", "yolov11", "anomaly"]
                currentIndex: 1
                Layout.fillWidth: true

                contentItem: Label {
                    text: modelFamilyCombo.displayText
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }

                background: Rectangle {
                    color: Theme.bgInput
                    radius: 4
                    border.color: modelFamilyCombo.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                    border.width: 1
                }

                popup: Popup {
                    y: modelFamilyCombo.height
                    width: modelFamilyCombo.width
                    implicitHeight: contentItem.implicitHeight
                    padding: 1

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: modelFamilyCombo.popup.visible ? modelFamilyCombo.delegateModel : null
                        currentIndex: modelFamilyCombo.highlightedIndex
                    }

                    background: Rectangle {
                        color: Theme.bgPrimary
                        border.color: Theme.borderNormal
                        radius: 4
                    }
                }

                delegate: ItemDelegate {
                    width: modelFamilyCombo.width
                    contentItem: Label {
                        text: modelData
                        color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                    }
                    highlighted: modelFamilyCombo.highlightedIndex === index
                    background: Rectangle {
                        color: highlighted ? Theme.bgInput : Theme.bgPrimary
                    }
                }
            }
        }

        // Training type selector
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Type:"
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.preferredWidth: 80
            }

            ComboBox {
                id: trainingTypeCombo
                model: ["From Scratch", "Pretrained", "Incremental"]
                currentIndex: 0
                Layout.fillWidth: true

                contentItem: Label {
                    text: trainingTypeCombo.displayText
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }

                background: Rectangle {
                    color: Theme.bgInput
                    radius: 4
                    border.color: trainingTypeCombo.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                    border.width: 1
                }

                popup: Popup {
                    y: trainingTypeCombo.height
                    width: trainingTypeCombo.width
                    implicitHeight: contentItem.implicitHeight
                    padding: 1

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: trainingTypeCombo.popup.visible ? trainingTypeCombo.delegateModel : null
                        currentIndex: trainingTypeCombo.highlightedIndex
                    }

                    background: Rectangle {
                        color: Theme.bgPrimary
                        border.color: Theme.borderNormal
                        radius: 4
                    }
                }

                delegate: ItemDelegate {
                    width: trainingTypeCombo.width
                    contentItem: Label {
                        text: modelData
                        color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                    }
                    highlighted: trainingTypeCombo.highlightedIndex === index
                    background: Rectangle {
                        color: highlighted ? Theme.bgInput : Theme.bgPrimary
                    }
                }
            }
        }

        // Parent model version selector (for incremental training)
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: trainingTypeCombo.currentIndex === 2

            Label {
                text: "Parent:"
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.preferredWidth: 80
            }

            ComboBox {
                id: parentVersionCombo
                model: modelVersionModel
                textRole: "versionId"
                valueRole: "versionId"
                Layout.fillWidth: true

                contentItem: Label {
                    text: parentVersionCombo.currentIndex >= 0 ?
                        parentVersionCombo.currentValue.substring(0, 8) + "..." :
                        "Select parent version"
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }

                background: Rectangle {
                    color: Theme.bgInput
                    radius: 4
                    border.color: parentVersionCombo.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                    border.width: 1
                }

                popup: Popup {
                    y: parentVersionCombo.height
                    width: parentVersionCombo.width
                    implicitHeight: Math.min(contentItem.implicitHeight, 300)
                    padding: 1

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: parentVersionCombo.popup.visible ? parentVersionCombo.delegateModel : null
                        currentIndex: parentVersionCombo.highlightedIndex
                    }

                    background: Rectangle {
                        color: Theme.bgPrimary
                        border.color: Theme.borderNormal
                        radius: 4
                    }
                }

                delegate: ItemDelegate {
                    width: parentVersionCombo.width
                    contentItem: Label {
                        text: model.versionId.substring(0, 8) + "... (" + model.bestWeight + ")"
                        color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                        font.pixelSize: 12
                        font.family: "monospace"
                        verticalAlignment: Text.AlignVCenter
                    }
                    highlighted: parentVersionCombo.highlightedIndex === index
                    background: Rectangle {
                        color: highlighted ? Theme.bgInput : Theme.bgPrimary
                    }
                }
            }
        }

        // Image size
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Image Size:"
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.preferredWidth: 80
            }

            SpinBox {
                id: imgSizeSpin
                from: 128
                to: 1280
                value: 640
                stepSize: 64
                editable: true
                Layout.fillWidth: true

                textFromValue: function(value) { return value }
                valueFromText: function(text) { return parseInt(text) }

                contentItem: Label {
                    text: imgSizeSpin.textFromValue(imgSizeSpin.value, imgSizeSpin.locale)
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }

                up.indicator: Rectangle {
                    x: imgSizeSpin.mirrored ? 0 : parent.width - width
                    height: parent.height
                    implicitWidth: 32
                    color: imgSizeSpin.up.pressed ? Theme.borderNormal : Theme.bgInput
                    border.color: Theme.borderNormal
                    radius: 2

                    Label {
                        anchors.centerIn: parent
                        text: "+"
                        color: Theme.textPrimary
                        font.pixelSize: 14
                    }
                }

                down.indicator: Rectangle {
                    x: imgSizeSpin.mirrored ? parent.width - width : 0
                    height: parent.height
                    implicitWidth: 32
                    color: imgSizeSpin.down.pressed ? Theme.borderNormal : Theme.bgInput
                    border.color: Theme.borderNormal
                    radius: 2

                    Label {
                        anchors.centerIn: parent
                        text: "-"
                        color: Theme.textPrimary
                        font.pixelSize: 14
                    }
                }

                background: Rectangle {
                    color: Theme.bgInput
                    border.color: imgSizeSpin.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                    radius: 4
                }
            }
        }

        // Batch size
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Batch:"
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.preferredWidth: 80
            }

            SpinBox {
                id: batchSpin
                from: 1
                to: 128
                value: 16
                stepSize: 1
                editable: true
                Layout.fillWidth: true

                contentItem: Label {
                    text: batchSpin.textFromValue(batchSpin.value, batchSpin.locale)
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }

                up.indicator: Rectangle {
                    x: batchSpin.mirrored ? 0 : parent.width - width
                    height: parent.height
                    implicitWidth: 32
                    color: batchSpin.up.pressed ? Theme.borderNormal : Theme.bgInput
                    border.color: Theme.borderNormal
                    radius: 2
                    Label { anchors.centerIn: parent; text: "+"; color: Theme.textPrimary; font.pixelSize: 14 }
                }

                down.indicator: Rectangle {
                    x: batchSpin.mirrored ? parent.width - width : 0
                    height: parent.height
                    implicitWidth: 32
                    color: batchSpin.down.pressed ? Theme.borderNormal : Theme.bgInput
                    border.color: Theme.borderNormal
                    radius: 2
                    Label { anchors.centerIn: parent; text: "-"; color: Theme.textPrimary; font.pixelSize: 14 }
                }

                background: Rectangle {
                    color: Theme.bgInput
                    border.color: batchSpin.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                    radius: 4
                }
            }
        }

        // Epochs
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Epochs:"
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.preferredWidth: 80
            }

            SpinBox {
                id: epochsSpin
                from: 1
                to: 1000
                value: 100
                stepSize: 10
                editable: true
                Layout.fillWidth: true

                contentItem: Label {
                    text: epochsSpin.textFromValue(epochsSpin.value, epochsSpin.locale)
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }

                up.indicator: Rectangle {
                    x: epochsSpin.mirrored ? 0 : parent.width - width
                    height: parent.height
                    implicitWidth: 32
                    color: epochsSpin.up.pressed ? Theme.borderNormal : Theme.bgInput
                    border.color: Theme.borderNormal
                    radius: 2
                    Label { anchors.centerIn: parent; text: "+"; color: Theme.textPrimary; font.pixelSize: 14 }
                }

                down.indicator: Rectangle {
                    x: epochsSpin.mirrored ? parent.width - width : 0
                    height: parent.height
                    implicitWidth: 32
                    color: epochsSpin.down.pressed ? Theme.borderNormal : Theme.bgInput
                    border.color: Theme.borderNormal
                    radius: 2
                    Label { anchors.centerIn: parent; text: "-"; color: Theme.textPrimary; font.pixelSize: 14 }
                }

                background: Rectangle {
                    color: Theme.bgInput
                    border.color: epochsSpin.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                    radius: 4
                }
            }
        }

        // Patience (early stopping)
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Patience:"
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.preferredWidth: 80
            }

            SpinBox {
                id: patienceSpin
                from: 0
                to: 200
                value: 50
                stepSize: 5
                editable: true
                Layout.fillWidth: true

                contentItem: Label {
                    text: patienceSpin.textFromValue(patienceSpin.value, patienceSpin.locale)
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }

                up.indicator: Rectangle {
                    x: patienceSpin.mirrored ? 0 : parent.width - width
                    height: parent.height
                    implicitWidth: 32
                    color: patienceSpin.up.pressed ? Theme.borderNormal : Theme.bgInput
                    border.color: Theme.borderNormal
                    radius: 2
                    Label { anchors.centerIn: parent; text: "+"; color: Theme.textPrimary; font.pixelSize: 14 }
                }

                down.indicator: Rectangle {
                    x: patienceSpin.mirrored ? parent.width - width : 0
                    height: parent.height
                    implicitWidth: 32
                    color: patienceSpin.down.pressed ? Theme.borderNormal : Theme.bgInput
                    border.color: Theme.borderNormal
                    radius: 2
                    Label { anchors.centerIn: parent; text: "-"; color: Theme.textPrimary; font.pixelSize: 14 }
                }

                background: Rectangle {
                    color: Theme.bgInput
                    border.color: patienceSpin.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                    radius: 4
                }
            }
        }

        // Workers
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Workers:"
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.preferredWidth: 80
            }

            SpinBox {
                id: workersSpin
                from: 0
                to: 32
                value: 8
                stepSize: 1
                editable: true
                Layout.fillWidth: true

                contentItem: Label {
                    text: workersSpin.textFromValue(workersSpin.value, workersSpin.locale)
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }

                up.indicator: Rectangle {
                    x: workersSpin.mirrored ? 0 : parent.width - width
                    height: parent.height
                    implicitWidth: 32
                    color: workersSpin.up.pressed ? Theme.borderNormal : Theme.bgInput
                    border.color: Theme.borderNormal
                    radius: 2
                    Label { anchors.centerIn: parent; text: "+"; color: Theme.textPrimary; font.pixelSize: 14 }
                }

                down.indicator: Rectangle {
                    x: workersSpin.mirrored ? parent.width - width : 0
                    height: parent.height
                    implicitWidth: 32
                    color: workersSpin.down.pressed ? Theme.borderNormal : Theme.bgInput
                    border.color: Theme.borderNormal
                    radius: 2
                    Label { anchors.centerIn: parent; text: "-"; color: Theme.textPrimary; font.pixelSize: 14 }
                }

                background: Rectangle {
                    color: Theme.bgInput
                    border.color: workersSpin.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                    radius: 4
                }
            }
        }

        // AMP toggle
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "AMP:"
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.preferredWidth: 80
            }

            Switch {
                id: ampSwitch
                checked: true
                Layout.fillWidth: true

                indicator: Rectangle {
                    x: ampSwitch.leftPadding
                    y: parent.height / 2 - height / 2
                    implicitWidth: 40
                    implicitHeight: 20
                    radius: 10
                    color: ampSwitch.checked ? Theme.accentPrimary : Theme.borderNormal

                    Rectangle {
                        x: ampSwitch.checked ? parent.width - width - 2 : 2
                        y: parent.height / 2 - height / 2
                        implicitWidth: 16
                        implicitHeight: 16
                        radius: 8
                        color: Theme.textPrimary
                    }
                }
            }
        }

        // Resume toggle
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Resume:"
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.preferredWidth: 80
            }

            Switch {
                id: resumeSwitch
                checked: false
                Layout.fillWidth: true

                indicator: Rectangle {
                    x: resumeSwitch.leftPadding
                    y: parent.height / 2 - height / 2
                    implicitWidth: 40
                    implicitHeight: 20
                    radius: 10
                    color: resumeSwitch.checked ? Theme.accentPrimary : Theme.borderNormal

                    Rectangle {
                        x: resumeSwitch.checked ? parent.width - width - 2 : 2
                        y: parent.height / 2 - height / 2
                        implicitWidth: 16
                        implicitHeight: 16
                        radius: 8
                        color: Theme.textPrimary
                    }
                }
            }
        }

        // Device selector
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Device:"
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.preferredWidth: 80
            }

            ComboBox {
                id: deviceCombo
                model: ["auto", "cpu", "0", "1", "2", "3"]
                currentIndex: 0
                Layout.fillWidth: true

                contentItem: Label {
                    text: deviceCombo.displayText
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }

                background: Rectangle {
                    color: Theme.bgInput
                    radius: 4
                    border.color: deviceCombo.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                    border.width: 1
                }

                popup: Popup {
                    y: deviceCombo.height
                    width: deviceCombo.width
                    implicitHeight: contentItem.implicitHeight
                    padding: 1

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: deviceCombo.popup.visible ? deviceCombo.delegateModel : null
                        currentIndex: deviceCombo.highlightedIndex
                    }

                    background: Rectangle {
                        color: Theme.bgPrimary
                        border.color: Theme.borderNormal
                        radius: 4
                    }
                }

                delegate: ItemDelegate {
                    width: deviceCombo.width
                    contentItem: Label {
                        text: modelData
                        color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                    }
                    highlighted: deviceCombo.highlightedIndex === index
                    background: Rectangle {
                        color: highlighted ? Theme.bgInput : Theme.bgPrimary
                    }
                }
            }
        }
    }
}
