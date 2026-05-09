// AssistedLabelPanel.qml - Inference and assisted labeling panel
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string currentProjectId: ""
    property string currentDatasetId: ""
    property string selectedBatchId: ""
    property var selectedBatch: null
    property var candidates: []
    property var batchStats: ({"total":0,"confirmed":0,"rejected":0,"pending":0,"edited":0})

    onCurrentDatasetIdChanged: {
        selectedBatchId = ""
        selectedBatch = null
        candidates = []
        refreshBatches()
    }

    function refreshBatches() {
        if (currentDatasetId === "") {
            batchListModel.clear()
            return
        }
        var batches = inferenceService.listBatches(currentDatasetId)
        batchListModel.clear()
        for (var i = 0; i < batches.length; i++) {
            batchListModel.append(batches[i])
        }
    }

    function loadCandidates(batchId) {
        selectedBatchId = batchId
        selectedBatch = inferenceService.getBatchStatus(batchId)
        candidates = assistedLabelService.getCandidates(batchId)
        batchStats = assistedLabelService.getBatchStats(batchId)
        candidateListModel.clear()
        for (var i = 0; i < candidates.length; i++) {
            candidateListModel.append(candidates[i])
        }
    }

    function refreshCandidates() {
        if (selectedBatchId !== "") {
            loadCandidates(selectedBatchId)
        }
    }

    ListModel {
        id: batchListModel
    }

    ListModel {
        id: candidateListModel
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // Left panel: Inference controls
        Rectangle {
            Layout.preferredWidth: 360
            Layout.fillHeight: true
            color: "#181825"
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // Section title
                Label {
                    text: "Assisted Labeling"
                    color: "#89b4fa"
                    font.pixelSize: 16
                    font.bold: true
                }

                // Model version selector
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Model:"
                        color: "#cdd6f4"
                        font.pixelSize: 13
                        Layout.preferredWidth: 52
                    }

                    ComboBox {
                        id: modelVersionCombo
                        Layout.fillWidth: true
                        model: modelVersionModel
                        textRole: "id"
                        valueRole: "id"
                        displayText: currentIndex >= 0 ?
                            modelVersionModel.data(modelVersionModel.index(currentIndex, 0), 257) ?
                            modelVersionModel.data(modelVersionModel.index(currentIndex, 0), 257).substring(0, 8) + "..." :
                            "Select model" : "Select model"

                        contentItem: Label {
                            text: modelVersionCombo.displayText
                            color: "#cdd6f4"
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }

                        background: Rectangle {
                            color: "#313244"
                            radius: 4
                            border.color: modelVersionCombo.activeFocus ? "#89b4fa" : "#45475a"
                            border.width: 1
                        }

                        popup: Popup {
                            y: modelVersionCombo.height
                            width: modelVersionCombo.width
                            implicitHeight: Math.min(contentItem.implicitHeight, 300)
                            padding: 1

                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: modelVersionCombo.popup.visible ? modelVersionCombo.delegateModel : null
                                currentIndex: modelVersionCombo.highlightedIndex
                            }

                            background: Rectangle {
                                color: "#1e1e2e"
                                border.color: "#45475a"
                                radius: 4
                            }
                        }

                        delegate: ItemDelegate {
                            width: modelVersionCombo.width
                            contentItem: Label {
                                text: model.id.substring(0, 8) + "..."
                                color: highlighted ? "#89b4fa" : "#cdd6f4"
                                font.pixelSize: 12
                                font.family: "monospace"
                                verticalAlignment: Text.AlignVCenter
                            }
                            highlighted: modelVersionCombo.highlightedIndex === index
                            background: Rectangle {
                                color: highlighted ? "#313244" : "#1e1e2e"
                            }
                        }
                    }
                }

                // Confidence threshold slider
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Conf:"
                        color: "#cdd6f4"
                        font.pixelSize: 13
                        Layout.preferredWidth: 52
                    }

                    Slider {
                        id: confSlider
                        Layout.fillWidth: true
                        from: 0.0
                        to: 1.0
                        value: 0.25
                        stepSize: 0.05

                        background: Rectangle {
                            x: confSlider.leftPadding
                            y: confSlider.topPadding + confSlider.availableHeight / 2 - height / 2
                            width: confSlider.availableWidth
                            height: 4
                            radius: 2
                            color: "#313244"

                            Rectangle {
                                width: confSlider.visualPosition * parent.width
                                height: parent.height
                                color: "#89b4fa"
                                radius: 2
                            }
                        }

                        handle: Rectangle {
                            x: confSlider.leftPadding + confSlider.visualPosition * (confSlider.availableWidth - width)
                            y: confSlider.topPadding + confSlider.availableHeight / 2 - height / 2
                            width: 16
                            height: 16
                            radius: 8
                            color: confSlider.pressed ? "#74a8f0" : "#89b4fa"
                        }
                    }

                    Label {
                        text: confSlider.value.toFixed(2)
                        color: "#a6adc8"
                        font.pixelSize: 12
                        font.family: "monospace"
                        Layout.preferredWidth: 36
                    }
                }

                // IoU threshold slider
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "IoU:"
                        color: "#cdd6f4"
                        font.pixelSize: 13
                        Layout.preferredWidth: 52
                    }

                    Slider {
                        id: iouSlider
                        Layout.fillWidth: true
                        from: 0.0
                        to: 1.0
                        value: 0.45
                        stepSize: 0.05

                        background: Rectangle {
                            x: iouSlider.leftPadding
                            y: iouSlider.topPadding + iouSlider.availableHeight / 2 - height / 2
                            width: iouSlider.availableWidth
                            height: 4
                            radius: 2
                            color: "#313244"

                            Rectangle {
                                width: iouSlider.visualPosition * parent.width
                                height: parent.height
                                color: "#89b4fa"
                                radius: 2
                            }
                        }

                        handle: Rectangle {
                            x: iouSlider.leftPadding + iouSlider.visualPosition * (iouSlider.availableWidth - width)
                            y: iouSlider.topPadding + iouSlider.availableHeight / 2 - height / 2
                            width: 16
                            height: 16
                            radius: 8
                            color: iouSlider.pressed ? "#74a8f0" : "#89b4fa"
                        }
                    }

                    Label {
                        text: iouSlider.value.toFixed(2)
                        color: "#a6adc8"
                        font.pixelSize: 12
                        font.family: "monospace"
                        Layout.preferredWidth: 36
                    }
                }

                // Target scope selector
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Scope:"
                        color: "#cdd6f4"
                        font.pixelSize: 13
                        Layout.preferredWidth: 52
                    }

                    ComboBox {
                        id: scopeCombo
                        Layout.fillWidth: true
                        model: ["all", "unlabeled", "failed"]
                        currentIndex: 0

                        contentItem: Label {
                            text: scopeCombo.currentText
                            color: "#cdd6f4"
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }

                        background: Rectangle {
                            color: "#313244"
                            radius: 4
                            border.color: scopeCombo.activeFocus ? "#89b4fa" : "#45475a"
                            border.width: 1
                        }

                        popup: Popup {
                            y: scopeCombo.height
                            width: scopeCombo.width
                            implicitHeight: Math.min(contentItem.implicitHeight, 200)
                            padding: 1

                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: scopeCombo.popup.visible ? scopeCombo.delegateModel : null
                                currentIndex: scopeCombo.highlightedIndex
                            }

                            background: Rectangle {
                                color: "#1e1e2e"
                                border.color: "#45475a"
                                radius: 4
                            }
                        }

                        delegate: ItemDelegate {
                            width: scopeCombo.width
                            contentItem: Label {
                                text: modelData
                                color: highlighted ? "#89b4fa" : "#cdd6f4"
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }
                            highlighted: scopeCombo.highlightedIndex === index
                            background: Rectangle {
                                color: highlighted ? "#313244" : "#1e1e2e"
                            }
                        }
                    }
                }

                // Run Inference button
                Button {
                    id: runBtn
                    text: "Run Inference"
                    Layout.fillWidth: true
                    enabled: modelVersionCombo.currentIndex >= 0 && currentDatasetId !== ""

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
                        var versionId = modelVersionCombo.currentValue
                        if (!versionId) return
                        var batchId = inferenceService.runInference(
                            versionId,
                            currentDatasetId,
                            scopeCombo.currentText,
                            confSlider.value,
                            iouSlider.value
                        )
                        if (batchId !== "") {
                            statusLabel.text = "Batch created: " + batchId.substring(0, 8) + "..."
                            statusLabel.color = "#a6e3a1"
                            refreshBatches()
                        } else {
                            statusLabel.text = "Failed to create batch"
                            statusLabel.color = "#f38ba8"
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

                // Separator
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#313244"
                }

                // Batch list header
                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: "Batches"
                        color: "#89b4fa"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "Refresh"
                        flat: true
                        palette.buttonText: "#89b4fa"
                        font.pixelSize: 12
                        onClicked: refreshBatches()
                    }
                }

                // Batch list
                ListView {
                    id: batchList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: batchListModel
                    spacing: 4

                    Label {
                        anchors.centerIn: parent
                        visible: batchList.count === 0
                        text: "No inference batches yet"
                        color: "#6c7086"
                        font.pixelSize: 14
                    }

                    delegate: Rectangle {
                        width: batchList.width
                        height: 48
                        radius: 6
                        color: batchMouseArea.containsMouse ? "#313244" : "#252536"
                        border.color: model.id === selectedBatchId ? "#89b4fa" : "transparent"
                        border.width: model.id === selectedBatchId ? 1 : 0

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            // Status dot
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: {
                                    switch (model.status) {
                                    case "completed": return "#a6e3a1"
                                    case "running": return "#f9e2af"
                                    case "cancelled": return "#6c7086"
                                    case "failed": return "#f38ba8"
                                    default: return "#89b4fa"
                                    }
                                }
                            }

                            Label {
                                text: model.id.substring(0, 8) + "..."
                                color: "#89b4fa"
                                font.pixelSize: 12
                                font.family: "monospace"
                            }

                            Label {
                                text: model.status
                                color: "#a6adc8"
                                font.pixelSize: 11
                            }

                            Label {
                                text: "conf:" + parseFloat(model.confThreshold).toFixed(2)
                                color: "#6c7086"
                                font.pixelSize: 11
                            }

                            Item { Layout.fillWidth: true }

                            // Cancel button for running/pending batches
                            Button {
                                text: "Cancel"
                                flat: true
                                visible: model.status === "pending" || model.status === "running"
                                palette.buttonText: "#f38ba8"
                                font.pixelSize: 11
                                onClicked: {
                                    inferenceService.cancelBatch(model.id)
                                    refreshBatches()
                                }
                            }
                        }

                        MouseArea {
                            id: batchMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: loadCandidates(model.id)
                        }
                    }
                }
            }
        }

        // Right panel: Candidates review
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#181825"
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // Header with stats
                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: selectedBatchId !== "" ?
                            "Candidates - " + selectedBatchId.substring(0, 8) + "..." :
                            "Candidates"
                        color: "#89b4fa"
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        visible: selectedBatchId !== ""
                        text: "Total: " + batchStats.total +
                              " | Confirmed: " + batchStats.confirmed +
                              " | Rejected: " + batchStats.rejected +
                              " | Pending: " + batchStats.pending
                        color: "#a6adc8"
                        font.pixelSize: 12
                    }
                }

                // Batch action buttons
                RowLayout {
                    Layout.fillWidth: true
                    visible: selectedBatchId !== ""
                    spacing: 8

                    Label {
                        text: "Batch confirm above:"
                        color: "#cdd6f4"
                        font.pixelSize: 12
                    }

                    Slider {
                        id: batchConfThresholdSlider
                        Layout.preferredWidth: 140
                        from: 0.0
                        to: 1.0
                        value: 0.5
                        stepSize: 0.05

                        background: Rectangle {
                            x: batchConfThresholdSlider.leftPadding
                            y: batchConfThresholdSlider.topPadding + batchConfThresholdSlider.availableHeight / 2 - height / 2
                            width: batchConfThresholdSlider.availableWidth
                            height: 4
                            radius: 2
                            color: "#313244"
                            Rectangle {
                                width: batchConfThresholdSlider.visualPosition * parent.width
                                height: parent.height
                                color: "#a6e3a1"
                                radius: 2
                            }
                        }

                        handle: Rectangle {
                            x: batchConfThresholdSlider.leftPadding + batchConfThresholdSlider.visualPosition * (batchConfThresholdSlider.availableWidth - width)
                            y: batchConfThresholdSlider.topPadding + batchConfThresholdSlider.availableHeight / 2 - height / 2
                            width: 14
                            height: 14
                            radius: 7
                            color: batchConfThresholdSlider.pressed ? "#7fd491" : "#a6e3a1"
                        }
                    }

                    Label {
                        text: batchConfThresholdSlider.value.toFixed(2)
                        color: "#a6adc8"
                        font.pixelSize: 11
                        font.family: "monospace"
                        Layout.preferredWidth: 32
                    }

                    Button {
                        text: "Confirm All Above"
                        Layout.preferredHeight: 28

                        background: Rectangle {
                            color: parent.pressed ? "#74c7a0" : "#a6e3a1"
                            radius: 4
                        }

                        contentItem: Label {
                            text: parent.text
                            color: "#1e1e2e"
                            font.pixelSize: 11
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            assistedLabelService.confirmAllAboveThreshold(selectedBatchId, batchConfThresholdSlider.value)
                            refreshCandidates()
                        }
                    }

                    Rectangle {
                        width: 1
                        height: 20
                        color: "#313244"
                    }

                    Button {
                        text: "Reject All Below"
                        Layout.preferredHeight: 28

                        background: Rectangle {
                            color: parent.pressed ? "#d6758e" : "#f38ba8"
                            radius: 4
                        }

                        contentItem: Label {
                            text: parent.text
                            color: "#1e1e2e"
                            font.pixelSize: 11
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            assistedLabelService.rejectAllBelowThreshold(selectedBatchId, batchConfThresholdSlider.value)
                            refreshCandidates()
                        }
                    }
                }

                // Candidate list
                ListView {
                    id: candidateList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: candidateListModel
                    spacing: 3

                    Label {
                        anchors.centerIn: parent
                        visible: candidateList.count === 0 && selectedBatchId !== ""
                        text: "No candidates in this batch"
                        color: "#6c7086"
                        font.pixelSize: 14
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: selectedBatchId === ""
                        text: "Select a batch to view candidates"
                        color: "#6c7086"
                        font.pixelSize: 14
                    }

                    delegate: Rectangle {
                        width: candidateList.width
                        height: 40
                        radius: 4
                        color: {
                            switch (model.state) {
                            case "confirmed": return "#a6e3a115"
                            case "rejected": return "#f38ba815"
                            case "edited": return "#f9e2af15"
                            default: return "#252536"
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            // State indicator
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: {
                                    switch (model.state) {
                                    case "confirmed": return "#a6e3a1"
                                    case "rejected": return "#f38ba8"
                                    case "edited": return "#f9e2af"
                                    default: return "#89b4fa"
                                    }
                                }
                            }

                            Label {
                                text: model.className || ("Class " + model.classIndex)
                                color: "#cdd6f4"
                                font.pixelSize: 12
                                Layout.preferredWidth: 100
                            }

                            Label {
                                text: "conf: " + parseFloat(model.confidence).toFixed(3)
                                color: parseFloat(model.confidence) >= 0.5 ? "#a6e3a1" : "#f9e2af"
                                font.pixelSize: 11
                                font.family: "monospace"
                            }

                            Label {
                                text: "[" + parseFloat(model.cx).toFixed(2) + ", " +
                                      parseFloat(model.cy).toFixed(2) + ", " +
                                      parseFloat(model.w).toFixed(2) + ", " +
                                      parseFloat(model.h).toFixed(2) + "]"
                                color: "#6c7086"
                                font.pixelSize: 10
                                font.family: "monospace"
                            }

                            Item { Layout.fillWidth: true }

                            // Confirm/Reject buttons for pending candidates
                            Button {
                                text: "Confirm"
                                visible: model.state === "pending"
                                Layout.preferredHeight: 24
                                Layout.preferredWidth: 60

                                background: Rectangle {
                                    color: parent.pressed ? "#74c7a0" : "#a6e3a1"
                                    radius: 3
                                }

                                contentItem: Label {
                                    text: parent.text
                                    color: "#1e1e2e"
                                    font.pixelSize: 10
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: {
                                    assistedLabelService.confirmCandidate(selectedBatchId, index)
                                    refreshCandidates()
                                }
                            }

                            Button {
                                text: "Reject"
                                visible: model.state === "pending"
                                Layout.preferredHeight: 24
                                Layout.preferredWidth: 52

                                background: Rectangle {
                                    color: parent.pressed ? "#d6758e" : "#f38ba8"
                                    radius: 3
                                }

                                contentItem: Label {
                                    text: parent.text
                                    color: "#1e1e2e"
                                    font.pixelSize: 10
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: {
                                    assistedLabelService.rejectCandidate(selectedBatchId, index)
                                    refreshCandidates()
                                }
                            }

                            // State badge for non-pending
                            Rectangle {
                                visible: model.state !== "pending"
                                height: 20
                                width: stateText.implicitWidth + 12
                                radius: 3
                                color: {
                                    switch (model.state) {
                                    case "confirmed": return "#a6e3a130"
                                    case "rejected": return "#f38ba830"
                                    case "edited": return "#f9e2af30"
                                    default: return "#45475a"
                                    }
                                }
                                border.color: {
                                    switch (model.state) {
                                    case "confirmed": return "#a6e3a1"
                                    case "rejected": return "#f38ba8"
                                    case "edited": return "#f9e2af"
                                    default: return "#45475a"
                                    }
                                }
                                border.width: 1

                                Label {
                                    id: stateText
                                    anchors.centerIn: parent
                                    text: model.state
                                    color: {
                                        switch (model.state) {
                                        case "confirmed": return "#a6e3a1"
                                        case "rejected": return "#f38ba8"
                                        case "edited": return "#f9e2af"
                                        default: return "#6c7086"
                                        }
                                    }
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
