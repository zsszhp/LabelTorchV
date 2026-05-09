// TrainingPage.qml - Training workbench
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string currentProjectId: ""
    property string currentRunId: ""
    property string currentRunStatus: ""

    onCurrentProjectIdChanged: {
        trainingModel.setProjectId(currentProjectId)
        snapshotModel.setDatasetId("")  // Reset snapshot filter; will be set via dataset
        runHistoryList.currentIndex = -1
        currentRunId = ""
        currentRunStatus = ""
        logView.clear()
    }

    // Auto-select model family based on project task type
    function applyTaskTypeToModelFamily(taskType) {
        if (!configPanel) return
        switch (taskType) {
            case "detect":
                configPanel.modelFamily = "yolov8"
                break
            case "obb":
                configPanel.modelFamily = "yolov8_obb"
                break
            case "classify":
                configPanel.modelFamily = "yolov8_cls"
                break
            case "anomaly":
                configPanel.modelFamily = "anomaly"
                break
        }
    }

    // Sync with global task type changes
    Connections {
        target: ApplicationWindow.window
        function onCurrentTaskTypeChanged() {
            root.applyTaskTypeToModelFamily(ApplicationWindow.window.currentTaskType)
        }
    }

    // On load, set initial model family from project task type
    Component.onCompleted: {
        if (appController.projectOpen) {
            root.applyTaskTypeToModelFamily(projectService.getTaskType(appController.currentProjectId))
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // Left panel: Configuration + Controls
        Rectangle {
            Layout.preferredWidth: 380
            Layout.fillHeight: true
            color: "#181825"
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // Section title
                Label {
                    text: "New Training Run"
                    color: "#89b4fa"
                    font.pixelSize: 16
                    font.bold: true
                }

                // Dataset snapshot selector
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Snapshot:"
                        color: "#cdd6f4"
                        font.pixelSize: 13
                        Layout.preferredWidth: 72
                    }

                    ComboBox {
                        id: snapshotCombo
                        Layout.fillWidth: true
                        model: snapshotModel
                        textRole: "snapshotId"
                        valueRole: "snapshotId"
                        displayText: currentIndex >= 0 ?
                            snapshotModel.data(snapshotModel.index(currentIndex, 0), 257) ?
                            snapshotModel.data(snapshotModel.index(currentIndex, 0), 257).substring(0, 8) + "..." :
                            "Select snapshot" : "Select snapshot"

                        contentItem: Label {
                            text: snapshotCombo.displayText
                            color: "#cdd6f4"
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }

                        background: Rectangle {
                            color: "#313244"
                            radius: 4
                            border.color: snapshotCombo.activeFocus ? "#89b4fa" : "#45475a"
                            border.width: 1
                        }

                        popup: Popup {
                            y: snapshotCombo.height
                            width: snapshotCombo.width
                            implicitHeight: Math.min(contentItem.implicitHeight, 300)
                            padding: 1

                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: snapshotCombo.popup.visible ? snapshotCombo.delegateModel : null
                                currentIndex: snapshotCombo.highlightedIndex
                            }

                            background: Rectangle {
                                color: "#1e1e2e"
                                border.color: "#45475a"
                                radius: 4
                            }
                        }

                        delegate: ItemDelegate {
                            width: snapshotCombo.width
                            contentItem: Label {
                                text: model.snapshotId.substring(0, 8) + "... (" + model.sampleCount + " samples, train:" + model.trainCount + " val:" + model.valCount + ")"
                                color: highlighted ? "#89b4fa" : "#cdd6f4"
                                font.pixelSize: 12
                                font.family: "monospace"
                                verticalAlignment: Text.AlignVCenter
                            }
                            highlighted: snapshotCombo.highlightedIndex === index
                            background: Rectangle {
                                color: highlighted ? "#313244" : "#1e1e2e"
                            }
                        }

                        onActivated: {
                            // Refresh snapshot details if needed
                        }
                    }
                }

                // Snapshot info display
                Label {
                    id: snapshotInfoLabel
                    Layout.fillWidth: true
                    color: "#a6adc8"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    visible: text !== ""

                    text: {
                        if (snapshotCombo.currentIndex < 0) return ""
                        var idx = snapshotCombo.currentIndex
                        var count = snapshotModel.data(snapshotModel.index(idx, 0), 259) // TrainCountRole
                        var val = snapshotModel.data(snapshotModel.index(idx, 0), 260)   // ValCountRole
                        var tax = snapshotModel.data(snapshotModel.index(idx, 0), 261)   // TaxonomyVersionRole
                        if (count === undefined) return ""
                        return "Train: " + count + " | Val: " + val + " | Taxonomy: " + (tax || "unknown")
                    }
                }

                // Adapter selector
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Adapter:"
                        color: "#cdd6f4"
                        font.pixelSize: 13
                        Layout.preferredWidth: 72
                    }

                    ComboBox {
                        id: adapterCombo
                        Layout.fillWidth: true
                        model: trainingService.listAdapters()
                        currentIndex: {
                            var adapters = trainingService.listAdapters()
                            var idx = adapters.indexOf("ultralytics")
                            return idx >= 0 ? idx : 0
                        }

                        contentItem: Label {
                            text: adapterCombo.displayText
                            color: "#cdd6f4"
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }

                        background: Rectangle {
                            color: "#313244"
                            radius: 4
                            border.color: adapterCombo.activeFocus ? "#89b4fa" : "#45475a"
                            border.width: 1
                        }

                        popup: Popup {
                            y: adapterCombo.height
                            width: adapterCombo.width
                            implicitHeight: contentItem.implicitHeight
                            padding: 1

                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: adapterCombo.popup.visible ? adapterCombo.delegateModel : null
                                currentIndex: adapterCombo.highlightedIndex
                            }

                            background: Rectangle {
                                color: "#1e1e2e"
                                border.color: "#45475a"
                                radius: 4
                            }
                        }

                        delegate: ItemDelegate {
                            width: adapterCombo.width
                            contentItem: Label {
                                text: modelData
                                color: highlighted ? "#89b4fa" : "#cdd6f4"
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }
                            highlighted: adapterCombo.highlightedIndex === index
                            background: Rectangle {
                                color: highlighted ? "#313244" : "#1e1e2e"
                            }
                        }

                        onActivated: {
                            configPanel.adapter = adapterCombo.currentText
                        }
                    }
                }

                // Config panel
                ConfigPanel {
                    id: configPanel
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                // OBB task type indicator
                Label {
                    id: taskTypeIndicator
                    Layout.fillWidth: true
                    visible: configPanel.modelFamily === "yolov8_obb"
                    text: "[OBB] Oriented Bounding Box training mode"
                    color: "#f9e2af"
                    font.pixelSize: 11
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                // Action buttons
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        id: startBtn
                        text: "Start Training"
                        highlighted: true
                        enabled: snapshotCombo.currentIndex >= 0 && currentRunStatus !== "running"
                        Layout.fillWidth: true

                        background: Rectangle {
                            color: parent.enabled ? (parent.pressed ? "#74c7a0" : "#a6e3a1") : "#45475a"
                            radius: 6
                            implicitHeight: 36
                        }

                        contentItem: Label {
                            text: parent.text
                            color: parent.enabled ? "#1e1e2e" : "#6c7086"
                            font.pixelSize: 13
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            if (currentProjectId === "") return
                            var snapshotId = snapshotCombo.currentValue
                            if (!snapshotId) return
                            var configJson = configPanel.getConfigJson()
                            var runId = trainingService.createRun(currentProjectId, snapshotId, configJson)
                            if (runId !== "") {
                                currentRunId = runId
                                currentRunStatus = "draft"
                                statusLabel.text = "Run created: " + runId.substring(0, 8) + "..."
                                statusLabel.color = "#a6e3a1"
                                logView.clear()
                                logView.appendLog("[LabelTorch] Training run created: " + runId)
                                logView.appendLog("[LabelTorch] Starting training...")
                                if (trainingService.startTraining(runId)) {
                                    currentRunStatus = "running"
                                    trainingModel.refresh()
                                    statusLabel.text = "Training started: " + runId.substring(0, 8) + "..."
                                } else {
                                    statusLabel.text = "Failed to start training"
                                    statusLabel.color = "#f38ba8"
                                    logView.appendLog("[LabelTorch] ERROR: Failed to start training")
                                }
                            } else {
                                statusLabel.text = "Failed to create training run"
                                statusLabel.color = "#f38ba8"
                            }
                        }
                    }

                    Button {
                        id: stopBtn
                        text: "Stop"
                        enabled: currentRunStatus === "running"
                        Layout.preferredWidth: 80

                        background: Rectangle {
                            color: parent.enabled ? (parent.pressed ? "#d6758e" : "#f38ba8") : "#45475a"
                            radius: 6
                            implicitHeight: 36
                        }

                        contentItem: Label {
                            text: parent.text
                            color: parent.enabled ? "#1e1e2e" : "#6c7086"
                            font.pixelSize: 13
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            if (currentRunId !== "" && trainingService.stopTraining(currentRunId)) {
                                currentRunStatus = "cancelled"
                                trainingModel.refresh()
                                logView.appendLog("[LabelTorch] Training stopped by user")
                            }
                        }
                    }
                }

                // Status label
                Label {
                    id: statusLabel
                    Layout.fillWidth: true
                    text: ""
                    color: "#a6e3a1"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }

        // Right panel: Log viewer + Run history
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#181825"
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // Tab bar for Log / History
                TabBar {
                    id: rightTabs
                    Layout.fillWidth: true

                    background: Rectangle { color: "transparent" }

                    TabButton {
                        text: "Training Log"
                        font.pixelSize: 13

                        contentItem: Label {
                            text: parent.text
                            color: parent.checked ? "#89b4fa" : "#6c7086"
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: parent.checked ? "#313244" : "transparent"
                            radius: 4
                        }
                    }

                    TabButton {
                        text: "Run History"
                        font.pixelSize: 13

                        contentItem: Label {
                            text: parent.text
                            color: parent.checked ? "#89b4fa" : "#6c7086"
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: parent.checked ? "#313244" : "transparent"
                            radius: 4
                        }
                    }
                }

                // Stack layout
                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: rightTabs.currentIndex

                    // Tab 0: Log viewer
                    LogView {
                        id: logView
                    }

                    // Tab 1: Run history
                    ListView {
                        id: runHistoryList
                        clip: true
                        model: trainingModel
                        spacing: 4

                        Label {
                            anchors.centerIn: parent
                            visible: runHistoryList.count === 0
                            text: "No training runs yet"
                            color: "#6c7086"
                            font.pixelSize: 14
                        }

                        delegate: Rectangle {
                            width: runHistoryList.width
                            height: 56
                            radius: 6
                            color: mouseArea.containsMouse ? "#313244" : "#252536"
                            border.color: model.runId === currentRunId ? "#89b4fa" : "transparent"
                            border.width: model.runId === currentRunId ? 1 : 0

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
                                        switch (model.status) {
                                        case "running": return "#f9e2af"
                                        case "succeeded": return "#a6e3a1"
                                        case "failed": return "#f38ba8"
                                        case "cancelled": return "#6c7086"
                                        case "draft": return "#89b4fa"
                                        default: return "#6c7086"
                                        }
                                    }
                                }

                                // Run ID
                                Label {
                                    text: model.runId.substring(0, 8) + "..."
                                    color: "#89b4fa"
                                    font.pixelSize: 13
                                    font.family: "monospace"
                                    Layout.preferredWidth: 80
                                }

                                // Status badge
                                Rectangle {
                                    Layout.preferredHeight: 22
                                    Layout.preferredWidth: statusBadgeText.implicitWidth + 16
                                    radius: 4
                                    color: {
                                        switch (model.status) {
                                        case "running": return "#f9e2af20"
                                        case "succeeded": return "#a6e3a120"
                                        case "failed": return "#f38ba820"
                                        case "cancelled": return "#6c708620"
                                        case "draft": return "#89b4fa20"
                                        default: return "#45475a"
                                        }
                                    }
                                    border.color: {
                                        switch (model.status) {
                                        case "running": return "#f9e2af"
                                        case "succeeded": return "#a6e3a1"
                                        case "failed": return "#f38ba8"
                                        case "cancelled": return "#6c7086"
                                        case "draft": return "#89b4fa"
                                        default: return "#45475a"
                                        }
                                    }
                                    border.width: 1

                                    Label {
                                        id: statusBadgeText
                                        anchors.centerIn: parent
                                        text: model.status
                                        color: {
                                            switch (model.status) {
                                            case "running": return "#f9e2af"
                                            case "succeeded": return "#a6e3a1"
                                            case "failed": return "#f38ba8"
                                            case "cancelled": return "#6c7086"
                                            case "draft": return "#89b4fa"
                                            default: return "#6c7086"
                                            }
                                        }
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }

                                // Snapshot ID
                                Label {
                                    text: "Snapshot: " + model.snapshotId.substring(0, 8) + "..."
                                    color: "#a6adc8"
                                    font.pixelSize: 12
                                }

                                // Time info
                                Label {
                                    text: {
                                        if (model.startedAt && model.startedAt !== "") {
                                            return model.startedAt
                                        }
                                        return "Not started"
                                    }
                                    color: "#6c7086"
                                    font.pixelSize: 11
                                }

                                Item { Layout.fillWidth: true }

                                // Delete button (only for draft/cancelled/failed)
                                Button {
                                    text: "Delete"
                                    flat: true
                                    visible: model.status === "draft" || model.status === "cancelled" || model.status === "failed"
                                    palette.buttonText: "#f38ba8"
                                    font.pixelSize: 11
                                    onClicked: {
                                        if (trainingService.deleteRun(model.runId)) {
                                            trainingModel.refresh()
                                            if (model.runId === currentRunId) {
                                                currentRunId = ""
                                                currentRunStatus = ""
                                                logView.clear()
                                            }
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: mouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    currentRunId = model.runId
                                    currentRunStatus = model.status
                                    // Load log for this run
                                    var runDetails = trainingService.getRun(model.runId)
                                    logView.clear()
                                    logView.appendLog("[LabelTorch] Run: " + model.runId)
                                    logView.appendLog("[LabelTorch] Status: " + model.status)
                                    logView.appendLog("[LabelTorch] Config: " + model.configJson)
                                    if (runDetails.startedAt) {
                                        logView.appendLog("[LabelTorch] Started: " + runDetails.startedAt)
                                    }
                                    if (runDetails.finishedAt) {
                                        logView.appendLog("[LabelTorch] Finished: " + runDetails.finishedAt)
                                    }
                                    // Switch to log tab
                                    rightTabs.currentIndex = 0
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
