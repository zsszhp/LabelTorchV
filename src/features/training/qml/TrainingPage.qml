// TrainingPage.qml - 训练工作台 V3
// 对标参考UI：左侧模型列表 + 子标签(配置参数/实时训练结果) + 2x2四宫格配置 + 4图表卡
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme
import LabelTorch.Components

Item {
    id: root

    property string currentProjectId: appController.currentProjectId
    property string currentRunId: ""
    property string currentRunStatus: ""
    property int currentSubTab: 0  // 0=配置参数, 1=实时训练结果

    // 实时指标数据模型
    ListModel { id: lossModel }
    ListModel { id: metricModel }
    ListModel { id: recallModel }
    ListModel { id: precisionModel }

    // 部署阈值：anomalib 用 AUROC > 0.95，ultralytics 用 mAP50 > 0.85
    property real deploymentThreshold: adapterCombo.currentText === "anomalib" ? 0.95 : 0.85
    // 当前精度指标名称，随适配器切换
    property string metricName: adapterCombo.currentText === "anomalib" ? "AUROC" : "mAP50"

    // 数据增强开关
    property bool augmentationEnabled: true
    property var ultralyticsModelFamilies: ["yolov5", "yolov8", "yolov8_obb", "yolov8_cls", "yolov10", "yolov11"]
    property var anomalibModelFamilies: ["patchcore", "padim", "efficient_ad", "stfpm"]

    onCurrentProjectIdChanged: {
        trainingModel.setProjectId(currentProjectId)
        snapshotModel.setProjectId(currentProjectId)
        currentRunId = ""
        currentRunStatus = ""
        logView.clear()
        lossModel.clear()
        metricModel.clear()
        recallModel.clear()
        precisionModel.clear()
        if (currentProjectId !== "") {
            root.applyTaskTypeToModelFamily(projectService.getTaskType(currentProjectId))
        }
    }

    function applyModelFamilyOptions(options, preferredValue) {
        modelFamilyCombo.model = options
        var targetIndex = options.indexOf(preferredValue)
        modelFamilyCombo.currentIndex = targetIndex >= 0 ? targetIndex : 0
    }

    function applyAdapterDefaults(adapterName) {
        if (adapterName === "anomalib") {
            applyModelFamilyOptions(anomalibModelFamilies, anomalibModelFamilies[0])
            return
        }
        applyModelFamilyOptions(ultralyticsModelFamilies, "yolov8")
    }

    // 根据任务类型自动选择模型系列与适配器
    function applyTaskTypeToModelFamily(taskType) {
        switch (taskType) {
            case "detect":
                applyModelFamilyOptions(ultralyticsModelFamilies, "yolov8")
                adapterCombo.currentIndex = adapterCombo.indexOfValue("ultralytics")
                break
            case "obb":
                applyModelFamilyOptions(ultralyticsModelFamilies, "yolov8_obb")
                adapterCombo.currentIndex = adapterCombo.indexOfValue("ultralytics")
                break
            case "classify":
                applyModelFamilyOptions(ultralyticsModelFamilies, "yolov8_cls")
                adapterCombo.currentIndex = adapterCombo.indexOfValue("ultralytics")
                break
            case "anomaly":
                applyModelFamilyOptions(anomalibModelFamilies, "patchcore")
                adapterCombo.currentIndex = adapterCombo.indexOfValue("anomalib")
                break
            default:
                applyModelFamilyOptions(ultralyticsModelFamilies, "yolov8")
                adapterCombo.currentIndex = adapterCombo.indexOfValue("ultralytics")
                break
        }
    }

    // 收集全部训练配置并序列化为 JSON
    function getConfigJson() {
        var trainingType = ["from_scratch", "pretrained", "incremental"][trainingTypeCombo.currentIndex]
        var usePretrainedWeights = trainingType !== "from_scratch"
        var config = {
            "adapter": adapterCombo.currentText,
            "img_size": imgSizeStepper.value,
            "imgsz": imgSizeStepper.value,
            "batch": batchStepper.value,
            "epochs": epochsStepper.value,
            "patience": patienceStepper.value,
            "workers": workersStepper.value,
            "amp": ampSwitch.checked,
            "resume": resumeSwitch.checked,
            "device": deviceCombo.currentText,
            "model_family": modelFamilyCombo.currentText,
            "training_type": trainingType,
            "pretrained": usePretrainedWeights,
            "pretrained_profile": pretrainedCombo.currentText,
            "input_channels": channelCombo.currentIndex === 0 ? 3 : 1,
            "save_period": savePeriodStepper.value,
            "iou": iouStepper.value / 100
        }
        if (trainingTypeCombo.currentIndex === 2 && parentVersionCombo.currentValue) {
            config["parent_model_version_id"] = parentVersionCombo.currentValue
        }
        if (adapterCombo.currentText === "ultralytics") {
            config["optimizer"] = optimizerCombo.currentText
            config["lr0"] = lrStepper.value / 10000
            config["weight_decay"] = wdStepper.value / 10000
        }
        if (adapterCombo.currentText === "anomalib") {
            config["backbone"] = backboneCombo.currentText
            config["anomaly_score_threshold"] = thresholdStepper.value / 100
        }
        // 数据增强配置
        if (augmentationEnabled) {
            config["augment"] = {
                "translate": augTranslateStepper.value / 100,
                "rotate": augRotateStepper.value,
                "scale": augScaleStepper.value / 100,
                "fliplr": augFliplrSwitch.checked,
                "flipud": augFlipudSwitch.checked,
                "brightness": augBrightnessStepper.value / 100,
                "hue": augHueStepper.value / 100,
                "saturation": augSaturationStepper.value / 100,
                "mosaic": augMosaicSwitch.checked
            }
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
            var recallValue = 0
            var precisionValue = 0
            if (adapterCombo.currentText === "anomalib") {
                metricValue = metrics["auroc"] || metrics["image_auroc"] || metrics["pixel_auroc"] || 0
            } else {
                metricValue = metrics["mAP50"] || metrics["map50"] || metrics["metrics/mAP50(B)"] || 0
                recallValue = metrics["recall"] || metrics["metrics/recall(B)"] || 0
                precisionValue = metrics["precision"] || metrics["metrics/precision(B)"] || 0
            }
            metricModel.append({"epoch": epoch, "value": metricValue})
            recallModel.append({"epoch": epoch, "value": recallValue})
            precisionModel.append({"epoch": epoch, "value": precisionValue})

            // 更新状态面板
            statusEpochText.text = epoch + " / " + totalEpochs
            statusLossText.text = loss.toFixed(4)
            statusMetricText.text = metricValue.toFixed(4)
            statusLrText.text = (metrics["lr"] || 0).toFixed(6)
            statusProgressBar.value = epoch / totalEpochs

            // 自动切换到实时训练结果标签
            if (currentSubTab === 0) {
                currentSubTab = 1
            }

            // 触发图表重绘
            lossChartCanvas.requestPaint()
            metricChartCanvas.requestPaint()
            recallChartCanvas.requestPaint()
            precisionChartCanvas.requestPaint()
        }

        function onTrainingLog(runId, logLine) {
            if (runId !== currentRunId) return
            logView.appendLog(logLine)
        }

        function onTrainingWarning(runId, message) {
            if (runId !== currentRunId) return
            logView.appendLog("[WARNING] " + message)
        }

        function onRunStatusChanged(runId, status) {
            if (runId !== currentRunId) return
            currentRunStatus = status
            if (status === "preparing") {
                statusMainLabel.text = "正在准备训练数据..."
                statusMainLabel.color = Theme.warning
                logView.appendLog("[LabelTorch] 正在准备训练数据目录...")
            } else if (status === "running") {
                statusMainLabel.text = "训练运行中"
                statusMainLabel.color = Theme.primary
                logView.appendLog("[LabelTorch] 训练已启动，等待进度事件...")
            } else if (status === "succeeded") {
                statusMainLabel.text = "训练完成"
                statusMainLabel.color = Theme.success
                logView.appendLog("[LabelTorch] 训练完成!")
            } else if (status === "failed") {
                statusMainLabel.text = "训练失败"
                statusMainLabel.color = Theme.danger
                logView.appendLog("[LabelTorch] 训练失败!")
            } else if (status === "cancelled" || status === "stopped") {
                statusMainLabel.text = "训练已停止"
                statusMainLabel.color = Theme.warning
                logView.appendLog("[LabelTorch] 训练已停止")
            }
            trainingModel.refresh()
        }
    }

    // === 图表绘制工具函数 ===

    function calculateDataRange(model, isLossModel) {
        if (model.count === 0) return { xMin: 0, xMax: 100, yMin: 0, yMax: isLossModel ? 2.0 : 1.0 }
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
        if (yMin < 0 && isLossModel) yMin = 0
        if (!isLossModel) { if (yMin < 0) yMin = 0; if (yMax > 1.05) yMax = 1.05 }
        return { xMin: xMin, xMax: xMax, yMin: yMin, yMax: yMax }
    }

    function createCoordinateMappers(padL, padR, padT, padB, w, h, xMin, xMax, yMin, yMax) {
        var cW = w - padL - padR
        var cH = h - padT - padB
        return {
            toX: function(ep) { return padL + (ep - xMin) / Math.max(xMax - xMin, 1) * cW },
            toY: function(val) { return padT + cH - (val - yMin) / Math.max(yMax - yMin, 0.001) * cH }
        }
    }

    function drawGridLines(ctx, padL, padR, padT, padB, w, h, xMin, xMax, yMin, yMax, toX, toY, modelCount) {
        var cH = h - padT - padB
        var ySteps = 5
        ctx.strokeStyle = Theme.chartGridLine
        ctx.lineWidth = 0.5
        for (var s = 0; s <= ySteps; s++) {
            var yVal = yMin + (yMax - yMin) * s / ySteps
            var yPx = toY(yVal)
            ctx.beginPath()
            ctx.moveTo(padL, yPx)
            ctx.lineTo(w - padR, yPx)
            ctx.stroke()
            ctx.fillStyle = Theme.textMuted
            ctx.font = "9px sans-serif"
            ctx.textAlign = "right"
            ctx.fillText(yVal.toFixed(2), padL - 4, yPx + 3)
        }
        var xSteps = modelCount === 0 ? 5 : Math.min(modelCount, 10)
        for (var s = 0; s <= xSteps; s++) {
            var xVal = xMin + (xMax - xMin) * s / xSteps
            var xPx = toX(xVal)
            ctx.beginPath()
            ctx.moveTo(xPx, padT)
            ctx.lineTo(xPx, padT + cH)
            ctx.stroke()
            ctx.fillStyle = Theme.textMuted
            ctx.font = "9px sans-serif"
            ctx.textAlign = "center"
            ctx.fillText(Math.round(xVal).toString(), xPx, padT + cH + 14)
        }
    }

    function drawAxes(ctx, padL, padR, padT, padB, w, h) {
        var cH = h - padT - padB
        ctx.strokeStyle = Theme.borderColor
        ctx.lineWidth = 1
        ctx.beginPath()
        ctx.moveTo(padL, padT)
        ctx.lineTo(padL, padT + cH)
        ctx.lineTo(w - padR, padT + cH)
        ctx.stroke()
    }

    function drawThresholdLine(ctx, padL, padR, w, toY, threshVal, yMin, yMax, showThresh) {
        if (showThresh && threshVal >= yMin && threshVal <= yMax) {
            var thY = toY(threshVal)
            ctx.strokeStyle = Theme.chartBaseline
            ctx.lineWidth = 1
            ctx.setLineDash([6, 4])
            ctx.beginPath()
            ctx.moveTo(padL, thY)
            ctx.lineTo(w - padR, thY)
            ctx.stroke()
            ctx.setLineDash([])
            ctx.fillStyle = Theme.chartBaseline
            ctx.font = "bold 9px sans-serif"
            ctx.textAlign = "right"
            ctx.fillText("部署阈值 " + threshVal.toFixed(2), w - padR - 4, thY - 4)
        }
    }

    function drawDataLine(ctx, model, toX, toY, lineColor) {
        if (model.count === 0) return
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
    }

    function drawDataPoints(ctx, model, toX, toY, lineColor) {
        for (var i = 0; i < model.count; i++) {
            var px = toX(model.get(i).epoch)
            var py = toY(model.get(i).value)
            ctx.fillStyle = lineColor
            ctx.beginPath()
            ctx.arc(px, py, 2, 0, Math.PI * 2)
            ctx.fill()
        }
    }

    function drawLatestPointHighlight(ctx, model, toX, toY, lineColor) {
        if (model.count > 0) {
            var li = model.count - 1
            var lpx = toX(model.get(li).epoch)
            var lpy = toY(model.get(li).value)
            ctx.fillStyle = lineColor
            ctx.globalAlpha = 0.3
            ctx.beginPath()
            ctx.arc(lpx, lpy, 6, 0, Math.PI * 2)
            ctx.fill()
            ctx.globalAlpha = 1.0
            ctx.beginPath()
            ctx.arc(lpx, lpy, 3, 0, Math.PI * 2)
            ctx.fill()
            ctx.fillStyle = Theme.textMain
            ctx.font = "bold 10px sans-serif"
            ctx.textAlign = "left"
            ctx.fillText(model.get(li).value.toFixed(4), lpx + 8, lpy - 4)
        }
    }

    function drawChart(ctx, w, h, model, yLabel, lineColor, threshVal, showThresh) {
        var padL = 48, padR = 12, padT = 6, padB = 26

        ctx.clearRect(0, 0, w, h)
        ctx.fillStyle = Theme.bgChart
        ctx.fillRect(0, 0, w, h)

        var isLossModel = (model === lossModel)

        if (model.count === 0) {
            var xMin = 0
            var xMax = epochsStepper.value || 100
            var yMin = 0.0
            var yMax = isLossModel ? 2.0 : 1.0
            var emptyMappers = createCoordinateMappers(padL, padR, padT, padB, w, h, xMin, xMax, yMin, yMax)
            drawGridLines(ctx, padL, padR, padT, padB, w, h, xMin, xMax, yMin, yMax, emptyMappers.toX, emptyMappers.toY, 0)
            drawAxes(ctx, padL, padR, padT, padB, w, h)
            drawThresholdLine(ctx, padL, padR, w, emptyMappers.toY, threshVal, yMin, yMax, showThresh)
            ctx.fillStyle = Theme.textMuted
            ctx.font = "12px sans-serif"
            ctx.textAlign = "center"
            ctx.fillText("等待训练数据...", w / 2, h / 2)
            return
        }

        var range = calculateDataRange(model, isLossModel)
        var mappers = createCoordinateMappers(padL, padR, padT, padB, w, h, range.xMin, range.xMax, range.yMin, range.yMax)
        drawGridLines(ctx, padL, padR, padT, padB, w, h, range.xMin, range.xMax, range.yMin, range.yMax, mappers.toX, mappers.toY, model.count)
        drawAxes(ctx, padL, padR, padT, padB, w, h)
        drawThresholdLine(ctx, padL, padR, w, mappers.toY, threshVal, range.yMin, range.yMax, showThresh)
        drawDataLine(ctx, model, mappers.toX, mappers.toY, lineColor)
        drawDataPoints(ctx, model, mappers.toX, mappers.toY, lineColor)
        drawLatestPointHighlight(ctx, model, mappers.toX, mappers.toY, lineColor)
    }

    // === 未打开项目时的空状态提示 ===
    ColumnLayout {
        anchors.centerIn: parent
        visible: currentProjectId === ""
        spacing: Theme.spacingLarge

        Text {
            text: "请先打开一个项目"
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeTitle
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }
        Text {
            text: "在左侧项目中心创建或打开项目后，即可开始训练"
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeNormal
            Layout.alignment: Qt.AlignHCenter
        }
        Button {
            text: "前往项目中心"
            font.family: Theme.fontFamily
            Layout.alignment: Qt.AlignHCenter
            background: Rectangle {
                color: parent.hovered ? Theme.primary : Theme.bgCard
                radius: Theme.radiusSmall
                border.color: Theme.primary
                border.width: 1
                implicitWidth: 140
                implicitHeight: 36
            }
            contentItem: Text {
                text: parent.text
                color: Theme.primary
                font.pixelSize: Theme.fontSizeNormal
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: appController.currentPageIndex = 0
        }
    }

    // === 主布局：左侧模型列表 + 右侧内容区 ===
    RowLayout {
        anchors.fill: parent
        spacing: 0
        visible: currentProjectId !== ""

        // === 左侧模型列表(240px) ===
        Rectangle {
            id: sidebar
            width: Theme.sidebarWidth
            Layout.preferredWidth: width
            Layout.fillHeight: true
            color: Theme.bgSide

            ScrollView {
                anchors.fill: parent
                anchors.rightMargin: 1
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width - Theme.spacingNormal * 2
                    x: Theme.spacingNormal
                    y: Theme.spacingNormal
                    spacing: Theme.spacingNormal

                // 区块标题 + 添加按钮
                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "模型列表"
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeSubheading
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        implicitWidth: 24
                        implicitHeight: 24
                        background: Rectangle {
                            color: parent.hovered ? Theme.bgHover : "transparent"
                            radius: Theme.radiusSmall
                        }
                        contentItem: Text {
                            text: "+"
                            color: Theme.primaryGlow
                            font.pixelSize: Theme.fontSizeSubheading
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: addModelDialog.open()
                    }
                }

                // 模型列表
                ListView {
                    id: modelListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 2

                    model: trainingModel

                    delegate: Rectangle {
                        width: modelListView.width
                        height: 48
                        radius: Theme.radiusSmall
                        color: model.runId === currentRunId ? Qt.alpha(Theme.primaryGlow, 0.05) : "transparent"
                        border.color: model.runId === currentRunId ? Theme.primaryGlow : "transparent"
                        border.width: model.runId === currentRunId ? 1 : 0

                        // 左侧选中指示条
                        Rectangle {
                            visible: model.runId === currentRunId
                            width: 3
                            height: parent.height - 8
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            radius: 1.5
                            color: Theme.primaryGlow
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            spacing: 2

                            Text {
                                text: model.runId.substring(0, 8) + "..."
                                color: Theme.textMain
                                font.pixelSize: Theme.fontSizeSmall
                                font.family: Theme.fontFamilyMono
                                font.bold: true
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Text {
                                text: {
                                    switch (model.status) {
                                    case "running": return "训练中"
                                    case "succeeded": return "已完成"
                                    case "failed": return "失败"
                                    case "cancelled": return "已停止"
                                    case "draft": return "就绪"
                                    case "preparing": return "准备中"
                                    default: return model.status
                                    }
                                }
                                color: {
                                    switch (model.status) {
                                    case "running": return Theme.warning
                                    case "succeeded": return Theme.success
                                    case "failed": return Theme.danger
                                    case "cancelled": return Theme.textMuted
                                    case "draft": return Theme.primary
                                    case "preparing": return Theme.warning
                                    default: return Theme.textMuted
                                    }
                                }
                                font.pixelSize: Theme.fontSizeCaption
                                Layout.fillWidth: true
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                currentRunId = model.runId
                                currentRunStatus = model.status
                                // 加载运行日志
                                logView.clear()
                                logView.appendLog("[LabelTorch] Run: " + model.runId)
                                logView.appendLog("[LabelTorch] Status: " + model.status)
                                logView.appendLog("[LabelTorch] Config: " + model.configJson)
                            }
                        }
                    }

                    // 空状态
                    Text {
                        anchors.centerIn: parent
                        visible: modelListView.count === 0
                        text: "暂无训练任务"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeNormal
                    }
                }

                // 底部"开始训练"按钮
                Button {
                    id: startTrainingBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    enabled: snapshotCombo.currentIndex >= 0 && currentRunStatus !== "running" && currentRunStatus !== "preparing"

                    background: Rectangle {
                        radius: Theme.radiusSmall
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: parent.parent.enabled ? Theme.primary : Theme.borderColor }
                            GradientStop { position: 1.0; color: parent.parent.enabled ? Theme.primaryGlow : Theme.borderColor }
                        }
                    }

                    contentItem: Text {
                        text: "开始训练"
                        color: parent.enabled ? "#FFFFFF" : Theme.textMuted
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
                            logView.clear()
                            logView.appendLog("[LabelTorch] Training run created: " + runId)
                            logView.appendLog("[LabelTorch] Starting training...")
                            // 清空旧图表数据
                            lossModel.clear()
                            metricModel.clear()
                            recallModel.clear()
                            precisionModel.clear()
                            lossChartCanvas.requestPaint()
                            metricChartCanvas.requestPaint()
                            recallChartCanvas.requestPaint()
                            precisionChartCanvas.requestPaint()
                            if (trainingService.startTraining(runId)) {
                                currentRunStatus = "running"
                                trainingModel.refresh()
                                statusMainLabel.text = "训练已启动"
                                statusMainLabel.color = Theme.primary
                            } else {
                                statusMainLabel.text = "训练启动失败"
                                statusMainLabel.color = Theme.danger
                                logView.appendLog("[LabelTorch] ERROR: Failed to start training")
                            }
                        } else {
                            statusMainLabel.text = "创建训练任务失败"
                            statusMainLabel.color = Theme.danger
                        }
                    }
                }

                // 停止按钮
                Button {
                    id: stopTrainingBtn
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    visible: currentRunStatus === "running"
                    background: Rectangle {
                        color: parent.hovered ? Qt.darker(Theme.danger, 1.2) : Theme.danger
                        radius: Theme.radiusSmall
                    }
                    contentItem: Text {
                        text: "停止训练"
                        color: "#FFFFFF"
                        font.pixelSize: Theme.fontSizeSmall
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
        }
    }

    // 左右分割线
    Splitter {
        id: sidebarResizer
        Layout.preferredWidth: 4
        Layout.fillHeight: true
        vertical: true
        targetItem: sidebar
        minSize: Theme.sidebarMinWidth
        maxSize: 400
    }

        // === 右侧内容区 ===
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // === 子标签栏(40px) ===
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.subTabHeight
                color: Theme.bgSide

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingLarge
                    spacing: 0

                    // 配置参数标签
                    Rectangle {
                        Layout.preferredWidth: 100
                        Layout.fillHeight: true
                        color: "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "配置参数"
                            color: currentSubTab === 0 ? Theme.primaryGlow : Theme.textMuted
                            font.pixelSize: Theme.fontSizeNormal
                            font.bold: currentSubTab === 0
                        }

                        // 底部指示条
                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - 16
                            height: 2
                            radius: 1
                            color: currentSubTab === 0 ? Theme.primaryGlow : "transparent"
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: currentSubTab = 0
                        }
                    }

                    // 实时训练结果标签
                    Rectangle {
                        Layout.preferredWidth: 120
                        Layout.fillHeight: true
                        color: "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "实时训练结果"
                            color: currentSubTab === 1 ? Theme.primaryGlow : Theme.textMuted
                            font.pixelSize: Theme.fontSizeNormal
                            font.bold: currentSubTab === 1
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width - 16
                            height: 2
                            radius: 1
                            color: currentSubTab === 1 ? Theme.primaryGlow : "transparent"
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: currentSubTab = 1
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // 状态指示
                    Text {
                        id: statusMainLabel
                        text: ""
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                        visible: text !== ""
                        Layout.rightMargin: Theme.spacingLarge
                    }
                }
            }

            // 分割线
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.borderColor
            }

            // === 子标签内容 ===
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: currentSubTab

                // ====== 子标签0：配置参数（2x2四宫格） ======
                Item {
                    id: configPage

                    // 2x2网格布局
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingNormal
                        spacing: 1

                        // 左列
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 1

                            // === 左上：数据集配置 ===
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                color: Theme.bgCard
                                radius: Theme.radiusNormal

                                ScrollView {
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingNormal
                                    clip: true
                                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                    ColumnLayout {
                                        width: parent.width - Theme.spacingNormal * 2
                                        spacing: Theme.spacingNormal

                                        // 区块标题
                                        SectionTitle {
                                            text: "数据集配置"
                                            Layout.fillWidth: true
                                        }

                                        // 训练集（数据快照）选择
                                        ParamRow {
                                            label: "训练集"
                                            labelWidth: 80
                                            Layout.fillWidth: true

                                            ComboBox {
                                                id: snapshotCombo
                                                anchors.fill: parent
                                                model: snapshotModel
                                                textRole: "snapshotId"
                                                valueRole: "snapshotId"
                                                displayText: currentIndex >= 0 && currentValue ?
                                                    currentValue.substring(0, 8) + "..." : "选择数据快照"

                                                contentItem: Text {
                                                    text: snapshotCombo.displayText
                                                    color: Theme.textMain
                                                    font.pixelSize: Theme.fontSizeSmall
                                                    verticalAlignment: Text.AlignVCenter
                                                    leftPadding: 8
                                                    elide: Text.ElideRight
                                                }

                                                background: Rectangle {
                                                    color: Theme.bgInput
                                                    radius: Theme.radiusSmall
                                                    border.color: snapshotCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                                                    border.width: 1
                                                }

                                                delegate: ItemDelegate {
                                                    width: snapshotCombo.width
                                                    contentItem: Text {
                                                        text: model.snapshotId.substring(0, 8) + "... (" + model.sampleCount + " 样本, train:" + model.trainCount + " val:" + model.valCount + ")"
                                                        color: highlighted ? Theme.primaryGlow : Theme.textMain
                                                        font.pixelSize: Theme.fontSizeCaption
                                                        font.family: Theme.fontFamilyMono
                                                        elide: Text.ElideRight
                                                    }
                                                    highlighted: snapshotCombo.highlightedIndex === index
                                                    background: Rectangle {
                                                        color: highlighted ? Theme.bgHover : Theme.bgMain
                                                    }
                                                }
                                            }
                                        }

                                        // 快照信息
                                        Text {
                                            id: snapshotInfoLabel
                                            Layout.fillWidth: true
                                            color: Theme.textMuted
                                            font.pixelSize: Theme.fontSizeCaption
                                            wrapMode: Text.WordWrap
                                            visible: text !== ""

                                            text: {
                                                if (snapshotCombo.currentIndex < 0) return ""
                                                var idx = snapshotCombo.currentIndex
                                                var trainCount = snapshotModel.data(snapshotModel.index(idx, 0), Qt.UserRole + 3)
                                                var valCount = snapshotModel.data(snapshotModel.index(idx, 0), Qt.UserRole + 4)
                                                var taxVer = snapshotModel.data(snapshotModel.index(idx, 0), Qt.UserRole + 5)
                                                if (trainCount === undefined) return ""
                                                return "Train: " + trainCount + " | Val: " + valCount + " | Taxonomy: " + (taxVer || "unknown")
                                            }
                                        }

                                        // 创建快照按钮
                                        Button {
                                            text: "+ 创建快照"
                                            font.family: Theme.fontFamily
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 28
                                            background: Rectangle {
                                                color: parent.hovered ? Theme.bgHover : Theme.bgInput
                                                radius: Theme.radiusSmall
                                                border.color: Theme.primary
                                                border.width: 1
                                            }
                                            contentItem: Text {
                                                text: parent.text
                                                color: Theme.primary
                                                font.pixelSize: Theme.fontSizeSmall
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            onClicked: createSnapshotDialog.open()
                                        }

                                        // 验证集（与训练集同快照，只读显示）
                                        ParamRow {
                                            label: "验证集"
                                            labelWidth: 80
                                            Layout.fillWidth: true

                                            Text {
                                                anchors.fill: parent
                                                verticalAlignment: Text.AlignVCenter
                                                text: snapshotCombo.currentIndex >= 0 ? "同快照验证集" : "—"
                                                color: Theme.textMuted
                                                font.pixelSize: Theme.fontSizeSmall
                                            }
                                        }
                                    }
                                }
                            }

                            // === 左下：训练设置 ===
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                color: Theme.bgCard
                                radius: Theme.radiusNormal

                                ScrollView {
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingNormal
                                    clip: true
                                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                    ColumnLayout {
                                        width: parent.width - Theme.spacingNormal * 2
                                        spacing: Theme.spacingSmall

                                        SectionTitle {
                                            text: "训练设置"
                                            Layout.fillWidth: true
                                        }

                                        // Epochs
                                        ParamRow {
                                            label: "Epochs"
                                            labelWidth: 100
                                            Layout.fillWidth: true
                                            Stepper {
                                                id: epochsStepper
                                                value: 100
                                                minValue: 1
                                                maxValue: 1000
                                                stepSize: 10
                                            }
                                        }

                                        // 批量大小
                                        ParamRow {
                                            label: "批量大小"
                                            labelWidth: 100
                                            Layout.fillWidth: true
                                            Stepper {
                                                id: batchStepper
                                                value: 16
                                                minValue: 1
                                                maxValue: 128
                                                stepSize: 4
                                            }
                                        }

                                        // 优化器
                                        ParamRow {
                                            label: "优化器"
                                            labelWidth: 100
                                            Layout.fillWidth: true
                                            visible: adapterCombo.currentText === "ultralytics"

                                            ComboBox {
                                                id: optimizerCombo
                                                anchors.fill: parent
                                                model: ["SGD", "Adam", "AdamW", "NAdam", "RAdam"]
                                                currentIndex: 2

                                                contentItem: Text {
                                                    text: optimizerCombo.displayText
                                                    color: Theme.textMain
                                                    font.pixelSize: Theme.fontSizeSmall
                                                    verticalAlignment: Text.AlignVCenter
                                                    leftPadding: 8
                                                }
                                                background: Rectangle {
                                                    color: Theme.bgInput
                                                    radius: Theme.radiusSmall
                                                    border.color: optimizerCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                                                    border.width: 1
                                                }
                                                delegate: ItemDelegate {
                                                    width: optimizerCombo.width
                                                    contentItem: Text {
                                                        text: modelData
                                                        color: highlighted ? Theme.primaryGlow : Theme.textMain
                                                        font.pixelSize: Theme.fontSizeSmall
                                                    }
                                                    highlighted: optimizerCombo.highlightedIndex === index
                                                    background: Rectangle { color: highlighted ? Theme.bgHover : Theme.bgMain }
                                                }
                                            }
                                        }

                                        // 学习率
                                        ParamRow {
                                            label: "学习率"
                                            labelWidth: 100
                                            Layout.fillWidth: true
                                            visible: adapterCombo.currentText === "ultralytics"

                                            Stepper {
                                                id: lrStepper
                                                value: 10
                                                minValue: 1
                                                maxValue: 1000
                                                stepSize: 1
                                                decimals: 4
                                                // 显示为 0.00x 格式，实际值 = value / 10000
                                                property string displayText: (value / 10000).toFixed(4)
                                            }
                                        }

                                        // 设备
                                        ParamRow {
                                            label: "设备"
                                            labelWidth: 100
                                            Layout.fillWidth: true

                                            ComboBox {
                                                id: deviceCombo
                                                anchors.fill: parent
                                                model: ["auto", "cpu", "0", "1", "2", "3"]
                                                currentIndex: 0

                                                contentItem: Text {
                                                    text: deviceCombo.displayText
                                                    color: Theme.textMain
                                                    font.pixelSize: Theme.fontSizeSmall
                                                    verticalAlignment: Text.AlignVCenter
                                                    leftPadding: 8
                                                }
                                                background: Rectangle {
                                                    color: Theme.bgInput
                                                    radius: Theme.radiusSmall
                                                    border.color: deviceCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                                                    border.width: 1
                                                }
                                                delegate: ItemDelegate {
                                                    width: deviceCombo.width
                                                    contentItem: Text {
                                                        text: modelData
                                                        color: highlighted ? Theme.primaryGlow : Theme.textMain
                                                        font.pixelSize: Theme.fontSizeSmall
                                                    }
                                                    highlighted: deviceCombo.highlightedIndex === index
                                                    background: Rectangle { color: highlighted ? Theme.bgHover : Theme.bgMain }
                                                }
                                            }
                                        }

                                        // 存储模型周期
                                        ParamRow {
                                            label: "存储周期"
                                            labelWidth: 100
                                            Layout.fillWidth: true

                                            Stepper {
                                                id: savePeriodStepper
                                                value: 0
                                                minValue: 0
                                                maxValue: 100
                                                stepSize: 1
                                            }
                                        }

                                        // 融合IOU阈值
                                        ParamRow {
                                            label: "融合IOU"
                                            labelWidth: 100
                                            Layout.fillWidth: true

                                            Stepper {
                                                id: iouStepper
                                                value: 70
                                                minValue: 0
                                                maxValue: 100
                                                stepSize: 5
                                                suffix: "%"
                                            }
                                        }

                                        // 训练完成后自动测试
                                        ParamRow {
                                            label: "自动测试"
                                            labelWidth: 100
                                            Layout.fillWidth: true

                                            ToggleSwitch {
                                                id: autoTestSwitch
                                                checked: false
                                            }
                                        }

                                        // Anomalib 专属：骨干网络
                                        ParamRow {
                                            label: "骨干网络"
                                            labelWidth: 100
                                            Layout.fillWidth: true
                                            visible: adapterCombo.currentText === "anomalib"

                                            ComboBox {
                                                id: backboneCombo
                                                anchors.fill: parent
                                                model: ["resnet18", "wide_resnet50_2", "efficientnet_b0", "mobilenet_v2"]
                                                currentIndex: 1

                                                contentItem: Text {
                                                    text: backboneCombo.displayText
                                                    color: Theme.textMain
                                                    font.pixelSize: Theme.fontSizeSmall
                                                    verticalAlignment: Text.AlignVCenter
                                                    leftPadding: 8
                                                }
                                                background: Rectangle {
                                                    color: Theme.bgInput
                                                    radius: Theme.radiusSmall
                                                    border.color: backboneCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                                                    border.width: 1
                                                }
                                                delegate: ItemDelegate {
                                                    width: backboneCombo.width
                                                    contentItem: Text {
                                                        text: modelData
                                                        color: highlighted ? Theme.primaryGlow : Theme.textMain
                                                        font.pixelSize: Theme.fontSizeSmall
                                                    }
                                                    highlighted: backboneCombo.highlightedIndex === index
                                                    background: Rectangle { color: highlighted ? Theme.bgHover : Theme.bgMain }
                                                }
                                            }
                                        }

                                        // Anomalib 专属：异常阈值
                                        ParamRow {
                                            label: "异常阈值"
                                            labelWidth: 100
                                            Layout.fillWidth: true
                                            visible: adapterCombo.currentText === "anomalib"

                                            Stepper {
                                                id: thresholdStepper
                                                value: 50
                                                minValue: 0
                                                maxValue: 100
                                                stepSize: 1
                                                decimals: 2
                                                suffix: "%"
                                            }
                                        }

                                        // 可折叠高级参数
                                        CollapsibleSection {
                                            title: "高级参数"
                                            Layout.fillWidth: true

                                            ColumnLayout {
                                                width: parent.width
                                                spacing: Theme.spacingSmall

                                                // 早停耐心值
                                                ParamRow {
                                                    label: "早停耐心"
                                                    labelWidth: 100
                                                    Layout.fillWidth: true
                                                    Stepper {
                                                        id: patienceStepper
                                                        value: 50
                                                        minValue: 0
                                                        maxValue: 200
                                                        stepSize: 5
                                                    }
                                                }

                                                // 工作线程
                                                ParamRow {
                                                    label: "工作线程"
                                                    labelWidth: 100
                                                    Layout.fillWidth: true
                                                    Stepper {
                                                        id: workersStepper
                                                        value: 8
                                                        minValue: 0
                                                        maxValue: 32
                                                        stepSize: 1
                                                    }
                                                }

                                                // 混合精度
                                                ParamRow {
                                                    label: "混合精度"
                                                    labelWidth: 100
                                                    Layout.fillWidth: true
                                                    ToggleSwitch {
                                                        id: ampSwitch
                                                        checked: true
                                                    }
                                                }

                                                // 继续训练
                                                ParamRow {
                                                    label: "继续训练"
                                                    labelWidth: 100
                                                    Layout.fillWidth: true
                                                    ToggleSwitch {
                                                        id: resumeSwitch
                                                        checked: false
                                                    }
                                                }

                                                // 权重衰减
                                                ParamRow {
                                                    label: "权重衰减"
                                                    labelWidth: 100
                                                    Layout.fillWidth: true
                                                    visible: adapterCombo.currentText === "ultralytics"
                                                    Stepper {
                                                        id: wdStepper
                                                        value: 5
                                                        minValue: 0
                                                        maxValue: 1000
                                                        stepSize: 1
                                                        decimals: 4
                                                    }
                                                }

                                                // 训练类型
                                                ParamRow {
                                                    label: "训练类型"
                                                    labelWidth: 100
                                                    Layout.fillWidth: true

                                                    ComboBox {
                                                        id: trainingTypeCombo
                                                        anchors.fill: parent
                                                        model: ["从头训练", "预训练", "增量训练"]
                                                        currentIndex: 0

                                                        contentItem: Text {
                                                            text: trainingTypeCombo.displayText
                                                            color: Theme.textMain
                                                            font.pixelSize: Theme.fontSizeSmall
                                                            verticalAlignment: Text.AlignVCenter
                                                            leftPadding: 8
                                                        }
                                                        background: Rectangle {
                                                            color: Theme.bgInput
                                                            radius: Theme.radiusSmall
                                                            border.color: trainingTypeCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                                                            border.width: 1
                                                        }
                                                        delegate: ItemDelegate {
                                                            width: trainingTypeCombo.width
                                                            contentItem: Text {
                                                                text: modelData
                                                                color: highlighted ? Theme.primaryGlow : Theme.textMain
                                                                font.pixelSize: Theme.fontSizeSmall
                                                            }
                                                            highlighted: trainingTypeCombo.highlightedIndex === index
                                                            background: Rectangle { color: highlighted ? Theme.bgHover : Theme.bgMain }
                                                        }
                                                    }
                                                }

                                                // 父模型版本（增量训练时可见）
                                                ParamRow {
                                                    label: "父版本"
                                                    labelWidth: 100
                                                    Layout.fillWidth: true
                                                    visible: trainingTypeCombo.currentIndex === 2

                                                    ComboBox {
                                                        id: parentVersionCombo
                                                        anchors.fill: parent
                                                        model: modelVersionModel
                                                        textRole: "versionId"
                                                        valueRole: "versionId"

                                                        contentItem: Text {
                                                            text: parentVersionCombo.currentIndex >= 0 ?
                                                                parentVersionCombo.currentValue.substring(0, 8) + "..." :
                                                                "选择父版本"
                                                            color: Theme.textMain
                                                            font.pixelSize: Theme.fontSizeSmall
                                                            font.family: Theme.fontFamilyMono
                                                            verticalAlignment: Text.AlignVCenter
                                                            leftPadding: 8
                                                            elide: Text.ElideRight
                                                        }
                                                        background: Rectangle {
                                                            color: Theme.bgInput
                                                            radius: Theme.radiusSmall
                                                            border.color: parentVersionCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                                                            border.width: 1
                                                        }
                                                        delegate: ItemDelegate {
                                                            width: parentVersionCombo.width
                                                            contentItem: Text {
                                                                text: model.versionId.substring(0, 8) + "... (" + model.bestWeightPath + ")"
                                                                color: highlighted ? Theme.primaryGlow : Theme.textMain
                                                                font.pixelSize: Theme.fontSizeCaption
                                                                font.family: Theme.fontFamilyMono
                                                            }
                                                            highlighted: parentVersionCombo.highlightedIndex === index
                                                            background: Rectangle { color: highlighted ? Theme.bgHover : Theme.bgMain }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // 右列
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 1

                            // === 右上：网络配置 ===
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                color: Theme.bgCard
                                radius: Theme.radiusNormal

                                ScrollView {
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingNormal
                                    clip: true
                                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                    ColumnLayout {
                                        width: parent.width - Theme.spacingNormal * 2
                                        spacing: Theme.spacingNormal

                                        SectionTitle {
                                            text: "网络配置"
                                            Layout.fillWidth: true
                                        }

                                        // 适配器选择
                                        ParamRow {
                                            label: "训练适配器"
                                            labelWidth: 80
                                            Layout.fillWidth: true

                                            ComboBox {
                                                id: adapterCombo
                                                anchors.fill: parent
                                                model: trainingService.listAdapters()
                                                valueRole: "value"

                                                contentItem: Text {
                                                    text: adapterCombo.displayText
                                                    color: Theme.textMain
                                                    font.pixelSize: Theme.fontSizeSmall
                                                    verticalAlignment: Text.AlignVCenter
                                                    leftPadding: 8
                                                }
                                                background: Rectangle {
                                                    color: Theme.bgInput
                                                    radius: Theme.radiusSmall
                                                    border.color: adapterCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                                                    border.width: 1
                                                }
                                                delegate: ItemDelegate {
                                                    width: adapterCombo.width
                                                    contentItem: Text {
                                                        text: modelData
                                                        color: highlighted ? Theme.primaryGlow : Theme.textMain
                                                        font.pixelSize: Theme.fontSizeSmall
                                                    }
                                                    highlighted: adapterCombo.highlightedIndex === index
                                                    background: Rectangle { color: highlighted ? Theme.bgHover : Theme.bgMain }
                                                }

                                                onActivated: {
                                                    root.applyAdapterDefaults(currentText)
                                                }
                                            }
                                        }

                                        // 网络结构（模型系列）
                                        ParamRow {
                                            label: "网络结构"
                                            labelWidth: 80
                                            Layout.fillWidth: true

                                            ComboBox {
                                                id: modelFamilyCombo
                                                anchors.fill: parent
                                                model: root.ultralyticsModelFamilies
                                                currentIndex: 1

                                                contentItem: Text {
                                                    text: modelFamilyCombo.displayText
                                                    color: Theme.textMain
                                                    font.pixelSize: Theme.fontSizeSmall
                                                    verticalAlignment: Text.AlignVCenter
                                                    leftPadding: 8
                                                }
                                                background: Rectangle {
                                                    color: Theme.bgInput
                                                    radius: Theme.radiusSmall
                                                    border.color: modelFamilyCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                                                    border.width: 1
                                                }
                                                delegate: ItemDelegate {
                                                    width: modelFamilyCombo.width
                                                    contentItem: Text {
                                                        text: modelData
                                                        color: highlighted ? Theme.primaryGlow : Theme.textMain
                                                        font.pixelSize: Theme.fontSizeSmall
                                                    }
                                                    highlighted: modelFamilyCombo.highlightedIndex === index
                                                    background: Rectangle { color: highlighted ? Theme.bgHover : Theme.bgMain }
                                                }
                                            }
                                        }

                                        // 预训练模型
                                        ParamRow {
                                            label: "预训练模型"
                                            labelWidth: 80
                                            Layout.fillWidth: true

                                            ComboBox {
                                                id: pretrainedCombo
                                                anchors.fill: parent
                                                model: ["默认", "COCO预训练"]
                                                currentIndex: 1

                                                contentItem: Text {
                                                    text: pretrainedCombo.displayText
                                                    color: Theme.textMain
                                                    font.pixelSize: Theme.fontSizeSmall
                                                    verticalAlignment: Text.AlignVCenter
                                                    leftPadding: 8
                                                }
                                                background: Rectangle {
                                                    color: Theme.bgInput
                                                    radius: Theme.radiusSmall
                                                    border.color: pretrainedCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                                                    border.width: 1
                                                }
                                                delegate: ItemDelegate {
                                                    width: pretrainedCombo.width
                                                    contentItem: Text {
                                                        text: modelData
                                                        color: highlighted ? Theme.primaryGlow : Theme.textMain
                                                        font.pixelSize: Theme.fontSizeSmall
                                                    }
                                                    highlighted: pretrainedCombo.highlightedIndex === index
                                                    background: Rectangle { color: highlighted ? Theme.bgHover : Theme.bgMain }
                                                }
                                            }
                                        }

                                        // 图像大小
                                        ParamRow {
                                            label: "图像大小"
                                            labelWidth: 80
                                            Layout.fillWidth: true

                                            Stepper {
                                                id: imgSizeStepper
                                                anchors.right: parent.right
                                                anchors.verticalCenter: parent.verticalCenter
                                                value: 640
                                                minValue: 320
                                                maxValue: 1280
                                                stepSize: 32
                                            }
                                        }

                                        // 图像通道
                                        ParamRow {
                                            label: "图像通道"
                                            labelWidth: 80
                                            Layout.fillWidth: true

                                            ComboBox {
                                                id: channelCombo
                                                anchors.fill: parent
                                                model: ["3 (RGB)", "1 (灰度)"]
                                                currentIndex: 0

                                                contentItem: Text {
                                                    text: channelCombo.displayText
                                                    color: Theme.textMain
                                                    font.pixelSize: Theme.fontSizeSmall
                                                    verticalAlignment: Text.AlignVCenter
                                                    leftPadding: 8
                                                }
                                                background: Rectangle {
                                                    color: Theme.bgInput
                                                    radius: Theme.radiusSmall
                                                    border.color: channelCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                                                    border.width: 1
                                                }
                                                delegate: ItemDelegate {
                                                    width: channelCombo.width
                                                    contentItem: Text {
                                                        text: modelData
                                                        color: highlighted ? Theme.primaryGlow : Theme.textMain
                                                        font.pixelSize: Theme.fontSizeSmall
                                                    }
                                                    highlighted: channelCombo.highlightedIndex === index
                                                    background: Rectangle { color: highlighted ? Theme.bgHover : Theme.bgMain }
                                                }
                                            }
                                        }

                                        // OBB 任务类型指示器
                                        Text {
                                            Layout.fillWidth: true
                                            visible: modelFamilyCombo.currentText === "yolov8_obb"
                                            text: "[OBB] 旋转边界框训练模式"
                                            color: Theme.warning
                                            font.pixelSize: Theme.fontSizeCaption
                                            font.bold: true
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }

                            // === 右下：数据增强配置 ===
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                color: Theme.bgCard
                                radius: Theme.radiusNormal

                                ScrollView {
                                    anchors.fill: parent
                                    anchors.margins: Theme.spacingNormal
                                    clip: true
                                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                    ColumnLayout {
                                        width: parent.width - Theme.spacingNormal * 2
                                        spacing: Theme.spacingSmall

                                        // 标题行 + 开关 + 预览按钮
                                        RowLayout {
                                            Layout.fillWidth: true

                                            SectionTitle {
                                                text: "数据增强配置"
                                            }

                                            Item { Layout.fillWidth: true }

                                            Text {
                                                text: "启用"
                                                color: Theme.textMuted
                                                font.pixelSize: Theme.fontSizeSmall
                                            }

                                            ToggleSwitch {
                                                id: augEnabledSwitch
                                                checked: root.augmentationEnabled
                                                onToggled: root.augmentationEnabled = checked
                                            }

                                            Button {
                                                text: "预览"
                                                font.pixelSize: Theme.fontSizeSmall
                                                font.family: Theme.fontFamily
                                                implicitHeight: 24
                                                background: Rectangle {
                                                    color: parent.hovered ? Theme.bgHover : Theme.bgInput
                                                    radius: Theme.radiusSmall
                                                    border.color: Theme.borderColor
                                                    border.width: 1
                                                }
                                                contentItem: Text {
                                                    text: parent.text
                                                    color: Theme.textSecondary
                                                    font.pixelSize: Theme.fontSizeCaption
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                            }
                                        }

                                        // 增强参数
                                        ParamRow {
                                            label: "平移"
                                            labelWidth: 80
                                            Layout.fillWidth: true
                                            enabled: root.augmentationEnabled
                                            Stepper {
                                                id: augTranslateStepper
                                                value: 10
                                                minValue: 0
                                                maxValue: 100
                                                stepSize: 5
                                                suffix: "%"
                                            }
                                        }

                                        ParamRow {
                                            label: "旋转"
                                            labelWidth: 80
                                            Layout.fillWidth: true
                                            enabled: root.augmentationEnabled
                                            Stepper {
                                                id: augRotateStepper
                                                value: 0
                                                minValue: -180
                                                maxValue: 180
                                                stepSize: 5
                                                suffix: "°"
                                            }
                                        }

                                        ParamRow {
                                            label: "缩放"
                                            labelWidth: 80
                                            Layout.fillWidth: true
                                            enabled: root.augmentationEnabled
                                            Stepper {
                                                id: augScaleStepper
                                                value: 50
                                                minValue: 0
                                                maxValue: 100
                                                stepSize: 5
                                                suffix: "%"
                                            }
                                        }

                                        ParamRow {
                                            label: "水平翻转"
                                            labelWidth: 80
                                            Layout.fillWidth: true
                                            enabled: root.augmentationEnabled
                                            ToggleSwitch {
                                                id: augFliplrSwitch
                                                checked: true
                                            }
                                        }

                                        ParamRow {
                                            label: "垂直翻转"
                                            labelWidth: 80
                                            Layout.fillWidth: true
                                            enabled: root.augmentationEnabled
                                            ToggleSwitch {
                                                id: augFlipudSwitch
                                                checked: false
                                            }
                                        }

                                        ParamRow {
                                            label: "亮度"
                                            labelWidth: 80
                                            Layout.fillWidth: true
                                            enabled: root.augmentationEnabled
                                            Stepper {
                                                id: augBrightnessStepper
                                                value: 20
                                                minValue: 0
                                                maxValue: 100
                                                stepSize: 5
                                                suffix: "%"
                                            }
                                        }

                                        ParamRow {
                                            label: "色调"
                                            labelWidth: 80
                                            Layout.fillWidth: true
                                            enabled: root.augmentationEnabled
                                            Stepper {
                                                id: augHueStepper
                                                value: 5
                                                minValue: 0
                                                maxValue: 100
                                                stepSize: 5
                                                suffix: "%"
                                            }
                                        }

                                        ParamRow {
                                            label: "饱和度"
                                            labelWidth: 80
                                            Layout.fillWidth: true
                                            enabled: root.augmentationEnabled
                                            Stepper {
                                                id: augSaturationStepper
                                                value: 30
                                                minValue: 0
                                                maxValue: 100
                                                stepSize: 5
                                                suffix: "%"
                                            }
                                        }

                                        ParamRow {
                                            label: "Mosaic"
                                            labelWidth: 80
                                            Layout.fillWidth: true
                                            enabled: root.augmentationEnabled
                                            ToggleSwitch {
                                                id: augMosaicSwitch
                                                checked: true
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // ====== 子标签1：实时训练结果 ======
                Item {
                    id: resultsPage

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingNormal
                        spacing: Theme.spacingNormal

                        // === 左侧状态面板(240px) ===
                        Rectangle {
                            Layout.preferredWidth: Theme.sidebarWidth
                            Layout.fillHeight: true
                            color: Theme.bgCard
                            radius: Theme.radiusNormal

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingNormal
                                spacing: Theme.spacingNormal

                                // 训练状态卡片
                                SectionTitle {
                                    text: "训练状态"
                                    Layout.fillWidth: true
                                }

                                // Epoch
                                ParamRow {
                                    label: "Epoch"
                                    labelWidth: 60
                                    Layout.fillWidth: true
                                    Text {
                                        id: statusEpochText
                                        anchors.fill: parent
                                        verticalAlignment: Text.AlignVCenter
                                        text: "0 / 0"
                                        color: Theme.textMain
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.family: Theme.fontFamilyMono
                                    }
                                }

                                // 学习率
                                ParamRow {
                                    label: "学习率"
                                    labelWidth: 60
                                    Layout.fillWidth: true
                                    Text {
                                        id: statusLrText
                                        anchors.fill: parent
                                        verticalAlignment: Text.AlignVCenter
                                        text: "0.000000"
                                        color: Theme.textMain
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.family: Theme.fontFamilyMono
                                    }
                                }

                                // 损失
                                ParamRow {
                                    label: "损失"
                                    labelWidth: 60
                                    Layout.fillWidth: true
                                    Text {
                                        id: statusLossText
                                        anchors.fill: parent
                                        verticalAlignment: Text.AlignVCenter
                                        text: "—"
                                        color: Theme.chartBoxLoss
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.family: Theme.fontFamilyMono
                                        font.bold: true
                                    }
                                }

                                // 进度条
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 6
                                    radius: 3
                                    color: Theme.bgInput

                                    Rectangle {
                                        id: statusProgressBar
                                        property real value: 0
                                        width: parent.width * value
                                        height: parent.height
                                        radius: 3
                                        gradient: Gradient {
                                            orientation: Gradient.Horizontal
                                            GradientStop { position: 0.0; color: Theme.primary }
                                            GradientStop { position: 1.0; color: Theme.primaryGlow }
                                        }
                                    }
                                }

                                // 分割线
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 1
                                    color: Theme.dividerColor
                                }

                                // 评估状态卡片
                                SectionTitle {
                                    text: "评估状态"
                                    Layout.fillWidth: true
                                }

                                // 精度指标
                                ParamRow {
                                    label: metricName
                                    labelWidth: 60
                                    Layout.fillWidth: true
                                    Text {
                                        id: statusMetricText
                                        anchors.fill: parent
                                        verticalAlignment: Text.AlignVCenter
                                        text: "—"
                                        color: Theme.chartMap50B
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.family: Theme.fontFamilyMono
                                        font.bold: true
                                    }
                                }

                                // 最佳Epoch
                                ParamRow {
                                    label: "最佳Epoch"
                                    labelWidth: 60
                                    Layout.fillWidth: true
                                    Text {
                                        id: statusBestEpochText
                                        anchors.fill: parent
                                        verticalAlignment: Text.AlignVCenter
                                        text: "—"
                                        color: Theme.textMain
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.family: Theme.fontFamilyMono
                                    }
                                }

                                // 分割线
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 1
                                    color: Theme.dividerColor
                                }

                                // 日志/历史选项卡
                                TabBar {
                                    id: logTabBar
                                    Layout.fillWidth: true
                                    background: Rectangle { color: "transparent" }

                                    TabButton {
                                        text: "训练日志"
                                        font.pixelSize: Theme.fontSizeSmall
                                        contentItem: Text {
                                            text: parent.text
                                            color: parent.checked ? Theme.primaryGlow : Theme.textMuted
                                            font.pixelSize: Theme.fontSizeSmall
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
                                        font.pixelSize: Theme.fontSizeSmall
                                        contentItem: Text {
                                            text: parent.text
                                            color: parent.checked ? Theme.primaryGlow : Theme.textMuted
                                            font.pixelSize: Theme.fontSizeSmall
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            color: parent.checked ? Theme.bgInput : "transparent"
                                            radius: Theme.radiusSmall
                                        }
                                    }
                                }

                                // 日志/历史堆叠
                                StackLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    currentIndex: logTabBar.currentIndex

                                    LogView {
                                        id: logView
                                    }

                                    ListView {
                                        id: runHistoryList
                                        clip: true
                                        model: trainingModel
                                        spacing: 2

                                        Text {
                                            anchors.centerIn: parent
                                            visible: runHistoryList.count === 0
                                            text: "暂无训练记录"
                                            color: Theme.textMuted
                                            font.pixelSize: Theme.fontSizeNormal
                                        }

                                        delegate: Rectangle {
                                            width: runHistoryList.width
                                            height: 40
                                            radius: Theme.radiusSmall
                                            color: model.runId === currentRunId ? Qt.alpha(Theme.primaryGlow, 0.05) : "transparent"
                                            border.color: model.runId === currentRunId ? Theme.primaryGlow : "transparent"
                                            border.width: model.runId === currentRunId ? 1 : 0

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8
                                                spacing: 6

                                                // 状态圆点
                                                Rectangle {
                                                    width: 8
                                                    height: 8
                                                    radius: 4
                                                    color: {
                                                        switch (model.status) {
                                                        case "running": return Theme.warning
                                                        case "succeeded": return Theme.success
                                                        case "failed": return Theme.danger
                                                        case "cancelled": return Theme.textMuted
                                                        case "draft": return Theme.primary
                                                        default: return Theme.textMuted
                                                        }
                                                    }
                                                }

                                                Text {
                                                    text: model.runId.substring(0, 8) + "..."
                                                    color: Theme.primaryGlow
                                                    font.pixelSize: Theme.fontSizeCaption
                                                    font.family: Theme.fontFamilyMono
                                                }

                                                Text {
                                                    text: model.status
                                                    color: Theme.textMuted
                                                    font.pixelSize: Theme.fontSizeCaption
                                                }

                                                Item { Layout.fillWidth: true }

                                                // 删除按钮
                                                Text {
                                                    text: "删除"
                                                    visible: model.status === "draft" || model.status === "cancelled" || model.status === "failed"
                                                    color: deleteMouseArea.containsMouse ? Theme.danger : Theme.textMuted
                                                    font.pixelSize: Theme.fontSizeCaption

                                                    MouseArea {
                                                        id: deleteMouseArea
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            if (trainingService.deleteRun(model.runId)) {
                                                                trainingModel.refresh()
                                                                if (model.runId === currentRunId) {
                                                                    currentRunId = ""
                                                                    currentRunStatus = ""
                                                                    logView.clear()
                                                                    lossModel.clear()
                                                                    metricModel.clear()
                                                                    recallModel.clear()
                                                                    precisionModel.clear()
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    currentRunId = model.runId
                                                    currentRunStatus = model.status
                                                    logView.clear()
                                                    logView.appendLog("[LabelTorch] Run: " + model.runId)
                                                    logView.appendLog("[LabelTorch] Status: " + model.status)
                                                    logView.appendLog("[LabelTorch] Config: " + model.configJson)
                                                    logTabBar.currentIndex = 0
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // === 右侧图表区：4个可折叠卡片 ===
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: Theme.spacingSmall

                            // 上排：Loss + mAP
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: Theme.spacingSmall

                                // Loss 图表卡
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: Theme.bgChartPanel
                                    radius: Theme.radiusNormal
                                    border.color: Theme.borderColor
                                    border.width: 1

                                    CollapsibleSection {
                                        anchors.fill: parent
                                        anchors.margins: Theme.spacingSmall
                                        title: "损失 (Loss)"

                                        ColumnLayout {
                                            width: parent.width
                                            spacing: 0

                                            // 图例
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: Theme.spacingNormal

                                                Rectangle { width: 12; height: 3; radius: 1.5; color: Theme.chartBoxLoss }
                                                Text { text: "box_loss"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeCaption }

                                                Item { Layout.fillWidth: true }

                                                Text {
                                                    text: lossModel.count > 0 ?
                                                        "最新: " + lossModel.get(lossModel.count - 1).value.toFixed(4) : ""
                                                    color: Theme.chartBoxLoss
                                                    font.pixelSize: Theme.fontSizeCaption
                                                    font.family: Theme.fontFamilyMono
                                                }
                                            }

                                            Canvas {
                                                id: lossChartCanvas
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 160
                                                onPaint: {
                                                    var ctx = getContext("2d")
                                                    root.drawChart(ctx, width, height, lossModel, "Loss", Theme.chartBoxLoss, 0, false)
                                                }
                                            }
                                        }
                                    }
                                }

                                // mAP 图表卡
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: Theme.bgChartPanel
                                    radius: Theme.radiusNormal
                                    border.color: Theme.borderColor
                                    border.width: 1

                                    CollapsibleSection {
                                        anchors.fill: parent
                                        anchors.margins: Theme.spacingSmall
                                        title: metricName

                                        ColumnLayout {
                                            width: parent.width
                                            spacing: 0

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: Theme.spacingNormal

                                                Rectangle { width: 12; height: 3; radius: 1.5; color: Theme.chartMap50B }
                                                Text { text: metricName; color: Theme.textMuted; font.pixelSize: Theme.fontSizeCaption }

                                                Item { Layout.fillWidth: true }

                                                Text {
                                                    text: metricModel.count > 0 ?
                                                        "最新: " + metricModel.get(metricModel.count - 1).value.toFixed(4) : ""
                                                    color: Theme.chartMap50B
                                                    font.pixelSize: Theme.fontSizeCaption
                                                    font.family: Theme.fontFamilyMono
                                                }
                                            }

                                            Canvas {
                                                id: metricChartCanvas
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 160
                                                onPaint: {
                                                    var ctx = getContext("2d")
                                                    root.drawChart(ctx, width, height, metricModel, metricName, Theme.chartMap50B, deploymentThreshold, true)
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // 下排：召回率 + 精确率
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: Theme.spacingSmall

                                // 召回率图表卡
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: Theme.bgChartPanel
                                    radius: Theme.radiusNormal
                                    border.color: Theme.borderColor
                                    border.width: 1

                                    CollapsibleSection {
                                        anchors.fill: parent
                                        anchors.margins: Theme.spacingSmall
                                        title: "召回率 (Recall)"

                                        ColumnLayout {
                                            width: parent.width
                                            spacing: 0

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: Theme.spacingNormal

                                                Rectangle { width: 12; height: 3; radius: 1.5; color: Theme.chartRecallB }
                                                Text { text: "Recall"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeCaption }

                                                Item { Layout.fillWidth: true }

                                                Text {
                                                    text: recallModel.count > 0 ?
                                                        "最新: " + recallModel.get(recallModel.count - 1).value.toFixed(4) : ""
                                                    color: Theme.chartRecallB
                                                    font.pixelSize: Theme.fontSizeCaption
                                                    font.family: Theme.fontFamilyMono
                                                }
                                            }

                                            Canvas {
                                                id: recallChartCanvas
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 160
                                                onPaint: {
                                                    var ctx = getContext("2d")
                                                    root.drawChart(ctx, width, height, recallModel, "Recall", Theme.chartRecallB, 0, false)
                                                }
                                            }
                                        }
                                    }
                                }

                                // 精确率图表卡
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: Theme.bgChartPanel
                                    radius: Theme.radiusNormal
                                    border.color: Theme.borderColor
                                    border.width: 1

                                    CollapsibleSection {
                                        anchors.fill: parent
                                        anchors.margins: Theme.spacingSmall
                                        title: "精确率 (Precision)"

                                        ColumnLayout {
                                            width: parent.width
                                            spacing: 0

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: Theme.spacingNormal

                                                Rectangle { width: 12; height: 3; radius: 1.5; color: Theme.chartPrecisionB }
                                                Text { text: "Precision"; color: Theme.textMuted; font.pixelSize: Theme.fontSizeCaption }

                                                Item { Layout.fillWidth: true }

                                                Text {
                                                    text: precisionModel.count > 0 ?
                                                        "最新: " + precisionModel.get(precisionModel.count - 1).value.toFixed(4) : ""
                                                    color: Theme.chartPrecisionB
                                                    font.pixelSize: Theme.fontSizeCaption
                                                    font.family: Theme.fontFamilyMono
                                                }
                                            }

                                            Canvas {
                                                id: precisionChartCanvas
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 160
                                                onPaint: {
                                                    var ctx = getContext("2d")
                                                    root.drawChart(ctx, width, height, precisionModel, "Precision", Theme.chartPrecisionB, 0, false)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // === 创建数据快照对话框 ===
    Dialog {
        id: createSnapshotDialog
        title: "创建数据快照"
        modal: true
        anchors.centerIn: parent
        width: 420
        padding: 16

        onOpened: {
            datasetModel.setProjectId(currentProjectId)
            datasetModel.refresh()
        }

        background: Rectangle {
            color: Theme.bgCard
            border.color: Theme.borderColor
            radius: Theme.radiusNormal
        }

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingNormal

            Text {
                text: "从当前项目的数据集创建不可变快照，用于训练。"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            // 数据集选择
            ParamRow {
                label: "数据集"
                labelWidth: 60
                Layout.fillWidth: true

                ComboBox {
                    id: snapshotDatasetCombo
                    anchors.fill: parent
                    model: datasetModel
                    textRole: "name"
                    valueRole: "datasetId"

                    contentItem: Text {
                        text: datasetModel.rowCount() === 0 ?
                            "暂无数据集，请先导入" : snapshotDatasetCombo.displayText
                        color: datasetModel.rowCount() === 0 ?
                            Theme.textMuted : Theme.textMain
                        font.pixelSize: Theme.fontSizeSmall
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                        elide: Text.ElideRight
                    }

                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: snapshotDatasetCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                        border.width: 1
                    }

                    delegate: ItemDelegate {
                        width: snapshotDatasetCombo.width
                        contentItem: Text {
                            text: model.name + " (" + model.sampleCount + " 样本)"
                            color: highlighted ? Theme.primaryGlow : Theme.textMain
                            font.pixelSize: Theme.fontSizeCaption
                        }
                        highlighted: snapshotDatasetCombo.highlightedIndex === index
                        background: Rectangle { color: highlighted ? Theme.bgHover : Theme.bgMain }
                    }
                }
            }

            // 训练比例
            ParamRow {
                label: "训练比例"
                labelWidth: 60
                Layout.fillWidth: true

                Stepper {
                    id: trainRatioStepper
                    value: 80
                    minValue: 50
                    maxValue: 95
                    stepSize: 5
                    suffix: "%"
                }
            }

            // 划分策略
            ParamRow {
                label: "划分策略"
                labelWidth: 60
                Layout.fillWidth: true

                ComboBox {
                    id: splitStrategyCombo
                    anchors.fill: parent
                    model: ["random", "sequential"]
                    currentIndex: 0

                    contentItem: Text {
                        text: splitStrategyCombo.displayText === "random" ? "随机划分" : "顺序划分"
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeSmall
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                    }
                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: splitStrategyCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                        border.width: 1
                    }
                    delegate: ItemDelegate {
                        width: splitStrategyCombo.width
                        contentItem: Text {
                            text: modelData === "random" ? "随机划分" : "顺序划分"
                            color: highlighted ? Theme.primaryGlow : Theme.textMain
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        highlighted: splitStrategyCombo.highlightedIndex === index
                        background: Rectangle { color: highlighted ? Theme.bgHover : Theme.bgMain }
                    }
                }
            }

            // 按钮行
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingNormal

                Item { Layout.fillWidth: true }

                Button {
                    text: "取消"
                    font.family: Theme.fontFamily
                    background: Rectangle {
                        color: parent.hovered ? Theme.bgHover : Theme.bgInput
                        radius: Theme.radiusSmall
                        implicitHeight: 32
                    }
                    contentItem: Text {
                        text: parent.text
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: createSnapshotDialog.reject()
                }

                Button {
                    text: "创建快照"
                    font.family: Theme.fontFamily
                    background: Rectangle {
                        color: parent.hovered ? Theme.primary : Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: Theme.primary
                        border.width: 1
                        implicitHeight: 32
                    }
                    contentItem: Text {
                        text: parent.text
                        color: Theme.primary
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        var dsId = snapshotDatasetCombo.currentValue
                        if (!dsId || dsId === "") return
                        var ratio = trainRatioStepper.value / 100.0
                        var strategy = splitStrategyCombo.currentText
                        var snapId = snapshotService.createSnapshot(dsId, ratio, strategy)
                        if (snapId !== "") {
                            snapshotModel.setProjectId(currentProjectId)
                            snapshotModel.refresh()
                            // 尝试选中新创建的快照
                            for (var i = 0; i < snapshotModel.rowCount(); i++) {
                                var sid = snapshotModel.data(snapshotModel.index(i, 0), Qt.UserRole + 1)
                                if (sid === snapId) {
                                    snapshotCombo.currentIndex = i
                                    break
                                }
                            }
                            createSnapshotDialog.accept()
                        }
                    }
                }
            }
        }
    }

    // === 添加模型对话框 ===
    Dialog {
        id: addModelDialog
        title: "添加模型"
        modal: true
        anchors.centerIn: parent
        width: 420
        padding: 16

        background: Rectangle {
            color: Theme.bgCard
            border.color: Theme.borderColor
            radius: Theme.radiusNormal
        }

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingNormal

            // 模型名称
            ParamRow {
                label: "模型名称"
                labelWidth: 70
                Layout.fillWidth: true

                TextField {
                    id: modelNameInput
                    anchors.fill: parent
                    placeholderText: "输入模型名称"
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeSmall
                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: modelNameInput.activeFocus ? Theme.primaryGlow : Theme.borderColor
                        border.width: 1
                    }
                }
            }

            // 模型类型
            ParamRow {
                label: "模型类型"
                labelWidth: 70
                Layout.fillWidth: true

                ComboBox {
                    id: addModelTypeCombo
                    anchors.fill: parent
                    model: ["yolov8", "yolov8_obb", "yolov8_cls", "yolov11", "patchcore"]
                    currentIndex: 0

                    contentItem: Text {
                        text: addModelTypeCombo.displayText
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeSmall
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                    }
                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: addModelTypeCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                        border.width: 1
                    }
                    delegate: ItemDelegate {
                        width: addModelTypeCombo.width
                        contentItem: Text {
                            text: modelData
                            color: highlighted ? Theme.primaryGlow : Theme.textMain
                            font.pixelSize: Theme.fontSizeSmall
                        }
                        highlighted: addModelTypeCombo.highlightedIndex === index
                        background: Rectangle { color: highlighted ? Theme.bgHover : Theme.bgMain }
                    }
                }
            }

            // 导入权重
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingNormal

                ToggleSwitch {
                    id: importWeightSwitch
                    checked: false
                }

                Text {
                    text: "导入权重"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                }

                Item { Layout.fillWidth: true }

                TextField {
                    id: weightPathInput
                    visible: importWeightSwitch.checked
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    placeholderText: "权重文件路径"
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: Theme.fontFamilyMono
                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: weightPathInput.activeFocus ? Theme.primaryGlow : Theme.borderColor
                        border.width: 1
                    }
                }
            }

            // 按钮行
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingNormal

                Item { Layout.fillWidth: true }

                Button {
                    text: "取消"
                    font.family: Theme.fontFamily
                    background: Rectangle {
                        color: parent.hovered ? Theme.bgHover : Theme.bgInput
                        radius: Theme.radiusSmall
                        implicitHeight: 32
                    }
                    contentItem: Text {
                        text: parent.text
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSmall
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: addModelDialog.reject()
                }

                Button {
                    text: "确认添加"
                    font.family: Theme.fontFamily
                    background: Rectangle {
                        color: parent.hovered ? Theme.primary : Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: Theme.primary
                        border.width: 1
                        implicitHeight: 32
                    }
                    contentItem: Text {
                        text: parent.text
                        color: Theme.primary
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        // 设置模型类型到配置面板
                        var modelType = addModelTypeCombo.currentText
                        if (modelType === "patchcore") {
                            adapterCombo.currentIndex = adapterCombo.indexOfValue("anomalib")
                            root.applyModelFamilyOptions(root.anomalibModelFamilies, "patchcore")
                        } else {
                            adapterCombo.currentIndex = adapterCombo.indexOfValue("ultralytics")
                            root.applyModelFamilyOptions(root.ultralyticsModelFamilies, modelType)
                        }
                        addModelDialog.accept()
                    }
                }
            }
        }
    }
}
