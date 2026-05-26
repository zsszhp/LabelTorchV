// TrainingPage.qml - 训练工作台 V2
// 支持实时双图表（Loss + 精度指标）、可折叠参数面板、适配器自适应配置
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme
import QtQuick.Layouts

Item {
    id: root

    property string currentProjectId: ""
    property string currentRunId: ""
    property string currentRunStatus: ""

    // 实时指标数据模型
    ListModel {
        id: lossModel
    }

    ListModel {
        id: metricModel
    }

    // 部署阈值：anomalib 用 AUROC > 0.95，ultralytics 用 mAP50 > 0.85
    property real deploymentThreshold: adapterCombo.currentText === "anomalib" ? 0.95 : 0.85

    // 当前精度指标名称，随适配器切换
    property string metricName: adapterCombo.currentText === "anomalib" ? "AUROC" : "mAP50"

    // 可折叠面板展开状态
    property bool basicParamsExpanded: true
    property bool advancedParamsExpanded: false
    property bool adapterParamsExpanded: true

    onCurrentProjectIdChanged: {
        trainingModel.setProjectId(currentProjectId)
        snapshotModel.setDatasetId("")
        runHistoryList.currentIndex = -1
        currentRunId = ""
        currentRunStatus = ""
        logView.clear()
        lossModel.clear()
        metricModel.clear()
    }

    // 根据任务类型自动选择模型系列与适配器
    function applyTaskTypeToModelFamily(taskType) {
        switch (taskType) {
            case "detect":
                modelFamilyCombo.currentIndex = modelFamilyCombo.indexOfValue("yolov8")
                adapterCombo.currentIndex = adapterCombo.indexOfValue("ultralytics")
                break
            case "obb":
                modelFamilyCombo.currentIndex = modelFamilyCombo.indexOfValue("yolov8_obb")
                adapterCombo.currentIndex = adapterCombo.indexOfValue("ultralytics")
                break
            case "classify":
                modelFamilyCombo.currentIndex = modelFamilyCombo.indexOfValue("yolov8_cls")
                adapterCombo.currentIndex = adapterCombo.indexOfValue("ultralytics")
                break
            case "anomaly":
                modelFamilyCombo.currentIndex = modelFamilyCombo.indexOfValue("anomaly")
                adapterCombo.currentIndex = adapterCombo.indexOfValue("anomalib")
                break
        }
    }

    // 收集全部训练配置并序列化为 JSON
    function getConfigJson() {
        var config = {
            "adapter": adapterCombo.currentText,
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
        if (adapterCombo.currentText === "ultralytics") {
            config["optimizer"] = optimizerCombo.currentText
            config["lr0"] = lrSpin.value
            config["weight_decay"] = wdSpin.value
        }
        if (adapterCombo.currentText === "anomalib") {
            config["backbone"] = backboneCombo.currentText
            config["anomaly_score_threshold"] = thresholdSpin.value
        }
        return JSON.stringify(config)
    }

    // 同步全局任务类型变更
    Connections {
        target: ApplicationWindow.window
        function onCurrentTaskTypeChanged() {
            root.applyTaskTypeToModelFamily(ApplicationWindow.window.currentTaskType)
        }
    }

    // 初始化时设置模型系列
    Component.onCompleted: {
        if (appController.projectOpen) {
            root.applyTaskTypeToModelFamily(projectService.getTaskType(appController.currentProjectId))
        }
    }

    // 监听训练进度信号，实时更新图表数据
    Connections {
        target: trainingService

        function onTrainingProgress(runId, epoch, totalEpochs, loss, metrics) {
            if (runId !== currentRunId) return

            // 追加 Loss 数据点
            lossModel.append({"epoch": epoch, "value": loss})

            // 根据适配器提取精度指标
            var metricValue = 0
            if (adapterCombo.currentText === "anomalib") {
                metricValue = metrics["auroc"] || metrics["image_auroc"] || metrics["pixel_auroc"] || 0
            } else {
                metricValue = metrics["mAP50"] || metrics["map50"] || metrics["metrics/mAP50(B)"] || 0
            }
            metricModel.append({"epoch": epoch, "value": metricValue})

            // 更新进度标签
            progressLabel.text = "Epoch " + epoch + "/" + totalEpochs +
                " | Loss: " + loss.toFixed(4) +
                " | " + metricName + ": " + metricValue.toFixed(4)

            // 触发图表重绘
            lossChart.requestPaint()
            metricChart.requestPaint()
        }

        function onRunStatusChanged(runId, status) {
            if (runId !== currentRunId) return
            currentRunStatus = status
            if (status === "succeeded") {
                statusLabel.text = "训练完成:" + runId.substring(0, 8) + "..."
                statusLabel.color = Theme.accentSuccess
            } else if (status === "failed") {
                statusLabel.text = "训练失败"
                statusLabel.color = Theme.accentError
            } else if (status === "cancelled" || status === "stopped") {
                statusLabel.text = "训练已停止"
                statusLabel.color = Theme.accentWarning
            }
            trainingModel.refresh()
        }
    }

    // 通用图表绘制函数
    function drawChart(ctx, w, h, model, yLabel, lineColor, threshVal, showThresh) {
        var padL = 56, padR = 16, padT = 8, padB = 32
        var cW = w - padL - padR
        var cH = h - padT - padB

        ctx.clearRect(0, 0, w, h)

        // 图表背景
        ctx.fillStyle = Theme.bgTertiary
        ctx.fillRect(0, 0, w, h)

        // 空数据占位提示
        if (model.count === 0) {
            ctx.fillStyle = Theme.textMuted
            ctx.font = "13px sans-serif"
            ctx.textAlign = "center"
            ctx.fillText("等待训练数据...", w / 2, h / 2)
            return
        }

        // 计算数据范围
        var xMin = 0
        var xMax = Math.max(model.get(model.count - 1).epoch, 1)
        var yMin = Infinity, yMax = -Infinity
        for (var i = 0; i < model.count; i++) {
            var v = model.get(i).value
            if (v < yMin) yMin = v
            if (v > yMax) yMax = v
        }
        var yRange = yMax - yMin
        if (yRange < 0.001) { yMin -= 0.05; yMax += 0.05; yRange = yMax - yMin }
        yMin -= yRange * 0.1
        yMax += yRange * 0.1
        if (yMin < 0 && model === lossModel) yMin = 0
        if (model === metricModel) { if (yMin < 0) yMin = 0; if (yMax > 1.05) yMax = 1.05 }

        // 坐标映射
        function toX(ep) { return padL + (ep - xMin) / Math.max(xMax - xMin, 1) * cW }
        function toY(val) { return padT + cH - (val - yMin) / Math.max(yMax - yMin, 0.001) * cH }

        // 水平网格线 + Y 轴刻度
        var ySteps = 5
        ctx.strokeStyle = Theme.divider
        ctx.lineWidth = 0.5
        for (var s = 0; s <= ySteps; s++) {
            var yVal = yMin + (yMax - yMin) * s / ySteps
            var yPx = toY(yVal)
            ctx.beginPath()
            ctx.moveTo(padL, yPx)
            ctx.lineTo(w - padR, yPx)
            ctx.stroke()
            ctx.fillStyle = Theme.textMuted
            ctx.font = "10px sans-serif"
            ctx.textAlign = "right"
            ctx.fillText(yVal.toFixed(2), padL - 6, yPx + 3)
        }

        // 垂直网格线 + X 轴刻度
        var xSteps = Math.min(model.count, 10)
        for (var s = 0; s <= xSteps; s++) {
            var xVal = xMin + (xMax - xMin) * s / xSteps
            var xPx = toX(xVal)
            ctx.beginPath()
            ctx.moveTo(xPx, padT)
            ctx.lineTo(xPx, padT + cH)
            ctx.stroke()
            ctx.fillStyle = Theme.textMuted
            ctx.font = "10px sans-serif"
            ctx.textAlign = "center"
            ctx.fillText(Math.round(xVal).toString(), xPx, padT + cH + 16)
        }

        // 坐标轴
        ctx.strokeStyle = Theme.borderNormal
        ctx.lineWidth = 1
        ctx.beginPath()
        ctx.moveTo(padL, padT)
        ctx.lineTo(padL, padT + cH)
        ctx.lineTo(w - padR, padT + cH)
        ctx.stroke()

        // 部署阈值虚线
        if (showThresh && threshVal >= yMin && threshVal <= yMax) {
            var thY = toY(threshVal)
            ctx.strokeStyle = Theme.accentWarning
            ctx.lineWidth = 1
            ctx.setLineDash([6, 4])
            ctx.beginPath()
            ctx.moveTo(padL, thY)
            ctx.lineTo(w - padR, thY)
            ctx.stroke()
            ctx.setLineDash([])
            ctx.fillStyle = Theme.accentWarning
            ctx.font = "bold 10px sans-serif"
            ctx.textAlign = "right"
            ctx.fillText("部署阈值 " + threshVal.toFixed(2), w - padR - 4, thY - 4)
        }

        // 数据折线
        ctx.strokeStyle = lineColor
        ctx.lineWidth = 2
        ctx.lineJoin = "round"
        ctx.beginPath()
        for (var i = 0; i < model.count; i++) {
            var px = toX(model.get(i).epoch)
            var py = toY(model.get(i).value)
            if (i === 0) ctx.moveTo(px, py)
            else ctx.lineTo(px, py)
        }
        ctx.stroke()

        // 数据点圆点
        for (var i = 0; i < model.count; i++) {
            var px = toX(model.get(i).epoch)
            var py = toY(model.get(i).value)
            ctx.fillStyle = lineColor
            ctx.beginPath()
            ctx.arc(px, py, 2.5, 0, Math.PI * 2)
            ctx.fill()
        }

        // 最新数据点高亮 + 数值标注
        if (model.count > 0) {
            var li = model.count - 1
            var lpx = toX(model.get(li).epoch)
            var lpy = toY(model.get(li).value)
            // 外圈光晕
            ctx.fillStyle = lineColor
            ctx.globalAlpha = 0.3
            ctx.beginPath()
            ctx.arc(lpx, lpy, 7, 0, Math.PI * 2)
            ctx.fill()
            ctx.globalAlpha = 1.0
            ctx.beginPath()
            ctx.arc(lpx, lpy, 4, 0, Math.PI * 2)
            ctx.fill()
            // 数值文本
            ctx.fillStyle = Theme.textPrimary
            ctx.font = "bold 11px sans-serif"
            ctx.textAlign = "left"
            ctx.fillText(model.get(li).value.toFixed(4), lpx + 8, lpy - 6)
        }

        // Y 轴标签（旋转）
        ctx.fillStyle = Theme.textSecondary
        ctx.font = "11px sans-serif"
        ctx.textAlign = "center"
        ctx.save()
        ctx.translate(14, padT + cH / 2)
        ctx.rotate(-Math.PI / 2)
        ctx.fillText(yLabel, 0, 0)
        ctx.restore()

        // X 轴标签
        ctx.fillStyle = Theme.textSecondary
        ctx.font = "11px sans-serif"
        ctx.textAlign = "center"
        ctx.fillText("Epoch", padL + cW / 2, h - 2)
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // === 左面板：配置 + 控制 ===
        Rectangle {
            Layout.preferredWidth: 380
            Layout.fillHeight: true
            color: Theme.bgCard
            radius: Theme.radiusNormal

            ScrollView {
                id: leftScroll
                anchors.fill: parent
                anchors.margins: 12
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: leftScroll.width - 24
                    spacing: 6

                    // 区域标题
                    Label {
                        text: "新建训练任务"
                        color: Theme.accentPrimary
                        font.pixelSize: Theme.fontSizeSubheading
                        font.bold: true
                    }

                    // 数据快照选择器
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: "数据快照:"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeNormal
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
                                font.pixelSize: Theme.fontSizeNormal
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 8
                            }

                            background: Rectangle {
                                color: Theme.bgInput
                                radius: Theme.radiusSmall
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
                                    radius: Theme.radiusSmall
                                }
                            }

                            delegate: ItemDelegate {
                                width: snapshotCombo.width
                                contentItem: Label {
                                    text: model.snapshotId.substring(0, 8) + "... (" + model.sampleCount + " 样本, train:" + model.trainCount + " val:" + model.valCount + ")"
                                    color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.family: "monospace"
                                    verticalAlignment: Text.AlignVCenter
                                }
                                highlighted: snapshotCombo.highlightedIndex === index
                                background: Rectangle {
                                    color: highlighted ? Theme.bgInput : Theme.bgPrimary
                                }
                            }
                        }
                    }

                    // 快照信息
                    Label {
                        id: snapshotInfoLabel
                        Layout.fillWidth: true
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeCaption
                        wrapMode: Text.WordWrap
                        visible: text !== ""

                        text: {
                            if (snapshotCombo.currentIndex < 0) return ""
                            var idx = snapshotCombo.currentIndex
                            var count = snapshotModel.data(snapshotModel.index(idx, 0), 259)
                            var val = snapshotModel.data(snapshotModel.index(idx, 0), 260)
                            var tax = snapshotModel.data(snapshotModel.index(idx, 0), 261)
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
                            font.pixelSize: Theme.fontSizeNormal
                            Layout.preferredWidth: 72
                        }

                        ComboBox {
                            id: adapterCombo
                            Layout.fillWidth: true
                            model: trainingService.listAdapters()
                            valueRole: "value"

                            contentItem: Label {
                                text: adapterCombo.displayText
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeNormal
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 8
                            }

                            background: Rectangle {
                                color: Theme.bgInput
                                radius: Theme.radiusSmall
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
                                    radius: Theme.radiusSmall
                                }
                            }

                            delegate: ItemDelegate {
                                width: adapterCombo.width
                                contentItem: Label {
                                    text: modelData
                                    color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
                                    verticalAlignment: Text.AlignVCenter
                                }
                                highlighted: adapterCombo.highlightedIndex === index
                                background: Rectangle {
                                    color: highlighted ? Theme.bgInput : Theme.bgPrimary
                                }
                            }

                            onActivated: {
                                // 切换适配器时自动更新模型系列选项
                                if (currentText === "anomalib") {
                                    modelFamilyCombo.model = ["anomaly"]
                                    modelFamilyCombo.currentIndex = 0
                                } else {
                                    modelFamilyCombo.model = ["yolov5", "yolov8", "yolov8_obb", "yolov8_cls", "yolov10", "yolov11"]
                                    modelFamilyCombo.currentIndex = 1
                                }
                            }
                        }
                    }

                    // === 可折叠面板：基础参数 ===
                    Column {
                        id: basicSection
                        property bool expanded: root.basicParamsExpanded
                        Layout.fillWidth: true

                        // 面板头部
                        Rectangle {
                            width: basicSection.width
                            height: 28
                            color: Theme.bgTertiary
                            radius: Theme.radiusSmall

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8

                                Label {
                                    text: basicSection.expanded ? "▼" : "▶"
                                    color: Theme.accentPrimary
                                    font.pixelSize: 10
                                }

                                Label {
                                    text: "基础参数"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
                                    font.bold: true
                                }

                                Item { Layout.fillWidth: true }

                                Label {
                                    text: basicSection.expanded ? "收起" : "展开"
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.basicParamsExpanded = !root.basicParamsExpanded
                            }
                        }

                        // 面板内容
                        ColumnLayout {
                            visible: basicSection.expanded
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            spacing: 6

                            // 模型系列
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: "模型:"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
                                    Layout.preferredWidth: 80
                                }

                                ComboBox {
                                    id: modelFamilyCombo
                                    model: ["yolov5", "yolov8", "yolov8_obb", "yolov8_cls", "yolov10", "yolov11"]
                                    currentIndex: 1
                                    Layout.fillWidth: true

                                    function indexOfValue(val) {
                                        for (var i = 0; i < count; i++) {
                                            if (model.get(i) === val) return i
                                        }
                                        return -1
                                    }

                                    contentItem: Label {
                                        text: modelFamilyCombo.displayText
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontSizeNormal
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 8
                                    }

                                    background: Rectangle {
                                        color: Theme.bgInput
                                        radius: Theme.radiusSmall
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
                                            radius: Theme.radiusSmall
                                        }
                                    }

                                    delegate: ItemDelegate {
                                        width: modelFamilyCombo.width
                                        contentItem: Label {
                                            text: modelData
                                            color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                            font.pixelSize: Theme.fontSizeNormal
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        highlighted: modelFamilyCombo.highlightedIndex === index
                                        background: Rectangle {
                                            color: highlighted ? Theme.bgInput : Theme.bgPrimary
                                        }
                                    }
                                }
                            }

                            // 训练类型
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: "类型:"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
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
                                        font.pixelSize: Theme.fontSizeNormal
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 8
                                    }

                                    background: Rectangle {
                                        color: Theme.bgInput
                                        radius: Theme.radiusSmall
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
                                            radius: Theme.radiusSmall
                                        }
                                    }

                                    delegate: ItemDelegate {
                                        width: trainingTypeCombo.width
                                        contentItem: Label {
                                            text: modelData
                                            color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                            font.pixelSize: Theme.fontSizeNormal
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        highlighted: trainingTypeCombo.highlightedIndex === index
                                        background: Rectangle {
                                            color: highlighted ? Theme.bgInput : Theme.bgPrimary
                                        }
                                    }
                                }
                            }

                            // 父模型版本（增量训练时可见）
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                visible: trainingTypeCombo.currentIndex === 2

                                Label {
                                    text: "父版本:"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
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
                                        font.pixelSize: Theme.fontSizeNormal
                                        font.family: "monospace"
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 8
                                    }

                                    background: Rectangle {
                                        color: Theme.bgInput
                                        radius: Theme.radiusSmall
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
                                            radius: Theme.radiusSmall
                                        }
                                    }

                                    delegate: ItemDelegate {
                                        width: parentVersionCombo.width
                                        contentItem: Label {
                                            text: model.versionId.substring(0, 8) + "... (" + model.bestWeight + ")"
                                            color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                            font.pixelSize: Theme.fontSizeCaption
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

                            // 图片尺寸
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: "图片尺寸:"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
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

                                    contentItem: Label {
                                        text: imgSizeSpin.textFromValue(imgSizeSpin.value, imgSizeSpin.locale)
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontSizeNormal
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
                                        Label { anchors.centerIn: parent; text: "+"; color: Theme.textPrimary; font.pixelSize: 14 }
                                    }

                                    down.indicator: Rectangle {
                                        x: imgSizeSpin.mirrored ? parent.width - width : 0
                                        height: parent.height
                                        implicitWidth: 32
                                        color: imgSizeSpin.down.pressed ? Theme.borderNormal : Theme.bgInput
                                        border.color: Theme.borderNormal
                                        radius: 2
                                        Label { anchors.centerIn: parent; text: "-"; color: Theme.textPrimary; font.pixelSize: 14 }
                                    }

                                    background: Rectangle {
                                        color: Theme.bgInput
                                        border.color: imgSizeSpin.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                                        radius: Theme.radiusSmall
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
                                    font.pixelSize: Theme.fontSizeNormal
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
                                        font.pixelSize: Theme.fontSizeNormal
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
                                        radius: Theme.radiusSmall
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
                                    font.pixelSize: Theme.fontSizeNormal
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
                                        font.pixelSize: Theme.fontSizeNormal
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
                                        radius: Theme.radiusSmall
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
                                    font.pixelSize: Theme.fontSizeNormal
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
                                        font.pixelSize: Theme.fontSizeNormal
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
                                        radius: Theme.radiusSmall
                                    }
                                }
                            }
                        }
                    }

                    // === 可折叠面板：高级参数 ===
                    Column {
                        id: advancedSection
                        property bool expanded: root.advancedParamsExpanded
                        Layout.fillWidth: true

                        Rectangle {
                            width: advancedSection.width
                            height: 28
                            color: Theme.bgTertiary
                            radius: Theme.radiusSmall

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8

                                Label {
                                    text: advancedSection.expanded ? "▼" : "▶"
                                    color: Theme.accentSecondary
                                    font.pixelSize: 10
                                }

                                Label {
                                    text: "高级参数"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
                                    font.bold: true
                                }

                                Item { Layout.fillWidth: true }

                                Label {
                                    text: advancedSection.expanded ? "收起" : "展开"
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.advancedParamsExpanded = !root.advancedParamsExpanded
                            }
                        }

                        ColumnLayout {
                            visible: advancedSection.expanded
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            spacing: 6

                            // 工作线程
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: "工作线程:"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
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
                                        font.pixelSize: Theme.fontSizeNormal
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
                                        radius: Theme.radiusSmall
                                    }
                                }
                            }

                            // 混合精度
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: "混合精度:"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
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

                            // 继续训练
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: "继续训练:"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
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

                            // 设备
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: "设备:"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
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
                                        font.pixelSize: Theme.fontSizeNormal
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 8
                                    }

                                    background: Rectangle {
                                        color: Theme.bgInput
                                        radius: Theme.radiusSmall
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
                                            radius: Theme.radiusSmall
                                        }
                                    }

                                    delegate: ItemDelegate {
                                        width: deviceCombo.width
                                        contentItem: Label {
                                            text: modelData
                                            color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                            font.pixelSize: Theme.fontSizeNormal
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

                    // === 可折叠面板：Ultralytics 专属参数 ===
                    Column {
                        id: ultralyticsSection
                        property bool expanded: root.adapterParamsExpanded
                        visible: adapterCombo.currentText === "ultralytics"
                        Layout.fillWidth: true

                        Rectangle {
                            width: ultralyticsSection.width
                            height: 28
                            color: Theme.bgTertiary
                            radius: Theme.radiusSmall

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8

                                Label {
                                    text: ultralyticsSection.expanded ? "▼" : "▶"
                                    color: Theme.accentSuccess
                                    font.pixelSize: 10
                                }

                                Label {
                                    text: "Ultralytics 专属"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
                                    font.bold: true
                                }

                                Item { Layout.fillWidth: true }

                                Label {
                                    text: ultralyticsSection.expanded ? "收起" : "展开"
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.adapterParamsExpanded = !root.adapterParamsExpanded
                            }
                        }

                        ColumnLayout {
                            visible: ultralyticsSection.expanded && ultralyticsSection.visible
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            spacing: 6

                            // 优化器
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: "优化器:"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
                                    Layout.preferredWidth: 80
                                }

                                ComboBox {
                                    id: optimizerCombo
                                    model: ["SGD", "Adam", "AdamW", "NAdam", "RAdam"]
                                    currentIndex: 2
                                    Layout.fillWidth: true

                                    contentItem: Label {
                                        text: optimizerCombo.displayText
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontSizeNormal
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 8
                                    }

                                    background: Rectangle {
                                        color: Theme.bgInput
                                        radius: Theme.radiusSmall
                                        border.color: optimizerCombo.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                                        border.width: 1
                                    }

                                    popup: Popup {
                                        y: optimizerCombo.height
                                        width: optimizerCombo.width
                                        implicitHeight: contentItem.implicitHeight
                                        padding: 1

                                        contentItem: ListView {
                                            clip: true
                                            implicitHeight: contentHeight
                                            model: optimizerCombo.popup.visible ? optimizerCombo.delegateModel : null
                                            currentIndex: optimizerCombo.highlightedIndex
                                        }

                                        background: Rectangle {
                                            color: Theme.bgPrimary
                                            border.color: Theme.borderNormal
                                            radius: Theme.radiusSmall
                                        }
                                    }

                                    delegate: ItemDelegate {
                                        width: optimizerCombo.width
                                        contentItem: Label {
                                            text: modelData
                                            color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                            font.pixelSize: Theme.fontSizeNormal
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        highlighted: optimizerCombo.highlightedIndex === index
                                        background: Rectangle {
                                            color: highlighted ? Theme.bgInput : Theme.bgPrimary
                                        }
                                    }
                                }
                            }

                            // 学习率
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: "学习率:"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
                                    Layout.preferredWidth: 80
                                }

                                SpinBox {
                                    id: lrSpin
                                    from: 1
                                    to: 1000
                                    value: 10
                                    stepSize: 1
                                    editable: true
                                    Layout.fillWidth: true

                                    // 显示为 0.00x 格式，实际值 = value / 10000
                                    textFromValue: function(value) { return (value / 10000).toFixed(4) }
                                    valueFromText: function(text) { return Math.round(parseFloat(text) * 10000) || 10 }

                                    contentItem: Label {
                                        text: lrSpin.textFromValue(lrSpin.value, lrSpin.locale)
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontSizeNormal
                                        font.family: "monospace"
                                        verticalAlignment: Text.AlignVCenter
                                        horizontalAlignment: Text.AlignHCenter
                                    }

                                    up.indicator: Rectangle {
                                        x: lrSpin.mirrored ? 0 : parent.width - width
                                        height: parent.height
                                        implicitWidth: 32
                                        color: lrSpin.up.pressed ? Theme.borderNormal : Theme.bgInput
                                        border.color: Theme.borderNormal
                                        radius: 2
                                        Label { anchors.centerIn: parent; text: "+"; color: Theme.textPrimary; font.pixelSize: 14 }
                                    }

                                    down.indicator: Rectangle {
                                        x: lrSpin.mirrored ? parent.width - width : 0
                                        height: parent.height
                                        implicitWidth: 32
                                        color: lrSpin.down.pressed ? Theme.borderNormal : Theme.bgInput
                                        border.color: Theme.borderNormal
                                        radius: 2
                                        Label { anchors.centerIn: parent; text: "-"; color: Theme.textPrimary; font.pixelSize: 14 }
                                    }

                                    background: Rectangle {
                                        color: Theme.bgInput
                                        border.color: lrSpin.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                                        radius: Theme.radiusSmall
                                    }
                                }
                            }

                            // 权重衰减
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: "权重衰减:"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
                                    Layout.preferredWidth: 80
                                }

                                SpinBox {
                                    id: wdSpin
                                    from: 0
                                    to: 1000
                                    value: 5
                                    stepSize: 1
                                    editable: true
                                    Layout.fillWidth: true

                                    // 显示为 0.000x 格式，实际值 = value / 10000
                                    textFromValue: function(value) { return (value / 10000).toFixed(4) }
                                    valueFromText: function(text) { return Math.round(parseFloat(text) * 10000) || 0 }

                                    contentItem: Label {
                                        text: wdSpin.textFromValue(wdSpin.value, wdSpin.locale)
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontSizeNormal
                                        font.family: "monospace"
                                        verticalAlignment: Text.AlignVCenter
                                        horizontalAlignment: Text.AlignHCenter
                                    }

                                    up.indicator: Rectangle {
                                        x: wdSpin.mirrored ? 0 : parent.width - width
                                        height: parent.height
                                        implicitWidth: 32
                                        color: wdSpin.up.pressed ? Theme.borderNormal : Theme.bgInput
                                        border.color: Theme.borderNormal
                                        radius: 2
                                        Label { anchors.centerIn: parent; text: "+"; color: Theme.textPrimary; font.pixelSize: 14 }
                                    }

                                    down.indicator: Rectangle {
                                        x: wdSpin.mirrored ? parent.width - width : 0
                                        height: parent.height
                                        implicitWidth: 32
                                        color: wdSpin.down.pressed ? Theme.borderNormal : Theme.bgInput
                                        border.color: Theme.borderNormal
                                        radius: 2
                                        Label { anchors.centerIn: parent; text: "-"; color: Theme.textPrimary; font.pixelSize: 14 }
                                    }

                                    background: Rectangle {
                                        color: Theme.bgInput
                                        border.color: wdSpin.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                                        radius: Theme.radiusSmall
                                    }
                                }
                            }
                        }
                    }

                    // === 可折叠面板：Anomalib 专属参数 ===
                    Column {
                        id: anomalibSection
                        property bool expanded: root.adapterParamsExpanded
                        visible: adapterCombo.currentText === "anomalib"
                        Layout.fillWidth: true

                        Rectangle {
                            width: anomalibSection.width
                            height: 28
                            color: Theme.bgTertiary
                            radius: Theme.radiusSmall

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8

                                Label {
                                    text: anomalibSection.expanded ? "▼" : "▶"
                                    color: Theme.accentPurple
                                    font.pixelSize: 10
                                }

                                Label {
                                    text: "Anomalib 专属"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
                                    font.bold: true
                                }

                                Item { Layout.fillWidth: true }

                                Label {
                                    text: anomalibSection.expanded ? "收起" : "展开"
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.adapterParamsExpanded = !root.adapterParamsExpanded
                            }
                        }

                        ColumnLayout {
                            visible: anomalibSection.expanded && anomalibSection.visible
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            spacing: 6

                            // 骨干网络
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: "骨干网络:"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
                                    Layout.preferredWidth: 80
                                }

                                ComboBox {
                                    id: backboneCombo
                                    model: ["resnet18", "wide_resnet50_2", "efficientnet_b0", "mobilenet_v2"]
                                    currentIndex: 1
                                    Layout.fillWidth: true

                                    contentItem: Label {
                                        text: backboneCombo.displayText
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontSizeNormal
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 8
                                    }

                                    background: Rectangle {
                                        color: Theme.bgInput
                                        radius: Theme.radiusSmall
                                        border.color: backboneCombo.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                                        border.width: 1
                                    }

                                    popup: Popup {
                                        y: backboneCombo.height
                                        width: backboneCombo.width
                                        implicitHeight: contentItem.implicitHeight
                                        padding: 1

                                        contentItem: ListView {
                                            clip: true
                                            implicitHeight: contentHeight
                                            model: backboneCombo.popup.visible ? backboneCombo.delegateModel : null
                                            currentIndex: backboneCombo.highlightedIndex
                                        }

                                        background: Rectangle {
                                            color: Theme.bgPrimary
                                            border.color: Theme.borderNormal
                                            radius: Theme.radiusSmall
                                        }
                                    }

                                    delegate: ItemDelegate {
                                        width: backboneCombo.width
                                        contentItem: Label {
                                            text: modelData
                                            color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                            font.pixelSize: Theme.fontSizeNormal
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        highlighted: backboneCombo.highlightedIndex === index
                                        background: Rectangle {
                                            color: highlighted ? Theme.bgInput : Theme.bgPrimary
                                        }
                                    }
                                }
                            }

                            // 异常分数阈值
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: "异常阈值:"
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
                                    Layout.preferredWidth: 80
                                }

                                SpinBox {
                                    id: thresholdSpin
                                    from: 0
                                    to: 100
                                    value: 50
                                    stepSize: 1
                                    editable: true
                                    Layout.fillWidth: true

                                    // 显示为 0.xx 格式，实际值 = value / 100
                                    textFromValue: function(value) { return (value / 100).toFixed(2) }
                                    valueFromText: function(text) { return Math.round(parseFloat(text) * 100) || 50 }

                                    contentItem: Label {
                                        text: thresholdSpin.textFromValue(thresholdSpin.value, thresholdSpin.locale)
                                        color: Theme.textPrimary
                                        font.pixelSize: Theme.fontSizeNormal
                                        font.family: "monospace"
                                        verticalAlignment: Text.AlignVCenter
                                        horizontalAlignment: Text.AlignHCenter
                                    }

                                    up.indicator: Rectangle {
                                        x: thresholdSpin.mirrored ? 0 : parent.width - width
                                        height: parent.height
                                        implicitWidth: 32
                                        color: thresholdSpin.up.pressed ? Theme.borderNormal : Theme.bgInput
                                        border.color: Theme.borderNormal
                                        radius: 2
                                        Label { anchors.centerIn: parent; text: "+"; color: Theme.textPrimary; font.pixelSize: 14 }
                                    }

                                    down.indicator: Rectangle {
                                        x: thresholdSpin.mirrored ? parent.width - width : 0
                                        height: parent.height
                                        implicitWidth: 32
                                        color: thresholdSpin.down.pressed ? Theme.borderNormal : Theme.bgInput
                                        border.color: Theme.borderNormal
                                        radius: 2
                                        Label { anchors.centerIn: parent; text: "-"; color: Theme.textPrimary; font.pixelSize: 14 }
                                    }

                                    background: Rectangle {
                                        color: Theme.bgInput
                                        border.color: thresholdSpin.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                                        radius: Theme.radiusSmall
                                    }
                                }
                            }
                        }
                    }

                    // OBB 任务类型指示器
                    Label {
                        id: taskTypeIndicator
                        Layout.fillWidth: true
                        visible: modelFamilyCombo.currentText === "yolov8_obb"
                        text: "[OBB] 旋转边界框训练模式"
                        color: Theme.accentWarning
                        font.pixelSize: Theme.fontSizeCaption
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
                                font.pixelSize: Theme.fontSizeNormal
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                if (currentProjectId === "") return
                                var snapshotId = snapshotCombo.currentValue
                                if (!snapshotId) return
                                var configJson = getConfigJson()
                                var runId = trainingService.createRun(currentProjectId, snapshotId, configJson)
                                if (runId !== "") {
                                    currentRunId = runId
                                    currentRunStatus = "draft"
                                    statusLabel.text = "训练任务已创建:" + runId.substring(0, 8) + "..."
                                    statusLabel.color = Theme.accentSuccess
                                    logView.clear()
                                    logView.appendLog("[LabelTorch] Training run created: " + runId)
                                    logView.appendLog("[LabelTorch] Starting training...")
                                    // 清空旧图表数据
                                    lossModel.clear()
                                    metricModel.clear()
                                    lossChart.requestPaint()
                                    metricChart.requestPaint()
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
                                font.pixelSize: Theme.fontSizeNormal
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
                        font.pixelSize: Theme.fontSizeCaption
                        wrapMode: Text.WordWrap
                    }

                    // 底部弹性空间
                    Item {
                        Layout.fillHeight: true
                        Layout.minimumHeight: 8
                    }
                }
            }
        }

        // === 右面板：图表 + 日志/历史 ===
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgCard
            radius: Theme.radiusNormal

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // 实时双图表区域
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 240
                    spacing: 8

                    // 图表 A：训练损失下降趋势
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: Theme.bgTertiary
                        radius: Theme.radiusSmall

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            // 图表标题栏
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                color: "transparent"
                                radius: Theme.radiusSmall

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8

                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: Theme.accentError
                                    }

                                    Label {
                                        text: "训练损失 (Loss)"
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.bold: true
                                    }

                                    Item { Layout.fillWidth: true }

                                    Label {
                                        text: lossModel.count > 0 ?
                                            "最新: " + lossModel.get(lossModel.count - 1).value.toFixed(4) : ""
                                        color: Theme.accentError
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamilyMono
                                    }
                                }
                            }

                            // Loss 图表画布
                            Canvas {
                                id: lossChart
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.margins: 2

                                onPaint: {
                                    var ctx = getContext("2d")
                                    root.drawChart(ctx, width, height, lossModel, "Loss", Theme.accentError, 0, false)
                                }
                            }
                        }
                    }

                    // 图表 B：精度指标上升趋势
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: Theme.bgTertiary
                        radius: Theme.radiusSmall

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            // 图表标题栏
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                color: "transparent"
                                radius: Theme.radiusSmall

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8

                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: Theme.accentSuccess
                                    }

                                    Label {
                                        text: metricName
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.bold: true
                                    }

                                    Item { Layout.fillWidth: true }

                                    Label {
                                        text: metricModel.count > 0 ?
                                            "最新: " + metricModel.get(metricModel.count - 1).value.toFixed(4) : ""
                                        color: Theme.accentSuccess
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamilyMono
                                    }
                                }
                            }

                            // 精度指标图表画布
                            Canvas {
                                id: metricChart
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.margins: 2

                                onPaint: {
                                    var ctx = getContext("2d")
                                    root.drawChart(ctx, width, height, metricModel, metricName, Theme.accentSuccess, deploymentThreshold, true)
                                }
                            }
                        }
                    }
                }

                // 实时进度标签
                Label {
                    id: progressLabel
                    Layout.fillWidth: true
                    text: ""
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: Theme.fontFamilyMono
                    visible: text !== ""
                }

                // 分割线
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.divider
                }

                // 日志/历史选项卡
                TabBar {
                    id: rightTabs
                    Layout.fillWidth: true

                    background: Rectangle { color: "transparent" }

                    TabButton {
                        text: "训练日志"
                        font.pixelSize: Theme.fontSizeNormal

                        contentItem: Label {
                            text: parent.text
                            color: parent.checked ? Theme.accentPrimary : Theme.textMuted
                            font.pixelSize: Theme.fontSizeNormal
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: parent.checked ? Theme.bgInput : "transparent"
                            radius: Theme.radiusSmall
                        }
                    }

                    TabButton {
                        text: "运行历史"
                        font.pixelSize: Theme.fontSizeNormal

                        contentItem: Label {
                            text: parent.text
                            color: parent.checked ? Theme.accentPrimary : Theme.textMuted
                            font.pixelSize: Theme.fontSizeNormal
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: parent.checked ? Theme.bgInput : "transparent"
                            radius: Theme.radiusSmall
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
                            font.pixelSize: Theme.fontSizeSubheading
                        }

                        delegate: Rectangle {
                            width: runHistoryList.width
                            height: 56
                            radius: 6
                            color: mouseArea.containsMouse ? Theme.bgInput : Theme.bgSecondary
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
                                    font.pixelSize: Theme.fontSizeNormal
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
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.bold: true
                                    }
                                }

                                // 快照ID
                                Label {
                                    text: "数据快照: " + model.snapshotId.substring(0, 8) + "..."
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSizeCaption
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
                                    font.pixelSize: Theme.fontSizeCaption
                                }

                                Item { Layout.fillWidth: true }

                                // 删除按钮
                                Button {
                                    text: "删除"
                                    flat: true
                                    visible: model.status === "draft" || model.status === "cancelled" || model.status === "failed"
                                    palette.buttonText: Theme.accentError
                                    font.pixelSize: Theme.fontSizeCaption
                                    onClicked: {
                                        if (trainingService.deleteRun(model.runId)) {
                                            trainingModel.refresh()
                                            if (model.runId === currentRunId) {
                                                currentRunId = ""
                                                currentRunStatus = ""
                                                logView.clear()
                                                lossModel.clear()
                                                metricModel.clear()
                                                lossChart.requestPaint()
                                                metricChart.requestPaint()
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
