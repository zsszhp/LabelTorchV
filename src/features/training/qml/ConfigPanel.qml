// ConfigPanel.qml - Training parameter configuration panel
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#181825"
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

    function getConfigJson() {
        var config = {
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
            color: "#89b4fa"
            font.pixelSize: 14
            font.bold: true
        }

        // Model family selector
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Model:"
                color: "#cdd6f4"
                font.pixelSize: 13
                Layout.preferredWidth: 80
            }

            ComboBox {
                id: modelFamilyCombo
                model: ["yolov5", "yolov8", "yolov8_obb", "yolov10", "yolov11"]
                currentIndex: 1
                Layout.fillWidth: true

                contentItem: Label {
                    text: modelFamilyCombo.displayText
                    color: "#cdd6f4"
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }

                background: Rectangle {
                    color: "#313244"
                    radius: 4
                    border.color: modelFamilyCombo.activeFocus ? "#89b4fa" : "#45475a"
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
                        color: "#1e1e2e"
                        border.color: "#45475a"
                        radius: 4
                    }
                }

                delegate: ItemDelegate {
                    width: modelFamilyCombo.width
                    contentItem: Label {
                        text: modelData
                        color: highlighted ? "#89b4fa" : "#cdd6f4"
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                    }
                    highlighted: modelFamilyCombo.highlightedIndex === index
                    background: Rectangle {
                        color: highlighted ? "#313244" : "#1e1e2e"
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
                color: "#cdd6f4"
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
                    color: "#cdd6f4"
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }

                background: Rectangle {
                    color: "#313244"
                    radius: 4
                    border.color: trainingTypeCombo.activeFocus ? "#89b4fa" : "#45475a"
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
                        color: "#1e1e2e"
                        border.color: "#45475a"
                        radius: 4
                    }
                }

                delegate: ItemDelegate {
                    width: trainingTypeCombo.width
                    contentItem: Label {
                        text: modelData
                        color: highlighted ? "#89b4fa" : "#cdd6f4"
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                    }
                    highlighted: trainingTypeCombo.highlightedIndex === index
                    background: Rectangle {
                        color: highlighted ? "#313244" : "#1e1e2e"
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
                color: "#cdd6f4"
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
                    color: "#cdd6f4"
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }

                background: Rectangle {
                    color: "#313244"
                    radius: 4
                    border.color: parentVersionCombo.activeFocus ? "#89b4fa" : "#45475a"
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
                        color: "#1e1e2e"
                        border.color: "#45475a"
                        radius: 4
                    }
                }

                delegate: ItemDelegate {
                    width: parentVersionCombo.width
                    contentItem: Label {
                        text: model.versionId.substring(0, 8) + "... (" + model.bestWeight + ")"
                        color: highlighted ? "#89b4fa" : "#cdd6f4"
                        font.pixelSize: 12
                        font.family: "monospace"
                        verticalAlignment: Text.AlignVCenter
                    }
                    highlighted: parentVersionCombo.highlightedIndex === index
                    background: Rectangle {
                        color: highlighted ? "#313244" : "#1e1e2e"
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
                color: "#cdd6f4"
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
                    color: "#cdd6f4"
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }

                up.indicator: Rectangle {
                    x: imgSizeSpin.mirrored ? 0 : parent.width - width
                    height: parent.height
                    implicitWidth: 32
                    color: imgSizeSpin.up.pressed ? "#45475a" : "#313244"
                    border.color: "#45475a"
                    radius: 2

                    Label {
                        anchors.centerIn: parent
                        text: "+"
                        color: "#cdd6f4"
                        font.pixelSize: 14
                    }
                }

                down.indicator: Rectangle {
                    x: imgSizeSpin.mirrored ? parent.width - width : 0
                    height: parent.height
                    implicitWidth: 32
                    color: imgSizeSpin.down.pressed ? "#45475a" : "#313244"
                    border.color: "#45475a"
                    radius: 2

                    Label {
                        anchors.centerIn: parent
                        text: "-"
                        color: "#cdd6f4"
                        font.pixelSize: 14
                    }
                }

                background: Rectangle {
                    color: "#313244"
                    border.color: imgSizeSpin.activeFocus ? "#89b4fa" : "#45475a"
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
                color: "#cdd6f4"
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
                    color: "#cdd6f4"
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }

                up.indicator: Rectangle {
                    x: batchSpin.mirrored ? 0 : parent.width - width
                    height: parent.height
                    implicitWidth: 32
                    color: batchSpin.up.pressed ? "#45475a" : "#313244"
                    border.color: "#45475a"
                    radius: 2
                    Label { anchors.centerIn: parent; text: "+"; color: "#cdd6f4"; font.pixelSize: 14 }
                }

                down.indicator: Rectangle {
                    x: batchSpin.mirrored ? parent.width - width : 0
                    height: parent.height
                    implicitWidth: 32
                    color: batchSpin.down.pressed ? "#45475a" : "#313244"
                    border.color: "#45475a"
                    radius: 2
                    Label { anchors.centerIn: parent; text: "-"; color: "#cdd6f4"; font.pixelSize: 14 }
                }

                background: Rectangle {
                    color: "#313244"
                    border.color: batchSpin.activeFocus ? "#89b4fa" : "#45475a"
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
                color: "#cdd6f4"
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
                    color: "#cdd6f4"
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }

                up.indicator: Rectangle {
                    x: epochsSpin.mirrored ? 0 : parent.width - width
                    height: parent.height
                    implicitWidth: 32
                    color: epochsSpin.up.pressed ? "#45475a" : "#313244"
                    border.color: "#45475a"
                    radius: 2
                    Label { anchors.centerIn: parent; text: "+"; color: "#cdd6f4"; font.pixelSize: 14 }
                }

                down.indicator: Rectangle {
                    x: epochsSpin.mirrored ? parent.width - width : 0
                    height: parent.height
                    implicitWidth: 32
                    color: epochsSpin.down.pressed ? "#45475a" : "#313244"
                    border.color: "#45475a"
                    radius: 2
                    Label { anchors.centerIn: parent; text: "-"; color: "#cdd6f4"; font.pixelSize: 14 }
                }

                background: Rectangle {
                    color: "#313244"
                    border.color: epochsSpin.activeFocus ? "#89b4fa" : "#45475a"
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
                color: "#cdd6f4"
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
                    color: "#cdd6f4"
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }

                up.indicator: Rectangle {
                    x: patienceSpin.mirrored ? 0 : parent.width - width
                    height: parent.height
                    implicitWidth: 32
                    color: patienceSpin.up.pressed ? "#45475a" : "#313244"
                    border.color: "#45475a"
                    radius: 2
                    Label { anchors.centerIn: parent; text: "+"; color: "#cdd6f4"; font.pixelSize: 14 }
                }

                down.indicator: Rectangle {
                    x: patienceSpin.mirrored ? parent.width - width : 0
                    height: parent.height
                    implicitWidth: 32
                    color: patienceSpin.down.pressed ? "#45475a" : "#313244"
                    border.color: "#45475a"
                    radius: 2
                    Label { anchors.centerIn: parent; text: "-"; color: "#cdd6f4"; font.pixelSize: 14 }
                }

                background: Rectangle {
                    color: "#313244"
                    border.color: patienceSpin.activeFocus ? "#89b4fa" : "#45475a"
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
                color: "#cdd6f4"
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
                    color: "#cdd6f4"
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }

                up.indicator: Rectangle {
                    x: workersSpin.mirrored ? 0 : parent.width - width
                    height: parent.height
                    implicitWidth: 32
                    color: workersSpin.up.pressed ? "#45475a" : "#313244"
                    border.color: "#45475a"
                    radius: 2
                    Label { anchors.centerIn: parent; text: "+"; color: "#cdd6f4"; font.pixelSize: 14 }
                }

                down.indicator: Rectangle {
                    x: workersSpin.mirrored ? parent.width - width : 0
                    height: parent.height
                    implicitWidth: 32
                    color: workersSpin.down.pressed ? "#45475a" : "#313244"
                    border.color: "#45475a"
                    radius: 2
                    Label { anchors.centerIn: parent; text: "-"; color: "#cdd6f4"; font.pixelSize: 14 }
                }

                background: Rectangle {
                    color: "#313244"
                    border.color: workersSpin.activeFocus ? "#89b4fa" : "#45475a"
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
                color: "#cdd6f4"
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
                    color: ampSwitch.checked ? "#89b4fa" : "#45475a"

                    Rectangle {
                        x: ampSwitch.checked ? parent.width - width - 2 : 2
                        y: parent.height / 2 - height / 2
                        implicitWidth: 16
                        implicitHeight: 16
                        radius: 8
                        color: "#cdd6f4"
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
                color: "#cdd6f4"
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
                    color: resumeSwitch.checked ? "#89b4fa" : "#45475a"

                    Rectangle {
                        x: resumeSwitch.checked ? parent.width - width - 2 : 2
                        y: parent.height / 2 - height / 2
                        implicitWidth: 16
                        implicitHeight: 16
                        radius: 8
                        color: "#cdd6f4"
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
                color: "#cdd6f4"
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
                    color: "#cdd6f4"
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }

                background: Rectangle {
                    color: "#313244"
                    radius: 4
                    border.color: deviceCombo.activeFocus ? "#89b4fa" : "#45475a"
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
                        color: "#1e1e2e"
                        border.color: "#45475a"
                        radius: 4
                    }
                }

                delegate: ItemDelegate {
                    width: deviceCombo.width
                    contentItem: Label {
                        text: modelData
                        color: highlighted ? "#89b4fa" : "#cdd6f4"
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                    }
                    highlighted: deviceCombo.highlightedIndex === index
                    background: Rectangle {
                        color: highlighted ? "#313244" : "#1e1e2e"
                    }
                }
            }
        }
    }
}
