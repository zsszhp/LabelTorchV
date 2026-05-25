// TrainingPage.qml - Training workbench
import QtQuick
import QtQuick.Controls
import LabelTorch.Shell
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
                configPanel.adapter = "ultralytics"
                break
            case "obb":
                configPanel.modelFamily = "yolov8_obb"
                configPanel.adapter = "ultralytics"
                break
            case "classify":
                configPanel.modelFamily = "yolov8_cls"
                configPanel.adapter = "ultralytics"
                break
            case "anomaly":
                configPanel.modelFamily = "anomaly"
                configPanel.adapter = "anomalib"
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
            color: Theme.bgCard
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // 区域标题
                Label {
                    text: "新建训练任务"
                    color: Theme.accentPrimary
                    font.pixelSize: 16
                    font.bold: true
                }

                // 数据快照选择器
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "数据快照:"
                        color: Theme.textPrimary
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
                            "选择数据快照" : "选择数据快照"

                        contentItem: Label {
                            text: snapshotCombo.displayText
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: 4
                            border.color: snapshotCombo.activeFocus ? Theme.accentPrimary : Theme.borderNormal
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
                                color: Theme.bgPrimary
                                border.color: Theme.borderNormal
                                radius: 4
                            }
                        }

                        delegate: ItemDelegate {
                            width: snapshotCombo.width
                            contentItem: Label {
                                text: model.snapshotId.substring(0, 8) + "... (" + model.sampleCount + " 样本, train:" + model.trainCount + " val:" + model.valCount + ")"
                                color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                font.pixelSize: 12
                                font.family: "monospace"
                                verticalAlignment: Text.AlignVCenter
                            }
                            highlighted: snapshotCombo.highlightedIndex === index
                            background: Rectangle {
                                color: highlighted ? Theme.bgInput : Theme.bgPrimary
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
                    color: Theme.textSecondary
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

                // 适配器选择器
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "训练适配器:"
                        color: Theme.textPrimary
                        font.pixelSize: 13
                        Layout.preferredWidth: 72
                    }

                    ComboBox {
                        id: adapterCombo
                        Layout.fillWidth: true
                        model: trainingService.listAdapters()
                        currentIndex: {
                            var adapters = trainingService.listAdapters()
                            var idx = adapters.indexOf(configPanel.adapter)
                            return idx >= 0 ? idx : 0
                        }

                        contentItem: Label {
                            text: adapterCombo.displayText
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: 4
                            border.color: adapterCombo.activeFocus ? Theme.accentPrimary : Theme.borderNormal
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
                                color: Theme.bgPrimary
                                border.color: Theme.borderNormal
                                radius: 4
                            }
                        }

                        delegate: ItemDelegate {
                            width: adapterCombo.width
                            contentItem: Label {
                                text: modelData
                                color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }
                            highlighted: adapterCombo.highlightedIndex === index
                            background: Rectangle {
                                color: highlighted ? Theme.bgInput : Theme.bgPrimary
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

                // OBB任务类型指示器
                Label {
                    id: taskTypeIndicator
                    Layout.fillWidth: true
                    visible: configPanel.modelFamily === "yolov8_obb"
                    text: "[OBB] 旋转边界框训练模式"
                    color: Theme.accentWarning
                    font.pixelSize: 11
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                // 操作按钮
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        id: startBtn
                        text: "开始训练"
                        highlighted: true
                        enabled: snapshotCombo.currentIndex >= 0 && currentRunStatus !== "running"
                        Layout.fillWidth: true

                        background: Rectangle {
                            color: parent.enabled ? (parent.pressed ? "#74c7a0" : Theme.accentSuccess) : Theme.borderNormal
                            radius: 6
                            implicitHeight: 36
                        }

                        contentItem: Label {
                            text: parent.text
                            color: parent.enabled ? Theme.bgPrimary : Theme.textMuted
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
                                statusLabel.text = "训练任务已创建:" + runId.substring(0, 8) + "..."
                                statusLabel.color = Theme.accentSuccess
                                logView.clear()
                                logView.appendLog("[LabelTorch] Training run created: " + runId)
                                logView.appendLog("[LabelTorch] Starting training...")
                                if (trainingService.startTraining(runId)) {
                                    currentRunStatus = "running"
                                    trainingModel.refresh()
                                    statusLabel.text = "训练已启动:" + runId.substring(0, 8) + "..."
                                } else {
                                    statusLabel.text = "训练启动失败"
                                    statusLabel.color = Theme.accentError
                                    logView.appendLog("[LabelTorch] ERROR: Failed to start training")
                                }
                            } else {
                                statusLabel.text = "创建训练任务失败"
                                statusLabel.color = Theme.accentError
                            }
                        }
                    }

                    Button {
                        id: stopBtn
                        text: "停止"
                        enabled: currentRunStatus === "running"
                        Layout.preferredWidth: 80

                        background: Rectangle {
                            color: parent.enabled ? (parent.pressed ? "#d6758e" : Theme.accentError) : Theme.borderNormal
                            radius: 6
                            implicitHeight: 36
                        }

                        contentItem: Label {
                            text: parent.text
                            color: parent.enabled ? Theme.bgPrimary : Theme.textMuted
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

                // 状态标签
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

        // 右侧面板：日志查看器 + 运行历史
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgCard
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // 日志/历史选项卡
                TabBar {
                    id: rightTabs
                    Layout.fillWidth: true

                    background: Rectangle { color: "transparent" }

                    TabButton {
                        text: "训练日志"
                        font.pixelSize: 13

                        contentItem: Label {
                            text: parent.text
                            color: parent.checked ? Theme.accentPrimary : Theme.textMuted
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: parent.checked ? Theme.bgInput : "transparent"
                            radius: 4
                        }
                    }

                    TabButton {
                        text: "运行历史"
                        font.pixelSize: 13

                        contentItem: Label {
                            text: parent.text
                            color: parent.checked ? Theme.accentPrimary : Theme.textMuted
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: parent.checked ? Theme.bgInput : "transparent"
                            radius: 4
                        }
                    }
                }

                // 堆叠布局
                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: rightTabs.currentIndex

                    // 选项卡0：日志查看器
                    LogView {
                        id: logView
                    }

                    // 选项卡1：运行历史
                    ListView {
                        id: runHistoryList
                        clip: true
                        model: trainingModel
                        spacing: 4

                        Label {
                            anchors.centerIn: parent
                            visible: runHistoryList.count === 0
                            text: "暂无训练记录"
                            color: Theme.textMuted
                            font.pixelSize: 14
                        }

                        delegate: Rectangle {
                            width: runHistoryList.width
                            height: 56
                            radius: 6
                            color: mouseArea.containsMouse ? Theme.bgInput : "#252536"
                            border.color: model.runId === currentRunId ? Theme.accentPrimary : "transparent"
                            border.width: model.runId === currentRunId ? 1 : 0

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 12

                                // 状态指示圆点
                                Rectangle {
                                    width: 10
                                    height: 10
                                    radius: 5
                                    color: {
                                        switch (model.status) {
                                        case "running": return Theme.accentWarning
                                        case "succeeded": return Theme.accentSuccess
                                        case "failed": return Theme.accentError
                                        case "cancelled": return Theme.textMuted
                                        case "draft": return Theme.accentPrimary
                                        default: return Theme.textMuted
                                        }
                                    }
                                }

                                // 运行ID
                                Label {
                                    text: model.runId.substring(0, 8) + "..."
                                    color: Theme.accentPrimary
                                    font.pixelSize: 13
                                    font.family: "monospace"
                                    Layout.preferredWidth: 80
                                }

                                // 状态徽章
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
                                        default: return Theme.borderNormal
                                        }
                                    }
                                    border.color: {
                                        switch (model.status) {
                                        case "running": return Theme.accentWarning
                                        case "succeeded": return Theme.accentSuccess
                                        case "failed": return Theme.accentError
                                        case "cancelled": return Theme.textMuted
                                        case "draft": return Theme.accentPrimary
                                        default: return Theme.borderNormal
                                        }
                                    }
                                    border.width: 1

                                    Label {
                                        id: statusBadgeText
                                        anchors.centerIn: parent
                                        text: model.status
                                        color: {
                                            switch (model.status) {
                                            case "running": return Theme.accentWarning
                                            case "succeeded": return Theme.accentSuccess
                                            case "failed": return Theme.accentError
                                            case "cancelled": return Theme.textMuted
                                            case "draft": return Theme.accentPrimary
                                            default: return Theme.textMuted
                                            }
                                        }
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }

                                // 快照ID
                                Label {
                                    text: "数据快照: " + model.snapshotId.substring(0, 8) + "..."
                                    color: Theme.textSecondary
                                    font.pixelSize: 12
                                }

                                // 时间信息
                                Label {
                                    text: {
                                        if (model.startedAt && model.startedAt !== "") {
                                            return model.startedAt
                                        }
                                        return "未开始"
                                    }
                                    color: Theme.textMuted
                                    font.pixelSize: 11
                                }

                                Item { Layout.fillWidth: true }

                                // 删除按钮（仅草稿/已取消/失败状态可用）
                                Button {
                                    text: "删除"
                                    flat: true
                                    visible: model.status === "draft" || model.status === "cancelled" || model.status === "failed"
                                    palette.buttonText: Theme.accentError
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
