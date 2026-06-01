// AssistedLabelPanel.qml - 辅助标注面板（推理与辅助标注）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme

Item {
    id: root

    property string currentProjectId: ""
    property string currentDatasetId: ""
    property string selectedBatchId: ""
    property var selectedBatch: null
    property var candidates: []
    property var batchStats: ({"total":0,"confirmed":0,"rejected":0,"pending":0,"edited":0})
    property var lowConfSamples: []
    property var confidenceStats: ({"totalCandidates":0,"lowConfCount":0,"highConfCount":0,"averageConfidence":0,"threshold":0.3})
    property var hardCaseQueue: []
    property bool showHardCaseQueue: false

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

    function collectLowConfidence() {
        if (selectedBatchId === "") return
        lowConfSamples = assistedLabelService.getLowConfidenceSamples(selectedBatchId, lowConfThresholdSpin.value)
        confidenceStats = assistedLabelService.getConfidenceStats(selectedBatchId, lowConfThresholdSpin.value)
        lowConfListModel.clear()
        for (var i = 0; i < lowConfSamples.length; i++) {
            lowConfListModel.append(lowConfSamples[i])
        }
    }

    function loadHardCaseQueue() {
        if (selectedBatchId === "" || currentDatasetId === "") return
        hardCaseQueue = assistedLabelService.getHardCaseQueue(selectedBatchId, currentDatasetId, lowConfThresholdSpin.realValue)
    }

    ListModel {
        id: batchListModel
    }

    ListModel {
        id: candidateListModel
    }

    ListModel {
        id: lowConfListModel
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // 左侧面板：推理控制
        Rectangle {
            Layout.preferredWidth: 360
            Layout.fillHeight: true
            color: Theme.bgInput
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // 区域标题
                Label {
                    text: "辅助标注"
                    color: Theme.accentPrimary
                    font.pixelSize: 16
                    font.bold: true
                }

                // 模型版本选择器
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "模型："
                        color: Theme.textPrimary
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
                            modelVersionModel.data(modelVersionModel.index(currentIndex, 0), Qt.UserRole + 1) ?
                            modelVersionModel.data(modelVersionModel.index(currentIndex, 0), Qt.UserRole + 1).substring(0, 8) + "..." :
                            "选择模型" : "选择模型"

                        contentItem: Label {
                            text: modelVersionCombo.displayText
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }

                        background: Rectangle {
                            color: Theme.bgHover
                            radius: 4
                            border.color: modelVersionCombo.activeFocus ? Theme.accentPrimary : Theme.textDisabled
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
                                color: Theme.bgPrimary
                                border.color: Theme.textDisabled
                                radius: 4
                            }
                        }

                        delegate: ItemDelegate {
                            width: modelVersionCombo.width
                            contentItem: Label {
                                text: model.id.substring(0, 8) + "..."
                                color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                font.pixelSize: 12
                                font.family: "monospace"
                                verticalAlignment: Text.AlignVCenter
                            }
                            highlighted: modelVersionCombo.highlightedIndex === index
                            background: Rectangle {
                                color: highlighted ? Theme.bgHover : Theme.bgPrimary
                            }
                        }
                    }
                }

                // 置信度阈值滑块
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "置信度："
                        color: Theme.textPrimary
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
                            color: Theme.bgHover

                            Rectangle {
                                width: confSlider.visualPosition * parent.width
                                height: parent.height
                                color: Theme.accentPrimary
                                radius: 2
                            }
                        }

                        handle: Rectangle {
                            x: confSlider.leftPadding + confSlider.visualPosition * (confSlider.availableWidth - width)
                            y: confSlider.topPadding + confSlider.availableHeight / 2 - height / 2
                            width: 16
                            height: 16
                            radius: 8
                            color: confSlider.pressed ? Theme.accentPrimary : Theme.accentPrimary
                        }
                    }

                    Label {
                        text: confSlider.value.toFixed(2)
                        color: Theme.textMuted
                        font.pixelSize: 12
                        font.family: "monospace"
                        Layout.preferredWidth: 36
                    }
                }

                // IoU 阈值滑块
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "IoU："
                        color: Theme.textPrimary
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
                            color: Theme.bgHover

                            Rectangle {
                                width: iouSlider.visualPosition * parent.width
                                height: parent.height
                                color: Theme.accentPrimary
                                radius: 2
                            }
                        }

                        handle: Rectangle {
                            x: iouSlider.leftPadding + iouSlider.visualPosition * (iouSlider.availableWidth - width)
                            y: iouSlider.topPadding + iouSlider.availableHeight / 2 - height / 2
                            width: 16
                            height: 16
                            radius: 8
                            color: iouSlider.pressed ? Theme.accentPrimary : Theme.accentPrimary
                        }
                    }

                    Label {
                        text: iouSlider.value.toFixed(2)
                        color: Theme.textMuted
                        font.pixelSize: 12
                        font.family: "monospace"
                        Layout.preferredWidth: 36
                    }
                }

                // 目标范围选择器
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "范围："
                        color: Theme.textPrimary
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
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }

                        background: Rectangle {
                            color: Theme.bgHover
                            radius: 4
                            border.color: scopeCombo.activeFocus ? Theme.accentPrimary : Theme.textDisabled
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
                                color: Theme.bgPrimary
                                border.color: Theme.textDisabled
                                radius: 4
                            }
                        }

                        delegate: ItemDelegate {
                            width: scopeCombo.width
                            contentItem: Label {
                                text: modelData
                                color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }
                            highlighted: scopeCombo.highlightedIndex === index
                            background: Rectangle {
                                color: highlighted ? Theme.bgHover : Theme.bgPrimary
                            }
                        }
                    }
                }

                // 运行推理按钮
                Button {
                    id: runBtn
                    text: "运行推理"
                    Layout.fillWidth: true
                    enabled: modelVersionCombo.currentIndex >= 0 && currentDatasetId !== ""

                    background: Rectangle {
                        color: parent.enabled ? (parent.pressed ? Qt.darker(Theme.accentSuccess, 1.2) : Theme.accentSuccess) : Theme.textDisabled
                        radius: 6
                        implicitHeight: 36
                    }

                    contentItem: Label {
                        text: parent.text
                        color: parent.enabled ? Theme.bgPrimary : Theme.textDisabled
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
                            statusLabel.text = "批次已创建: " + batchId.substring(0, 8) + "..."
                            statusLabel.color = Theme.accentSuccess
                            refreshBatches()
                        } else {
                            statusLabel.text = "创建批次失败"
                            statusLabel.color = Theme.accentError
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

                // 分隔线
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.bgHover
                }

                // 低置信度反馈循环区域
                Label {
                    text: "低置信度反馈"
                    color: Theme.accentWarning
                    font.pixelSize: 14
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "阈值："
                        color: Theme.textPrimary
                        font.pixelSize: 12
                    }

                    SpinBox {
                        id: lowConfThresholdSpin
                        from: 0
                        to: 100
                        value: 30
                        stepSize: 5
                        editable: true

                        property real realValue: value / 100.0

                        validator: DoubleValidator {
                            bottom: 0.0
                            top: 1.0
                            decimals: 2
                        }

                        textFromValue: function(value) {
                            return (value / 100.0).toFixed(2)
                        }

                        valueFromText: function(text) {
                            return Math.round(parseFloat(text) * 100) || 50
                        }

                        background: Rectangle {
                            color: Theme.bgHover
                            radius: 4
                            border.color: lowConfThresholdSpin.activeFocus ? Theme.accentWarning : Theme.textDisabled
                            border.width: 1
                        }

                        contentItem: Label {
                            text: lowConfThresholdSpin.textFromValue(lowConfThresholdSpin.value)
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            font.family: "monospace"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        up.indicator: Rectangle {
                            x: parent.width - width
                            height: parent.height / 2
                            width: 28
                            color: lowConfThresholdSpin.up.pressed ? Theme.textDisabled : Theme.bgHover
                            border.color: Theme.textDisabled
                            border.width: 1

                            Label {
                                anchors.centerIn: parent
                                text: "+"
                                color: Theme.textPrimary
                                font.pixelSize: 14
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        down.indicator: Rectangle {
                            x: parent.width - width
                            y: parent.height / 2
                            height: parent.height / 2
                            width: 28
                            color: lowConfThresholdSpin.down.pressed ? Theme.textDisabled : Theme.bgHover
                            border.color: Theme.textDisabled
                            border.width: 1

                            Label {
                                anchors.centerIn: parent
                                text: "-"
                                color: Theme.textPrimary
                                font.pixelSize: 14
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }

                    Button {
                        id: collectLowConfBtn
                        text: "收集低置信度"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        enabled: selectedBatchId !== ""

                        background: Rectangle {
                            color: parent.enabled ? (parent.pressed ? Theme.accentWarning : Theme.accentWarning) : Theme.textDisabled
                            radius: 6
                        }

                        contentItem: Label {
                            text: parent.text
                            color: parent.enabled ? Theme.bgPrimary : Theme.textDisabled
                            font.pixelSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: collectLowConfidence()
                    }
                }

                // 低置信度计数摘要
                Label {
                    id: lowConfSummary
                    Layout.fillWidth: true
                    visible: confidenceStats.totalCandidates > 0
                    text: "低置信度:" + confidenceStats.lowConfCount + " / " + confidenceStats.totalCandidates +
                          "  |  平均置信度: " + (parseFloat(confidenceStats.averageConfidence) || 0).toFixed(3) +
                          "  |  阈值: " + (parseFloat(confidenceStats.threshold) || 0).toFixed(2)
                    color: confidenceStats.lowConfCount > 0 ? Theme.accentWarning : Theme.accentSuccess
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }

                // 低置信度样本ID列表（截断显示）
                Label {
                    id: lowConfIdList
                    Layout.fillWidth: true
                    visible: lowConfListModel.count > 0
                    text: {
                        var ids = []
                        var maxShow = Math.min(lowConfListModel.count, 5)
                        for (var i = 0; i < maxShow; i++) {
                            var idx = lowConfListModel.get(i).candidateIndex
                            ids.push("#" + idx)
                        }
                        var suffix = lowConfListModel.count > 5 ? " ..." : ""
                        return "候选: " + ids.join(", ") + suffix
                    }
                    color: Theme.textDisabled
                    font.pixelSize: 10
                    font.family: "monospace"
                    wrapMode: Text.WordWrap
                }

                // 分隔线
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.bgHover
                }

                // 难例审核区域
                Label {
                    text: "难例审核"
                    color: Theme.accentSecondary
                    font.pixelSize: 14
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        id: hardCaseQueueBtn
                        text: "难例队列"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        enabled: selectedBatchId !== ""

                        background: Rectangle {
                            color: parent.enabled ? (parent.pressed ? Theme.accentSecondary : Theme.accentSecondary) : Theme.textDisabled
                            radius: 6
                        }

                        contentItem: Label {
                            text: parent.text + (hardCaseQueue.length > 0 ? " (" + hardCaseQueue.length + ")" : "")
                            color: parent.enabled ? Theme.bgPrimary : Theme.textDisabled
                            font.pixelSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            loadHardCaseQueue()
                            showHardCaseQueue = !showHardCaseQueue
                        }
                    }
                }

                // 漏检警告横幅
                Rectangle {
                    Layout.fillWidth: true
                    height: 28
                    radius: 4
                    visible: {
                        if (hardCaseQueue.length === 0) return false
                        for (var i = 0; i < hardCaseQueue.length; i++) {
                            if (hardCaseQueue[i].reason === "false_negative") return true
                        }
                        return false
                    }
                    color: Theme.accentWarning
                    border.color: Theme.accentWarning
                    border.width: 1

                    Label {
                        anchors.centerIn: parent
                        text: "! 检测到漏检 - 建议立即审核"
                        color: Theme.accentWarning
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                // 难例队列加载器
                Loader {
                    id: hardCaseQueueLoader
                    Layout.fillWidth: true
                    Layout.preferredHeight: showHardCaseQueue ? 280 : 0
                    visible: showHardCaseQueue
                    active: showHardCaseQueue

                    source: "HardCaseQueue.qml"
                    onLoaded: {
                        if (item) {
                            item.batchId = Qt.binding(function() { return selectedBatchId })
                            item.datasetId = Qt.binding(function() { return currentDatasetId })
                            item.lowConfThreshold = Qt.binding(function() { return lowConfThresholdSpin.realValue })
                        }
                    }
                }

                // 分隔线
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.bgHover
                }

                // 批次列表标题
                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: "批次"
                        color: Theme.accentPrimary
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "刷新"
                        flat: true
                        palette.buttonText: Theme.accentPrimary
                        font.pixelSize: 12
                        onClicked: refreshBatches()
                    }
                }

                // 批次列表
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
                        text: "暂无推理批次"
                        color: Theme.textDisabled
                        font.pixelSize: 14
                    }

                    delegate: Rectangle {
                        width: batchList.width
                        height: 48
                        radius: 6
                        color: batchMouseArea.containsMouse ? Theme.bgHover : Theme.bgSecondary
                        border.color: model.id === selectedBatchId ? Theme.accentPrimary : "transparent"
                        border.width: model.id === selectedBatchId ? 1 : 0

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            // 状态圆点
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: {
                                    switch (model.status) {
                                    case "completed": return Theme.accentSuccess
                                    case "running": return Theme.accentWarning
                                    case "cancelled": return Theme.textDisabled
                                    case "failed": return Theme.accentError
                                    default: return Theme.accentPrimary
                                    }
                                }
                            }

                            Label {
                                text: model.id.substring(0, 8) + "..."
                                color: Theme.accentPrimary
                                font.pixelSize: 12
                                font.family: "monospace"
                            }

                            Label {
                                text: model.status
                                color: Theme.textMuted
                                font.pixelSize: 11
                            }

                            Label {
                                text: "置信度:" + (parseFloat(model.confThreshold) || 0).toFixed(2)
                                color: Theme.textDisabled
                                font.pixelSize: 11
                            }

                            Item { Layout.fillWidth: true }

                            // 取消按钮（运行中/待处理批次）
                            Button {
                                text: "取消"
                                flat: true
                                visible: model.status === "pending" || model.status === "running"
                                palette.buttonText: Theme.accentError
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

        // 右侧面板：候选结果审核
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgInput
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // 带统计信息的标题
                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: selectedBatchId !== "" ?
                            "候选结果 - " + selectedBatchId.substring(0, 8) + "..." :
                            "候选结果"
                        color: Theme.accentPrimary
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        visible: selectedBatchId !== ""
                        text: "总计:" + batchStats.total +
                              " | 已确认: " + batchStats.confirmed +
                              " | 已拒绝: " + batchStats.rejected +
                              " | 待处理: " + batchStats.pending
                        color: Theme.textMuted
                        font.pixelSize: 12
                    }
                }

                // 批次操作按钮
                RowLayout {
                    Layout.fillWidth: true
                    visible: selectedBatchId !== ""
                    spacing: 8

                    Label {
                        text: "批次确认阈值："
                        color: Theme.textPrimary
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
                            color: Theme.bgHover
                            Rectangle {
                                width: batchConfThresholdSlider.visualPosition * parent.width
                                height: parent.height
                                color: Theme.accentSuccess
                                radius: 2
                            }
                        }

                        handle: Rectangle {
                            x: batchConfThresholdSlider.leftPadding + batchConfThresholdSlider.visualPosition * (batchConfThresholdSlider.availableWidth - width)
                            y: batchConfThresholdSlider.topPadding + batchConfThresholdSlider.availableHeight / 2 - height / 2
                            width: 14
                            height: 14
                            radius: 7
                            color: batchConfThresholdSlider.pressed ? Theme.accentSuccess : Theme.accentSuccess
                        }
                    }

                    Label {
                        text: batchConfThresholdSlider.value.toFixed(2)
                        color: Theme.textMuted
                        font.pixelSize: 11
                        font.family: "monospace"
                        Layout.preferredWidth: 32
                    }

                    Button {
                        text: "确认以上全部"
                        Layout.preferredHeight: 28

                        background: Rectangle {
                            color: parent.pressed ? Qt.darker(Theme.accentSuccess, 1.2) : Theme.accentSuccess
                            radius: 4
                        }

                        contentItem: Label {
                            text: parent.text
                            color: Theme.bgPrimary
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
                        color: Theme.bgHover
                    }

                    Button {
                        text: "拒绝以下全部"
                        Layout.preferredHeight: 28

                        background: Rectangle {
                            color: parent.pressed ? Qt.darker(Theme.accentError, 1.2) : Theme.accentError
                            radius: 4
                        }

                        contentItem: Label {
                            text: parent.text
                            color: Theme.bgPrimary
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

                // 候选结果列表
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
                        text: "该批次无候选结果"
                        color: Theme.textDisabled
                        font.pixelSize: 14
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: selectedBatchId === ""
                        text: "选择批次查看候选结果"
                        color: Theme.textDisabled
                        font.pixelSize: 14
                    }

                    delegate: Rectangle {
                        width: candidateList.width
                        height: 40
                        radius: 4
                        color: {
                            switch (model.state) {
                            case "confirmed": return Theme.accentSuccess
                            case "rejected": return Theme.accentError
                            case "edited": return Theme.accentWarning
                            default: return Theme.bgSecondary
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            // 状态指示器
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: {
                                    switch (model.state) {
                                    case "confirmed": return Theme.accentSuccess
                                    case "rejected": return Theme.accentError
                                    case "edited": return Theme.accentWarning
                                    default: return Theme.accentPrimary
                                    }
                                }
                            }

                            Label {
                                text: model.className || ("Class " + model.classIndex)
                                color: Theme.textPrimary
                                font.pixelSize: 12
                                Layout.preferredWidth: 100
                            }

                            Label {
                                text: "置信度: " + (parseFloat(model.confidence) || 0).toFixed(3)
                                color: (parseFloat(model.confidence) || 0) >= 0.5 ? Theme.accentSuccess : Theme.accentWarning
                                font.pixelSize: 11
                                font.family: "monospace"
                            }

                            Label {
                                text: "[" + (parseFloat(model.cx) || 0).toFixed(2) + ", " +
                                      (parseFloat(model.cy) || 0).toFixed(2) + ", " +
                                      (parseFloat(model.w) || 0).toFixed(2) + ", " +
                                      (parseFloat(model.h) || 0).toFixed(2) + "]"
                                color: Theme.textDisabled
                                font.pixelSize: 10
                                font.family: "monospace"
                            }

                            Item { Layout.fillWidth: true }

                            // 待处理候选的确认/拒绝按钮
                            Button {
                                text: "确认"
                                visible: model.state === "pending"
                                Layout.preferredHeight: 24
                                Layout.preferredWidth: 60

                                background: Rectangle {
                                    color: parent.pressed ? Qt.darker(Theme.accentSuccess, 1.2) : Theme.accentSuccess
                                    radius: 3
                                }

                                contentItem: Label {
                                    text: parent.text
                                    color: Theme.bgPrimary
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
                                text: "拒绝"
                                visible: model.state === "pending"
                                Layout.preferredHeight: 24
                                Layout.preferredWidth: 52

                                background: Rectangle {
                                    color: parent.pressed ? Qt.darker(Theme.accentError, 1.2) : Theme.accentError
                                    radius: 3
                                }

                                contentItem: Label {
                                    text: parent.text
                                    color: Theme.bgPrimary
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

                            // 非待处理状态的状态徽章
                            Rectangle {
                                visible: model.state !== "pending"
                                height: 20
                                width: stateText.implicitWidth + 12
                                radius: 3
                                color: {
                                    switch (model.state) {
                                    case "confirmed": return Theme.accentSuccess
                                    case "rejected": return Theme.accentError
                                    case "edited": return Theme.accentWarning
                                    default: return Theme.textDisabled
                                    }
                                }
                                border.color: {
                                    switch (model.state) {
                                    case "confirmed": return Theme.accentSuccess
                                    case "rejected": return Theme.accentError
                                    case "edited": return Theme.accentWarning
                                    default: return Theme.textDisabled
                                    }
                                }
                                border.width: 1

                                Label {
                                    id: stateText
                                    anchors.centerIn: parent
                                    text: model.state
                                    color: {
                                        switch (model.state) {
                                        case "confirmed": return Theme.accentSuccess
                                        case "rejected": return Theme.accentError
                                        case "edited": return Theme.accentWarning
                                        default: return Theme.textDisabled
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
