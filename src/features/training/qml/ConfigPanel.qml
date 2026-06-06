// ConfigPanel.qml - Training parameter configuration panel
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme
import QtQuick.Layouts
import QtQuick.Effects

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
            text: "训练参数"
            color: Theme.accentPrimary
            font.pixelSize: 14
            font.bold: true
        }

        // 模型系列选择器
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "模型:"
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
                    radius: Theme.radiusSmall
                    border.color: modelFamilyCombo.activeFocus ? Theme.primaryGlow : (modelFamilyCombo.hovered ? Theme.primary : Theme.borderColor)
                    border.width: 1

                    layer.enabled: modelFamilyCombo.activeFocus || modelFamilyCombo.hovered
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: modelFamilyCombo.activeFocus ? Theme.primaryGlow : Theme.primary
                        shadowBlur: 0.15
                    }
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
                        color: Theme.bgInputDropdown
                        border.color: Theme.borderColor
                        radius: Theme.radiusSmall
                    }
                }

                delegate: ItemDelegate {
                    width: modelFamilyCombo.width
                    contentItem: Label {
                        text: modelData
                        color: highlighted ? Theme.primaryGlow : Theme.textPrimary
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                    }
                    highlighted: modelFamilyCombo.highlightedIndex === index
                    background: Rectangle {
                        color: highlighted ? Theme.bgHover : "transparent"
                    }
                }
            }
        }

        // 训练类型选择器
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "类型:"
                color: Theme.textPrimary
                font.pixelSize: 13
                Layout.preferredWidth: 80
            }

            ComboBox {
                id: trainingTypeCombo
                model: ["从头训练", "预训练", "增量训练"]
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
                    radius: Theme.radiusSmall
                    border.color: trainingTypeCombo.activeFocus ? Theme.primaryGlow : (trainingTypeCombo.hovered ? Theme.primary : Theme.borderColor)
                    border.width: 1

                    layer.enabled: trainingTypeCombo.activeFocus || trainingTypeCombo.hovered
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: trainingTypeCombo.activeFocus ? Theme.primaryGlow : Theme.primary
                        shadowBlur: 0.15
                    }
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
                        color: Theme.bgInputDropdown
                        border.color: Theme.borderColor
                        radius: Theme.radiusSmall
                    }
                }

                delegate: ItemDelegate {
                    width: trainingTypeCombo.width
                    contentItem: Label {
                        text: modelData
                        color: highlighted ? Theme.primaryGlow : Theme.textPrimary
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                    }
                    highlighted: trainingTypeCombo.highlightedIndex === index
                    background: Rectangle {
                        color: highlighted ? Theme.bgHover : "transparent"
                    }
                }
            }
        }

        // 父模型版本选择器（增量训练时使用）
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: trainingTypeCombo.currentIndex === 2

            Label {
                text: "父版本:"
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
                        "选择父版本"
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }

                background: Rectangle {
                    color: Theme.bgInput
                    radius: Theme.radiusSmall
                    border.color: parentVersionCombo.activeFocus ? Theme.primaryGlow : (parentVersionCombo.hovered ? Theme.primary : Theme.borderColor)
                    border.width: 1

                    layer.enabled: parentVersionCombo.activeFocus || parentVersionCombo.hovered
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: parentVersionCombo.activeFocus ? Theme.primaryGlow : Theme.primary
                        shadowBlur: 0.15
                    }
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
                        color: Theme.bgInputDropdown
                        border.color: Theme.borderColor
                        radius: Theme.radiusSmall
                    }
                }

                delegate: ItemDelegate {
                    width: parentVersionCombo.width
                    contentItem: Label {
                        text: model.versionId.substring(0, 8) + "... (" + model.bestWeight + ")"
                        color: highlighted ? Theme.primaryGlow : Theme.textPrimary
                        font.pixelSize: 12
                        font.family: "monospace"
                        verticalAlignment: Text.AlignVCenter
                    }
                    highlighted: parentVersionCombo.highlightedIndex === index
                    background: Rectangle {
                        color: highlighted ? Theme.bgHover : "transparent"
                    }
                }
            }
        }

        // 图片尺寸
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "图片尺寸:"
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
                valueFromText: function(text) { return parseInt(text) || 640 }

                contentItem: TextInput {
                    text: imgSizeSpin.displayText
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    selectByMouse: true
                    readOnly: !imgSizeSpin.editable
                    validator: imgSizeSpin.validator
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }

                up.indicator: Rectangle {
                    x: imgSizeSpin.mirrored ? 0 : parent.width - width
                    height: parent.height
                    implicitWidth: 28
                    color: imgSizeSpin.up.pressed ? Theme.primary : (imgSizeSpin.up.hovered ? Theme.bgHover : Theme.bgInput)
                    border.color: Theme.borderColor
                    radius: Theme.radiusSmall
                    Behavior on color { ColorAnimation { duration: 100 } }

                    Label {
                        anchors.centerIn: parent
                        text: "+"
                        color: imgSizeSpin.up.hovered ? Theme.primaryGlow : Theme.textPrimary
                        font.pixelSize: 14
                        font.bold: true
                    }
                }

                down.indicator: Rectangle {
                    x: imgSizeSpin.mirrored ? parent.width - width : 0
                    height: parent.height
                    implicitWidth: 28
                    color: imgSizeSpin.down.pressed ? Theme.primary : (imgSizeSpin.down.hovered ? Theme.bgHover : Theme.bgInput)
                    border.color: Theme.borderColor
                    radius: Theme.radiusSmall
                    Behavior on color { ColorAnimation { duration: 100 } }

                    Label {
                        anchors.centerIn: parent
                        text: "-"
                        color: imgSizeSpin.down.hovered ? Theme.primaryGlow : Theme.textPrimary
                        font.pixelSize: 14
                        font.bold: true
                    }
                }

                background: Rectangle {
                    color: Theme.bgInput
                    border.color: imgSizeSpin.activeFocus ? Theme.primaryGlow : (imgSizeSpin.hovered ? Theme.primary : Theme.borderColor)
                    radius: Theme.radiusSmall
                    border.width: 1

                    layer.enabled: imgSizeSpin.activeFocus || imgSizeSpin.hovered
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: imgSizeSpin.activeFocus ? Theme.primaryGlow : Theme.primary
                        shadowBlur: 0.15
                    }
                }
            }
        }

        // 批大小
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "批大小:"
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

                contentItem: TextInput {
                    text: batchSpin.displayText
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    selectByMouse: true
                    readOnly: !batchSpin.editable
                    validator: batchSpin.validator
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }

                up.indicator: Rectangle {
                    x: batchSpin.mirrored ? 0 : parent.width - width
                    height: parent.height
                    implicitWidth: 28
                    color: batchSpin.up.pressed ? Theme.primary : (batchSpin.up.hovered ? Theme.bgHover : Theme.bgInput)
                    border.color: Theme.borderColor
                    radius: Theme.radiusSmall
                    Behavior on color { ColorAnimation { duration: 100 } }
                    Label { anchors.centerIn: parent; text: "+"; color: batchSpin.up.hovered ? Theme.primaryGlow : Theme.textPrimary; font.pixelSize: 14; font.bold: true }
                }

                down.indicator: Rectangle {
                    x: batchSpin.mirrored ? parent.width - width : 0
                    height: parent.height
                    implicitWidth: 28
                    color: batchSpin.down.pressed ? Theme.primary : (batchSpin.down.hovered ? Theme.bgHover : Theme.bgInput)
                    border.color: Theme.borderColor
                    radius: Theme.radiusSmall
                    Behavior on color { ColorAnimation { duration: 100 } }
                    Label { anchors.centerIn: parent; text: "-"; color: batchSpin.down.hovered ? Theme.primaryGlow : Theme.textPrimary; font.pixelSize: 14; font.bold: true }
                }

                background: Rectangle {
                    color: Theme.bgInput
                    border.color: batchSpin.activeFocus ? Theme.primaryGlow : (batchSpin.hovered ? Theme.primary : Theme.borderColor)
                    radius: Theme.radiusSmall
                    border.width: 1

                    layer.enabled: batchSpin.activeFocus || batchSpin.hovered
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: batchSpin.activeFocus ? Theme.primaryGlow : Theme.primary
                        shadowBlur: 0.15
                    }
                }
            }
        }

        // 训练轮数
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "训练轮数:"
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

                contentItem: TextInput {
                    text: epochsSpin.displayText
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    selectByMouse: true
                    readOnly: !epochsSpin.editable
                    validator: epochsSpin.validator
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }

                up.indicator: Rectangle {
                    x: epochsSpin.mirrored ? 0 : parent.width - width
                    height: parent.height
                    implicitWidth: 28
                    color: epochsSpin.up.pressed ? Theme.primary : (epochsSpin.up.hovered ? Theme.bgHover : Theme.bgInput)
                    border.color: Theme.borderColor
                    radius: Theme.radiusSmall
                    Behavior on color { ColorAnimation { duration: 100 } }
                    Label { anchors.centerIn: parent; text: "+"; color: epochsSpin.up.hovered ? Theme.primaryGlow : Theme.textPrimary; font.pixelSize: 14; font.bold: true }
                }

                down.indicator: Rectangle {
                    x: epochsSpin.mirrored ? parent.width - width : 0
                    height: parent.height
                    implicitWidth: 28
                    color: epochsSpin.down.pressed ? Theme.primary : (epochsSpin.down.hovered ? Theme.bgHover : Theme.bgInput)
                    border.color: Theme.borderColor
                    radius: Theme.radiusSmall
                    Behavior on color { ColorAnimation { duration: 100 } }
                    Label { anchors.centerIn: parent; text: "-"; color: epochsSpin.down.hovered ? Theme.primaryGlow : Theme.textPrimary; font.pixelSize: 14; font.bold: true }
                }

                background: Rectangle {
                    color: Theme.bgInput
                    border.color: epochsSpin.activeFocus ? Theme.primaryGlow : (epochsSpin.hovered ? Theme.primary : Theme.borderColor)
                    radius: Theme.radiusSmall
                    border.width: 1

                    layer.enabled: epochsSpin.activeFocus || epochsSpin.hovered
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: epochsSpin.activeFocus ? Theme.primaryGlow : Theme.primary
                        shadowBlur: 0.15
                    }
                }
            }
        }

        // 早停耐心值
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "早停耐心:"
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

                contentItem: TextInput {
                    text: patienceSpin.displayText
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    selectByMouse: true
                    readOnly: !patienceSpin.editable
                    validator: patienceSpin.validator
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }

                up.indicator: Rectangle {
                    x: patienceSpin.mirrored ? 0 : parent.width - width
                    height: parent.height
                    implicitWidth: 28
                    color: patienceSpin.up.pressed ? Theme.primary : (patienceSpin.up.hovered ? Theme.bgHover : Theme.bgInput)
                    border.color: Theme.borderColor
                    radius: Theme.radiusSmall
                    Behavior on color { ColorAnimation { duration: 100 } }
                    Label { anchors.centerIn: parent; text: "+"; color: patienceSpin.up.hovered ? Theme.primaryGlow : Theme.textPrimary; font.pixelSize: 14; font.bold: true }
                }

                down.indicator: Rectangle {
                    x: patienceSpin.mirrored ? parent.width - width : 0
                    height: parent.height
                    implicitWidth: 28
                    color: patienceSpin.down.pressed ? Theme.primary : (patienceSpin.down.hovered ? Theme.bgHover : Theme.bgInput)
                    border.color: Theme.borderColor
                    radius: Theme.radiusSmall
                    Behavior on color { ColorAnimation { duration: 100 } }
                    Label { anchors.centerIn: parent; text: "-"; color: patienceSpin.down.hovered ? Theme.primaryGlow : Theme.textPrimary; font.pixelSize: 14; font.bold: true }
                }

                background: Rectangle {
                    color: Theme.bgInput
                    border.color: patienceSpin.activeFocus ? Theme.primaryGlow : (patienceSpin.hovered ? Theme.primary : Theme.borderColor)
                    radius: Theme.radiusSmall
                    border.width: 1

                    layer.enabled: patienceSpin.activeFocus || patienceSpin.hovered
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: patienceSpin.activeFocus ? Theme.primaryGlow : Theme.primary
                        shadowBlur: 0.15
                    }
                }
            }
        }

        // 工作线程
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "工作线程:"
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

                contentItem: TextInput {
                    text: workersSpin.displayText
                    color: Theme.textPrimary
                    font.pixelSize: 13
                    font.family: "monospace"
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    selectByMouse: true
                    readOnly: !workersSpin.editable
                    validator: workersSpin.validator
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }

                up.indicator: Rectangle {
                    x: workersSpin.mirrored ? 0 : parent.width - width
                    height: parent.height
                    implicitWidth: 28
                    color: workersSpin.up.pressed ? Theme.primary : (workersSpin.up.hovered ? Theme.bgHover : Theme.bgInput)
                    border.color: Theme.borderColor
                    radius: Theme.radiusSmall
                    Behavior on color { ColorAnimation { duration: 100 } }
                    Label { anchors.centerIn: parent; text: "+"; color: workersSpin.up.hovered ? Theme.primaryGlow : Theme.textPrimary; font.pixelSize: 14; font.bold: true }
                }

                down.indicator: Rectangle {
                    x: workersSpin.mirrored ? parent.width - width : 0
                    height: parent.height
                    implicitWidth: 28
                    color: workersSpin.down.pressed ? Theme.primary : (workersSpin.down.hovered ? Theme.bgHover : Theme.bgInput)
                    border.color: Theme.borderColor
                    radius: Theme.radiusSmall
                    Behavior on color { ColorAnimation { duration: 100 } }
                    Label { anchors.centerIn: parent; text: "-"; color: workersSpin.down.hovered ? Theme.primaryGlow : Theme.textPrimary; font.pixelSize: 14; font.bold: true }
                }

                background: Rectangle {
                    color: Theme.bgInput
                    border.color: workersSpin.activeFocus ? Theme.primaryGlow : (workersSpin.hovered ? Theme.primary : Theme.borderColor)
                    radius: Theme.radiusSmall
                    border.width: 1

                    layer.enabled: workersSpin.activeFocus || workersSpin.hovered
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: workersSpin.activeFocus ? Theme.primaryGlow : Theme.primary
                        shadowBlur: 0.15
                    }
                }
            }
        }

        // 混合精度开关
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "混合精度:"
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
                    implicitWidth: Theme.toggleWidth
                    implicitHeight: Theme.toggleHeight
                    radius: implicitHeight / 2
                    color: ampSwitch.checked ? Theme.primary : Theme.bgInput
                    border.color: ampSwitch.checked ? Theme.primaryGlow : Theme.borderColor
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: 150 } }

                    Rectangle {
                        x: ampSwitch.checked ? parent.width - width - 2 : 2
                        y: parent.height / 2 - height / 2
                        implicitWidth: 14
                        implicitHeight: 14
                        radius: 7
                        color: ampSwitch.checked ? Theme.primaryGlow : Theme.textMuted
                        
                        Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.InOutQuad } }
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }
                }
            }
        }

        // 继续训练开关
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "继续训练:"
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
                    implicitWidth: Theme.toggleWidth
                    implicitHeight: Theme.toggleHeight
                    radius: implicitHeight / 2
                    color: resumeSwitch.checked ? Theme.primary : Theme.bgInput
                    border.color: resumeSwitch.checked ? Theme.primaryGlow : Theme.borderColor
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: 150 } }

                    Rectangle {
                        x: resumeSwitch.checked ? parent.width - width - 2 : 2
                        y: parent.height / 2 - height / 2
                        implicitWidth: 14
                        implicitHeight: 14
                        radius: 7
                        color: resumeSwitch.checked ? Theme.primaryGlow : Theme.textMuted
                        
                        Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.InOutQuad } }
                        Behavior on color { ColorAnimation { duration: 150 } }
                    }
                }
            }
        }

        // 设备选择器
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "设备:"
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
                    radius: Theme.radiusSmall
                    border.color: deviceCombo.activeFocus ? Theme.primaryGlow : (deviceCombo.hovered ? Theme.primary : Theme.borderColor)
                    border.width: 1

                    layer.enabled: deviceCombo.activeFocus || deviceCombo.hovered
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: deviceCombo.activeFocus ? Theme.primaryGlow : Theme.primary
                        shadowBlur: 0.15
                    }
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
                        color: Theme.bgInputDropdown
                        border.color: Theme.borderColor
                        radius: Theme.radiusSmall
                    }
                }

                delegate: ItemDelegate {
                    width: deviceCombo.width
                    contentItem: Label {
                        text: modelData
                        color: highlighted ? Theme.primaryGlow : Theme.textPrimary
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                    }
                    highlighted: deviceCombo.highlightedIndex === index
                    background: Rectangle {
                        color: highlighted ? Theme.bgHover : "transparent"
                    }
                }
            }
        }
    }
}
