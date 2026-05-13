// ExportPage.qml - Export Center
import QtQuick
import QtQuick.Controls
import LabelTorch.Shell
import QtQuick.Layouts

Item {
    id: root

    property string currentProjectId: ""
    property string selectedVersionId: ""
    property var exportHistory: []

    onCurrentProjectIdChanged: {
        modelVersionModel.setProjectId(currentProjectId)
        selectedVersionId = ""
        exportHistory = []
        refreshExports()
    }

    function refreshExports() {
        if (selectedVersionId !== "") {
            exportHistory = exportService.listExports(selectedVersionId)
        } else {
            exportHistory = []
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // Left panel: Export configuration
        Rectangle {
            Layout.preferredWidth: 400
            Layout.fillHeight: true
            color: Theme.bgCard
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // Section title
                Label {
                    text: "Export Model"
                    color: Theme.accentPrimary
                    font.pixelSize: 16
                    font.bold: true
                }

                // Model version selector
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Model Version:"
                        color: Theme.textPrimary
                        font.pixelSize: 13
                        Layout.preferredWidth: 100
                    }

                    ComboBox {
                        id: versionCombo
                        Layout.fillWidth: true
                        model: modelVersionModel
                        textRole: "versionId"
                        valueRole: "versionId"
                        displayText: currentIndex >= 0 ?
                            modelVersionModel.data(modelVersionModel.index(currentIndex, 0), 257) ?
                            modelVersionModel.data(modelVersionModel.index(currentIndex, 0), 257).substring(0, 8) + "..." :
                            "Select version" : "Select version"

                        contentItem: Label {
                            text: versionCombo.displayText
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: 4
                            border.color: versionCombo.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                            border.width: 1
                        }

                        popup: Popup {
                            y: versionCombo.height
                            width: versionCombo.width
                            implicitHeight: Math.min(contentItem.implicitHeight, 300)
                            padding: 1

                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: versionCombo.popup.visible ? versionCombo.delegateModel : null
                                currentIndex: versionCombo.highlightedIndex
                            }

                            background: Rectangle {
                                color: Theme.bgPrimary
                                border.color: Theme.borderNormal
                                radius: 4
                            }
                        }

                        delegate: ItemDelegate {
                            width: versionCombo.width
                            contentItem: Label {
                                text: model.versionId.substring(0, 8) + "..." + (model.bestWeightPath ? " (" + model.bestWeightPath + ")" : "")
                                color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                font.pixelSize: 12
                                font.family: "monospace"
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            highlighted: versionCombo.highlightedIndex === index
                            background: Rectangle {
                                color: highlighted ? Theme.bgInput : Theme.bgPrimary
                            }
                        }

                        onActivated: {
                            selectedVersionId = currentValue
                            refreshExports()
                        }
                    }
                }

                // Format selector
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Format:"
                        color: Theme.textPrimary
                        font.pixelSize: 13
                        Layout.preferredWidth: 100
                    }

                    ComboBox {
                        id: formatCombo
                        Layout.fillWidth: true
                        model: ["pt", "onnx", "tflite", "engine"]
                        currentIndex: 1

                        contentItem: Label {
                            text: formatCombo.displayText
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: 4
                            border.color: formatCombo.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                            border.width: 1
                        }

                        popup: Popup {
                            y: formatCombo.height
                            width: formatCombo.width
                            implicitHeight: Math.min(contentItem.implicitHeight, 200)
                            padding: 1

                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: formatCombo.popup.visible ? formatCombo.delegateModel : null
                                currentIndex: formatCombo.highlightedIndex
                            }

                            background: Rectangle {
                                color: Theme.bgPrimary
                                border.color: Theme.borderNormal
                                radius: 4
                            }
                        }

                        delegate: ItemDelegate {
                            width: formatCombo.width
                            contentItem: Label {
                                text: modelData.toUpperCase()
                                color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                font.pixelSize: 13
                                font.bold: true
                                verticalAlignment: Text.AlignVCenter
                            }
                            highlighted: formatCombo.highlightedIndex === index
                            background: Rectangle {
                                color: highlighted ? Theme.bgInput : Theme.bgPrimary
                            }
                        }
                    }
                }

                // ONNX config panel (visible only when format=onnx)
                OnnxConfigPanel {
                    id: onnxConfigPanel
                    Layout.fillWidth: true
                    visible: formatCombo.currentText === "onnx"
                }

                // Format info for non-ONNX formats
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    visible: formatCombo.currentText !== "onnx"
                    color: "#11111b"
                    radius: 6

                    Label {
                        anchors.centerIn: parent
                        text: {
                            switch (formatCombo.currentText) {
                            case "pt": return "PyTorch .pt format - no additional options"
                            case "tflite": return "TensorFlow Lite format - no additional options"
                            case "engine": return "TensorRT Engine format - no additional options"
                            default: return ""
                            }
                        }
                        color: Theme.textMuted
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Item { Layout.fillHeight: true }

                // Export button
                Button {
                    id: exportBtn
                    text: "Start Export"
                    highlighted: true
                    enabled: versionCombo.currentIndex >= 0 && selectedVersionId !== ""
                    Layout.fillWidth: true

                    background: Rectangle {
                        color: parent.enabled ? (parent.pressed ? "#74c7a0" : Theme.accentSuccess) : Theme.borderNormal
                        radius: 6
                        implicitHeight: 40
                    }

                    contentItem: Label {
                        text: parent.text
                        color: parent.enabled ? Theme.bgPrimary : Theme.textMuted
                        font.pixelSize: 14
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        if (selectedVersionId === "") return
                        var format = formatCombo.currentText
                        var optionsJson = "{}"
                        if (format === "onnx") {
                            optionsJson = onnxConfigPanel.getConfigJson()
                        }
                        var artifactId = exportService.exportModel(selectedVersionId, format, optionsJson)
                        if (artifactId !== "") {
                            statusLabel.text = "Export started: " + artifactId.substring(0, 8) + "..."
                            statusLabel.color = Theme.accentSuccess
                            refreshExports()
                        } else {
                            statusLabel.text = "Failed to start export"
                            statusLabel.color = Theme.accentError
                        }
                    }
                }

                // Status label
                Label {
                    id: statusLabel
                    Layout.fillWidth: true
                    text: ""
                    color: Theme.accentSuccess
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }

        // Right panel: Export history
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgCard
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // Header
                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: "Export History"
                        color: Theme.accentPrimary
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: exportHistory.length + " artifacts"
                        color: Theme.textMuted
                        font.pixelSize: 12
                    }

                    Button {
                        text: "Refresh"
                        flat: true
                        palette.buttonText: Theme.accentPrimary
                        font.pixelSize: 12
                        onClicked: refreshExports()
                    }
                }

                // Version info label
                Label {
                    Layout.fillWidth: true
                    visible: selectedVersionId !== ""
                    text: "Showing exports for version: " + selectedVersionId.substring(0, 8) + "..."
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    font.family: "monospace"
                }

                // Export list
                ListView {
                    id: exportList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: exportHistory
                    spacing: 4

                    Label {
                        anchors.centerIn: parent
                        visible: exportList.count === 0
                        text: selectedVersionId === "" ?
                            "Select a model version to view exports" :
                            "No exports yet for this version"
                        color: Theme.textMuted
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                    }

                    delegate: Rectangle {
                        width: exportList.width
                        height: 72
                        radius: 6
                        color: delegateMouseArea.containsMouse ? Theme.bgInput : "#252536"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 12

                            // Status indicator dot
                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                color: {
                                    switch (modelData.status) {
                                    case "pending": return Theme.accentPrimary
                                    case "running": return Theme.accentWarning
                                    case "verifying": return Theme.accentWarning
                                    case "succeeded": return Theme.accentSuccess
                                    case "failed": return Theme.accentError
                                    default: return Theme.textMuted
                                    }
                                }
                            }

                            // Artifact info
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Label {
                                        text: modelData.id.substring(0, 8) + "..."
                                        color: Theme.accentPrimary
                                        font.pixelSize: 13
                                        font.family: "monospace"
                                    }

                                    // Format badge
                                    Rectangle {
                                        Layout.preferredHeight: 20
                                        Layout.preferredWidth: formatBadgeText.implicitWidth + 12
                                        radius: 4
                                        color: "#89b4fa20"
                                        border.color: Theme.accentPrimary
                                        border.width: 1

                                        Label {
                                            id: formatBadgeText
                                            anchors.centerIn: parent
                                            text: modelData.format ? modelData.format.toUpperCase() : ""
                                            color: Theme.accentPrimary
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    // Status badge
                                    Rectangle {
                                        Layout.preferredHeight: 20
                                        Layout.preferredWidth: statusBadgeText.implicitWidth + 12
                                        radius: 4
                                        color: {
                                            switch (modelData.status) {
                                            case "pending": return "#89b4fa20"
                                            case "running": return "#f9e2af20"
                                            case "verifying": return "#f9e2af20"
                                            case "succeeded": return "#a6e3a120"
                                            case "failed": return "#f38ba820"
                                            default: return "#45475a20"
                                            }
                                        }
                                        border.color: {
                                            switch (modelData.status) {
                                            case "pending": return Theme.accentPrimary
                                            case "running": return Theme.accentWarning
                                            case "verifying": return Theme.accentWarning
                                            case "succeeded": return Theme.accentSuccess
                                            case "failed": return Theme.accentError
                                            default: return Theme.borderNormal
                                            }
                                        }
                                        border.width: 1

                                        Label {
                                            id: statusBadgeText
                                            anchors.centerIn: parent
                                            text: modelData.status || "pending"
                                            color: {
                                                switch (modelData.status) {
                                                case "pending": return Theme.accentPrimary
                                                case "running": return Theme.accentWarning
                                                case "verifying": return Theme.accentWarning
                                                case "succeeded": return Theme.accentSuccess
                                                case "failed": return Theme.accentError
                                                default: return Theme.textMuted
                                                }
                                            }
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    Item { Layout.fillWidth: true }
                                }

                                // Output path
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.outputPath || "N/A"
                                    color: Theme.textSecondary
                                    font.pixelSize: 11
                                    font.family: "monospace"
                                    elide: Text.ElideMiddle
                                }

                                // Timestamp
                                Label {
                                    text: modelData.createdAt || "N/A"
                                    color: Theme.textMuted
                                    font.pixelSize: 10
                                }
                            }

                            // Verify button (only for succeeded exports)
                            Button {
                                text: "Verify"
                                visible: modelData.status === "succeeded"
                                flat: true
                                Layout.preferredWidth: 64

                                background: Rectangle {
                                    color: parent.pressed ? "#74c7a0" : "#a6e3a120"
                                    radius: 4
                                    border.color: Theme.accentSuccess
                                    border.width: 1
                                    implicitHeight: 28
                                }

                                contentItem: Label {
                                    text: parent.text
                                    color: Theme.accentSuccess
                                    font.pixelSize: 11
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: {
                                    if (exportService.verifyExport(modelData.id)) {
                                        statusLabel.text = "Verification started for " + modelData.id.substring(0, 8) + "..."
                                        statusLabel.color = Theme.accentWarning
                                        refreshExports()
                                    } else {
                                        statusLabel.text = "Failed to start verification"
                                        statusLabel.color = Theme.accentError
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: delegateMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }
                }
            }
        }
    }
}
