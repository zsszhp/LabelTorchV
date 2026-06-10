// TestingPage.qml - V5 测试页：模型评估与结果查看
// 像素级复刻参考UI：左侧模型列表(240px) + 任务选择栏 + 3个可折叠卡片（设置/测试/测试详情）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LabelTorch.Theme
import LabelTorch.Components

Item {
    id: root
    anchors.fill: parent

    property string currentProjectId: appController.currentProjectId
    property string selectedTaskId: ""
    property string selectedModelVersionId: ""
    property var testMetrics: ({})
    property var confusionMatrix: ({})
    property var prCurveData: ([])
    property string testActionMessage: ""
    property string testActionTone: "neutral"
    property string currentTaskType: currentProjectId !== "" ? projectService.getTaskType(currentProjectId) : "detect"
    property bool isAnomalyProject: currentTaskType === "anomaly"
    property var environmentInfo: ({})
    property var availableDeviceOptions: {
        var options = ["auto", "cpu"]
        var gpuCount = environmentInfo.gpu_count || 0
        for (var index = 0; index < gpuCount; ++index) {
            options.push(String(index))
        }
        return options
    }
    property string primaryMetricLabel: isAnomalyProject ? "AUROC" : "mAP50"
    property real primaryMetricValue: isAnomalyProject ? (testMetrics.auroc || testMetrics.image_auroc || 0) : (testMetrics.mAP50 || 0)
    property string secondaryMetricLabel: isAnomalyProject ? "像素AUROC" : "mAP50-95"
    property real secondaryMetricValue: isAnomalyProject ? (testMetrics.pixel_auroc || 0) : (testMetrics["mAP50-95"] || 0)
    property string recallLikeLabel: isAnomalyProject ? "图像AUROC" : "召回率"
    property real recallLikeValue: isAnomalyProject ? (testMetrics.image_auroc || testMetrics.auroc || 0) : (testMetrics.recall || 0)
    property string precisionLikeLabel: isAnomalyProject ? "像素AUROC" : "精确率"
    property real precisionLikeValue: isAnomalyProject ? (testMetrics.pixel_auroc || 0) : (testMetrics.precision || 0)
    property string testStatusText: {
        switch (root.testStatus) {
            case "draft": return "草稿"
            case "preparing": return "准备中"
            case "running": return "测试中"
            case "succeeded": return "已完成"
            case "failed": return "失败"
            case "cancelled": return "已取消"
            default: return ""
        }
    }
    property string testStatusTone: {
        switch (root.testStatus) {
            case "running": return "info"
            case "succeeded": return "success"
            case "failed": return "danger"
            case "cancelled": return "neutral"
            case "draft": return "warning"
            case "preparing": return "warning"
            default: return "neutral"
        }
    }
    property string deviceHintText: {
        var hasCuda = environmentInfo.cuda_available === true
        var gpuName = environmentInfo.gpu_name || "GPU 信息未知"
        var gpuMemory = environmentInfo.gpu_memory_total_mb ? ("，显存约 " + environmentInfo.gpu_memory_total_mb + " MB") : ""
        if (deviceCombo.currentText === "cpu") return "当前使用 CPU 评估，耗时会明显增加"
        if (deviceCombo.currentText === "auto") {
            return hasCuda ? ("设备自动选择，当前环境可使用 " + gpuName + gpuMemory) : "设备自动选择，当前环境将回退到 CPU 评估"
        }
        return hasCuda ? ("当前优先使用 GPU " + deviceCombo.currentText + "，设备为 " + gpuName + gpuMemory) : ("当前指定 GPU " + deviceCombo.currentText + "，环境未检测到可用 CUDA，评估将回退到 CPU")
    }
    property string environmentSummaryText: {
        if (Object.keys(environmentInfo).length === 0)
            return "运行环境检测中..."
        if (environmentInfo.cuda_available === true) {
            var cudaVer = environmentInfo.torch_cuda || "?"
            var providerText = environmentInfo.onnxruntime_providers && environmentInfo.onnxruntime_providers.length > 0
                ? environmentInfo.onnxruntime_providers.join(", ") : "未检测到 ONNX Runtime Provider"
            return "CUDA " + cudaVer + " | " + (environmentInfo.gpu_name || "GPU 信息未知") + " | ONNX Runtime: " + providerText
        }
        return "当前未检测到可用 CUDA，评估将使用 CPU 执行"
    }
    property string memorySuggestionText: {
        if (environmentInfo.cuda_available !== true)
            return "CPU 路径下建议优先降低批量大小，避免评估耗时过长"
        var memoryMb = environmentInfo.gpu_memory_total_mb || 0
        if (memoryMb > 0 && memoryMb < 8192)
            return "当前显存偏小，建议先用较小 batch 进行评估"
        if (memoryMb > 0 && memoryMb < 12288)
            return "当前显存中等，评估时建议逐步增大 batch"
        return "当前显存条件较好，可按默认 batch 评估"
    }
    // 测试状态：idle/draft/preparing/running/succeeded/failed/cancelled
    property string testStatus: "idle"
    // 测试详情子视图索引：0=混淆矩阵, 1=检查图像
    property int detailViewIndex: 0
    // 检查图像相关属性
    property var checkImageList: []
    property string selectedCheckImagePath: ""
    property string selectedCheckImageName: ""

    // 测试参数别名
    property alias batchSize: batchSizeStepper.value
    property alias iouThreshold: iouThresholdStepper.value
    property alias confThreshold: confThresholdStepper.value
    property alias testDevice: deviceCombo.currentText
    property alias testWeight: weightCombo.currentIndex

    // 项目切换时刷新数据
    onCurrentProjectIdChanged: {
        if (currentProjectId !== "") {
            currentTaskType = projectService.getTaskType(currentProjectId)
            testingModel.setProjectId(currentProjectId)
            modelVersionModel.setProjectId(currentProjectId)
            snapshotModel.setProjectId(currentProjectId)
        }
    }

    // 页面可见时刷新
    onVisibleChanged: {
        if (visible && currentProjectId !== "") {
            testingModel.setProjectId(currentProjectId)
            modelVersionModel.setProjectId(currentProjectId)
            snapshotModel.setProjectId(currentProjectId)
        }
    }

    // 监听测试服务事件
    Connections {
        target: testingService
        function onTestTaskStatusChanged(taskId, status) {
            if (taskId === root.selectedTaskId) {
                root.testStatus = status
                if (status === "succeeded") {
                    loadTestResults(taskId)
                }
            }
            testingModel.refresh()
        }
        function onTestProgress(taskId, current, total, metrics) {
            if (taskId === root.selectedTaskId) {
                root.testMetrics = metrics
            }
        }
    }

    Connections {
        target: projectService
        function onTaskTypeChanged(projectId, taskType) {
            if (projectId === root.currentProjectId) {
                root.currentTaskType = taskType
            }
        }
    }

    Connections {
        target: ipcClient
        function onConnectedChanged() {
            if (ipcClient.connected) {
                ipcClient.sendRequest("environment.check", {})
            } else {
                root.environmentInfo = ({})
            }
        }
        function onResponseReceived(response) {
            if ((response.command || "") === "environment.check" && response.success) {
                root.environmentInfo = response.result || {}
            }
        }
    }

    Component.onCompleted: {
        if (ipcClient && ipcClient.connected) {
            ipcClient.sendRequest("environment.check", {})
        }
        if (currentProjectId !== "") {
            testingModel.setProjectId(currentProjectId)
            modelVersionModel.setProjectId(currentProjectId)
            snapshotModel.setProjectId(currentProjectId)
        }
    }

    // 加载测试结果
    function loadTestResults(taskId) {
        var results = testingService.getTestResults(taskId)
        if (results.taskId) {
            try {
                root.testMetrics = JSON.parse(results.metricsJson || "{}")
            } catch(e) { root.testMetrics = {} }
            try {
                root.confusionMatrix = JSON.parse(results.confusionMatrixJson || "{}")
            } catch(e) { root.confusionMatrix = {} }
            try {
                root.prCurveData = JSON.parse(results.prCurveJson || "[]")
            } catch(e) { root.prCurveData = [] }
            root.testStatus = results.status

            // 加载检查图像列表：从快照关联的数据集中获取测试集样本
            loadCheckImages(results.snapshotId)
        }
    }

    // 加载检查图像（从快照关联的数据集获取测试集样本）
    function loadCheckImages(snapshotId) {
        root.checkImageList = []
        if (!snapshotId) return

        // 从快照获取数据集ID
        var snapshotData = snapshotService.getSnapshot(snapshotId)
        if (!snapshotData || !snapshotData.datasetId) return

        // 从数据集获取测试集样本
        var samples = annotationService.listSamples(snapshotData.datasetId)
        var testSamples = []
        for (var i = 0; i < samples.length; i++) {
            var sample = samples[i]
            // 只取测试集样本
            if (sample.split === "val" || sample.split === "test") {
                testSamples.push({
                    "imagePath": sample.imagePath || "",
                    "labelPath": sample.labelPath || "",
                    "sampleId": sample.id || sample.sampleId || "",
                    "width": sample.width || 0,
                    "height": sample.height || 0,
                    "validationStatus": sample.validationStatus || ""
                })
            }
        }
        root.checkImageList = testSamples
    }

    // 创建新测试任务
    function createNewTestTask() {
        if (!root.selectedModelVersionId || !snapshotCombo.currentValue) return
        var config = JSON.stringify({
            "batch": batchSizeStepper.value,
            "iou_threshold": iouThresholdStepper.value,
            "conf_threshold": confThresholdStepper.value,
            "device": deviceCombo.currentText,
            "weight_index": weightCombo.currentIndex
        })
        var taskId = testingService.createTestTask(
            root.currentProjectId,
            root.selectedModelVersionId,
            snapshotCombo.currentValue,
            config
        )
        if (taskId !== "") {
            root.selectedTaskId = taskId
            testingModel.refresh()
        }
    }

    function validateTestStart() {
        if (!root.selectedModelVersionId)
            return {"ok": false, "message": "请先选择一个模型版本"}
        if (!snapshotCombo.currentValue)
            return {"ok": false, "message": "请先选择用于测试的数据快照"}
        if (Object.keys(environmentInfo).length === 0)
            return {"ok": false, "message": "运行环境尚未检测完成，请稍后再启动测试"}
        if (deviceCombo.currentText !== "auto" && deviceCombo.currentText !== "cpu" && environmentInfo.cuda_available !== true)
            return {"ok": false, "message": "当前选择了 GPU 设备，但运行环境未检测到可用 CUDA"}

        var memoryMb = environmentInfo.gpu_memory_total_mb || 0
        if (environmentInfo.cuda_available === true && memoryMb > 0 && batchSizeStepper.value >= 32 && memoryMb < 8192)
            return {"ok": false, "message": "当前显存较小，评估 batch 过高，建议降到 16 或更小"}

        return {"ok": true, "message": ""}
    }

    function startOrCreateTestTask() {
        var validation = validateTestStart()
        if (!validation.ok) {
            testActionMessage = validation.message
            testActionTone = "warning"
            return
        }

        if (root.testStatus === "running") {
            testingService.stopTestTask(root.selectedTaskId)
            testActionMessage = "已发送停止测试请求"
            testActionTone = "warning"
        } else if (root.selectedTaskId && root.testStatus === "draft") {
            if (testingService.startTestTask(root.selectedTaskId)) {
                testActionMessage = "测试任务已启动"
                testActionTone = "info"
            }
        } else {
            createNewTestTask()
            if (root.selectedTaskId && testingService.startTestTask(root.selectedTaskId)) {
                testActionMessage = "测试任务已创建并启动"
                testActionTone = "info"
            }
        }
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        handle: Rectangle {
            implicitWidth: 4
            color: SplitHandle.pressed ? Theme.primaryGlow : (SplitHandle.hovered ? Theme.primaryGlow : Theme.borderColor)
            Behavior on color { ColorAnimation { duration: 150 } }
        }

        // ========================================
        // 左侧模型列表 (240px, padding:0)
        // ========================================
        Rectangle {
            id: sidebarLeft
            SplitView.preferredWidth: 240
            SplitView.minimumWidth: 120
            SplitView.maximumWidth: 400
            color: Theme.bgSide

            // 右侧边线
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Theme.borderColor
            }

            ScrollView {
                anchors.fill: parent
                anchors.rightMargin: 1
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width - 1
                    spacing: 0

                // 区块标题 "模型列表" + 右侧"+"图标
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.topMargin: 16
                    spacing: Theme.spacingSmall

                    Text {
                        text: "模型列表"
                        font.pixelSize: Theme.fontSizeNormal
                        font.weight: Font.DemiBold
                        font.family: Theme.fontFamily
                        color: Theme.textMain
                    }

                    Item { Layout.fillWidth: true }

                    // "+" 图标按钮
                    Rectangle {
                        width: 20
                        height: 20
                        radius: Theme.radiusSmall
                        color: addModelMouse.hovered ? Theme.bgHover : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "+"
                            font.pixelSize: Theme.fontSizeNormal
                            font.weight: Font.Bold
                            font.family: Theme.fontFamily
                            color: Theme.primaryGlow
                        }

                        MouseArea {
                            id: addModelMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: importFileDialog.open()
                        }
                    }
                }

                // 模型版本列表
                ListView {
                    id: modelVersionList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.topMargin: 8
                    Layout.leftMargin: 0
                    Layout.rightMargin: 0
                    clip: true
                    spacing: 0

                    model: modelVersionModel
                    delegate: Rectangle {
                        id: listItem
                        width: modelVersionList.width
                        height: 48

                        // 选中态：左侧3px青色边线 + 背景高亮
                        Rectangle {
                            visible: root.selectedModelVersionId === model.versionId
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 3
                            color: Theme.primaryGlow
                        }

                        // 背景色
                        Rectangle {
                            anchors.fill: parent
                            anchors.leftMargin: 3
                            color: {
                                if (root.selectedModelVersionId === model.versionId)
                                    return Qt.alpha(Theme.primaryGlow, 0.05)
                                if (itemMouse.containsMouse)
                                    return Theme.bgHover
                                return "transparent"
                            }
                        }

                        // 内容
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            anchors.topMargin: 6
                            anchors.bottomMargin: 6
                            spacing: 2

                            Text {
                                text: {
                                    var name = model.bestWeightPath ? model.bestWeightPath.split("/").pop().split("\\").pop() : "模型 " + model.versionId.substring(0, 8)
                                    if (model.source === "imported") name = "[导入] " + name
                                    return name
                                }
                                font.pixelSize: Theme.fontSizeSmall
                                font.weight: Font.DemiBold
                                font.family: Theme.fontFamily
                                color: root.selectedModelVersionId === model.versionId ? Theme.primaryGlow : Theme.textMain
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: {
                                    try {
                                        var metrics = model.metricsJson ? JSON.parse(model.metricsJson) : {}
                                        if (root.isAnomalyProject) {
                                            if (metrics.auroc !== undefined) return "AUROC: " + (metrics.auroc * 100).toFixed(1) + "%"
                                        } else if (metrics.mAP50 !== undefined) {
                                            return "mAP50: " + (metrics.mAP50 * 100).toFixed(1) + "%"
                                        }
                                    } catch(e) {}
                                    return "未评估"
                                }
                                font.pixelSize: Theme.fontSizeCaption
                                font.family: Theme.fontFamily
                                color: Theme.textMuted
                                Layout.fillWidth: true
                            }
                        }

                        MouseArea {
                            id: itemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectedModelVersionId = model.versionId
                        }
                    }
                }

                // 底部按钮区
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.bottomMargin: 16
                    Layout.topMargin: 8
                    color: {
                        if (!root.selectedModelVersionId) return Theme.bgCard
                        if (startTestMouse.pressed) return Qt.darker(Theme.primary, 1.3)
                        if (startTestMouse.hovered) return Qt.lighter(Theme.primary, 1.1)
                        return Theme.primary
                    }
                    radius: Theme.radiusNormal

                    Row {
                        anchors.centerIn: parent
                        spacing: Theme.spacingSmall

                        Text {
                            text: root.testStatus === "running" ? "■" : "▶"
                            font.pixelSize: Theme.fontSizeSmall
                            font.family: Theme.fontFamily
                            color: root.selectedModelVersionId ? "#FFFFFF" : Theme.textDisabled
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: root.testStatus === "running" ? "停止测试" : "开始测试"
                            font.pixelSize: Theme.fontSizeNormal
                            font.weight: Font.DemiBold
                            font.family: Theme.fontFamily
                            color: root.selectedModelVersionId ? "#FFFFFF" : Theme.textDisabled
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    MouseArea {
                        id: startTestMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: root.selectedModelVersionId ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                        onClicked: {
                            root.startOrCreateTestTask()
                        }
                    }
                }
                }
            }
        }

        // ========================================
        // 右侧内容区
        // ========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgMain

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // === 任务选择栏 (40px) ===
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.filterBarHeight
                    color: Theme.bgSide

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Theme.borderColor
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingLarge
                        anchors.rightMargin: Theme.spacingLarge
                        spacing: Theme.spacingNormal

                        Text {
                            text: "测试任务"
                            font.pixelSize: Theme.fontSizeNormal
                            font.weight: Font.DemiBold
                            font.family: Theme.fontFamily
                            color: Theme.textMain
                        }

                        ComboBox {
                            id: taskCombo
                            Layout.preferredWidth: 160
                            Layout.alignment: Qt.AlignVCenter
                            textRole: "taskId"
                            valueRole: "taskId"
                            model: testingModel

                            delegate: ItemDelegate {
                                width: taskCombo.width
                                contentItem: Text {
                                    text: "测试 " + model.taskId.substring(0, 8) + " - " + model.status
                                    font.pixelSize: Theme.fontSizeSmall
                                    font.family: Theme.fontFamily
                                    color: Theme.textMain
                                }
                                background: Rectangle {
                                    color: parent.hovered ? Theme.bgHover : Theme.bgInputDropdown
                                }
                            }

                            onActivated: {
                                if (currentIndex >= 0) {
                                    root.selectedTaskId = currentValue
                                    loadTestResults(root.selectedTaskId)
                                }
                            }
                        }

                        // 新建测试任务 "+" 按钮
                        Rectangle {
                            width: 28
                            height: 28
                            radius: Theme.radiusSmall
                            color: addTaskMouse.hovered ? Theme.bgHover : Theme.bgCard
                            border.color: Theme.borderColor
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "+"
                                font.pixelSize: Theme.fontSizeNormal
                                font.weight: Font.Bold
                                color: Theme.primaryGlow
                            }

                            MouseArea {
                                id: addTaskMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: createNewTestTask()
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // 状态指示
                        StatusTag {
                            visible: root.testStatus !== "idle"
                            text: root.testStatusText
                            tone: root.testStatusTone
                            Layout.alignment: Qt.AlignVCenter
                        }

                        StatusTag {
                            visible: root.testActionMessage !== ""
                            text: root.testActionMessage
                            tone: root.testActionTone
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }

                // === 主内容区（3个CollapsibleSection卡片，可滚动） ===
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: Math.max(parent ? parent.width : 900, 900)
                        spacing: 12

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 0
                        anchors.topMargin: 12
                        anchors.bottomMargin: 12

                        // 左右内边距
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20

                        // ========================================
                        // Card 1: 设置
                        // ========================================
                        CollapsibleSection {
                            title: "设置"
                            Layout.fillWidth: true
                            expanded: true

                            GridLayout {
                                width: parent.width
                                columns: 3
                                rowSpacing: Theme.spacingNormal
                                columnSpacing: Theme.spacingLarge

                                // --- 列1: 数据集配置 ---
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSmall

                                    Text {
                                        text: "数据集配置"
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.weight: Font.DemiBold
                                        font.family: Theme.fontFamily
                                        color: Theme.textSecondary
                                    }

                                    ParamRow {
                                        label: "测试集"
                                        labelWidth: 60
                                        Layout.fillWidth: true

                                        ComboBox {
                                            id: snapshotCombo
                                            model: snapshotModel
                                            textRole: "snapshotId"
                                            valueRole: "snapshotId"
                                            Layout.fillWidth: true

                                            delegate: ItemDelegate {
                                                width: snapshotCombo.width
                                                contentItem: Text {
                                                    text: "快照 " + model.snapshotId.substring(0, 8) + " (" + model.sampleCount + "样本)"
                                                    font.pixelSize: Theme.fontSizeSmall
                                                    font.family: Theme.fontFamily
                                                    color: Theme.textMain
                                                    elide: Text.ElideRight
                                                }
                                                background: Rectangle {
                                                    color: parent.hovered ? Theme.bgHover : Theme.bgInputDropdown
                                                }
                                            }
                                        }
                                    }

                                    // 快照状态提示
                                    Text {
                                        text: snapshotCombo.currentValue ? "快照已选择" : "请选择数据快照"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: snapshotCombo.currentValue ? Theme.success : Theme.textMuted
                                    }
                                }

                                // --- 列2: 测试参数配置 ---
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSmall

                                    Text {
                                        text: "测试参数配置"
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.weight: Font.DemiBold
                                        font.family: Theme.fontFamily
                                        color: Theme.textSecondary
                                    }

                                    ParamRow {
                                        label: "批量大小"
                                        labelWidth: 80
                                        Layout.fillWidth: true
                                        Stepper {
                                            id: batchSizeStepper
                                            value: 16
                                            minValue: 1
                                            maxValue: 128
                                            stepSize: 4
                                        }
                                    }

                                    ParamRow {
                                        label: "IoU阈值"
                                        labelWidth: 80
                                        Layout.fillWidth: true
                                        Stepper {
                                            id: iouThresholdStepper
                                            value: 0.45
                                            minValue: 0.1
                                            maxValue: 0.95
                                            stepSize: 0.05
                                            decimals: 2
                                        }
                                    }

                                    ParamRow {
                                        label: "评估置信度"
                                        labelWidth: 80
                                        Layout.fillWidth: true
                                        Stepper {
                                            id: confThresholdStepper
                                            value: 0.25
                                            minValue: 0.01
                                            maxValue: 0.99
                                            stepSize: 0.05
                                            decimals: 2
                                        }
                                    }

                                    ParamRow {
                                        label: "设备"
                                        labelWidth: 80
                                        Layout.fillWidth: true
                                        ComboBox {
                                            id: deviceCombo
                                            model: root.availableDeviceOptions
                                            Layout.fillWidth: true
                                        }
                                    }

                                    Text {
                                        text: root.deviceHintText
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: deviceCombo.currentText === "cpu" ? Theme.warning : Theme.textMuted
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: root.environmentSummaryText
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamilyMono
                                        color: Theme.textMuted
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: root.memorySuggestionText
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: Theme.warning
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    ParamRow {
                                        label: "测试权重"
                                        labelWidth: 80
                                        Layout.fillWidth: true
                                        ComboBox {
                                            id: weightCombo
                                            model: ["最佳权重", "最末权重"]
                                            Layout.fillWidth: true
                                        }
                                    }
                                }

                                // --- 列3: 高级测试参数配置 ---
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSmall

                                    Text {
                                        text: "高级测试参数配置"
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.weight: Font.DemiBold
                                        font.family: Theme.fontFamily
                                        color: Theme.textSecondary
                                    }

                                    ParamRow {
                                        label: "分割阈值"
                                        labelWidth: 100
                                        Layout.fillWidth: true
                                        Stepper {
                                            value: 0.5
                                            minValue: 0.1
                                            maxValue: 0.99
                                            stepSize: 0.05
                                            decimals: 2
                                        }
                                    }

                                    ParamRow {
                                        label: "程度分类阈值"
                                        labelWidth: 100
                                        Layout.fillWidth: true
                                        Stepper {
                                            value: 0.5
                                            minValue: 0.1
                                            maxValue: 0.99
                                            stepSize: 0.05
                                            decimals: 2
                                        }
                                    }

                                    ParamRow {
                                        label: "输出置信度轮廓"
                                        labelWidth: 100
                                        Layout.fillWidth: true
                                        ComboBox {
                                            model: ["关闭", "开启"]
                                            Layout.fillWidth: true
                                        }
                                    }

                                    ParamRow {
                                        label: "融合IoU阈值"
                                        labelWidth: 100
                                        Layout.fillWidth: true
                                        Stepper {
                                            value: 0.7
                                            minValue: 0.1
                                            maxValue: 0.99
                                            stepSize: 0.05
                                            decimals: 2
                                        }
                                    }
                                }
                            }
                        }

                        // ========================================
                        // Card 2: 测试
                        // ========================================
                        CollapsibleSection {
                            title: "测试"
                            Layout.fillWidth: true
                            expanded: true

                            GridLayout {
                                width: parent.width
                                columns: 3
                                rowSpacing: Theme.spacingNormal
                                columnSpacing: Theme.spacingLarge

                                // --- 列1: 实例统计 ---
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingNormal

                                    Text {
                                        text: "实例统计"
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.weight: Font.DemiBold
                                        font.family: Theme.fontFamily
                                        color: Theme.textSecondary
                                    }

                                    // 漏检率 + 误检率 圆环
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignHCenter
                                        spacing: Theme.spacingXLarge

                                        // 漏检率
                                        RingProgress {
                                            value: root.isAnomalyProject ? root.primaryMetricValue * 100 : (1 - (root.testMetrics.recall || 0)) * 100
                                            ringColor: Theme.danger
                                            centerText: (root.isAnomalyProject ? root.primaryMetricValue * 100 : (1 - (root.testMetrics.recall || 0)) * 100).toFixed(1) + "%"
                                            labelText: root.isAnomalyProject ? root.primaryMetricLabel : "漏检率"
                                        }

                                        // 误检率
                                        RingProgress {
                                            value: root.isAnomalyProject ? root.secondaryMetricValue * 100 : (1 - (root.testMetrics.precision || 0)) * 100
                                            ringColor: Theme.warning
                                            centerText: (root.isAnomalyProject ? root.secondaryMetricValue * 100 : (1 - (root.testMetrics.precision || 0)) * 100).toFixed(1) + "%"
                                            labelText: root.isAnomalyProject ? root.secondaryMetricLabel : "误检率"
                                        }
                                    }

                                    // F1 分数
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.spacingNormal

                                        Text {
                                            text: root.isAnomalyProject ? "F1 分数" : "F1 分数"
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamily
                                            color: Theme.textMuted
                                        }

                                        Text {
                                            text: ((root.testMetrics.f1 || 0) * 100).toFixed(2) + "%"
                                            font.pixelSize: Theme.fontSizeNormal
                                            font.family: Theme.fontFamilyMono
                                            font.weight: Font.Bold
                                            color: Theme.primaryGlow
                                        }

                                        Item { Layout.fillWidth: true }
                                    }

                                    // 类别配置按钮
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 28
                                        color: classConfigMouse.hovered ? Theme.bgHover : Theme.bgCard
                                        border.color: Theme.borderColor
                                        border.width: 1
                                        radius: Theme.radiusSmall

                                        Text {
                                            anchors.centerIn: parent
                                            text: "类别配置"
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamily
                                            color: Theme.textSecondary
                                        }

                                        MouseArea {
                                            id: classConfigMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: classConfigDialog.open()
                                        }
                                    }
                                }

                                // --- 列2: 图像统计 ---
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingNormal

                                    Text {
                                        text: "图像统计"
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.weight: Font.DemiBold
                                        font.family: Theme.fontFamily
                                        color: Theme.textSecondary
                                    }

                                    // 超检率 / 召回率
                                    GridLayout {
                                        columns: 2
                                        columnSpacing: Theme.spacingNormal
                                        rowSpacing: Theme.spacingSmall
                                        Layout.fillWidth: true

                                        Text {
                                            text: root.isAnomalyProject ? root.precisionLikeLabel : "超检率"
                                            font.pixelSize: Theme.fontSizeSmall
                                            color: Theme.textMuted
                                        }
                                        Text {
                                            text: (root.isAnomalyProject ? (root.precisionLikeValue * 100) : ((1 - (root.testMetrics.precision || 0)) * 100)).toFixed(2) + "%"
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamilyMono
                                            color: Theme.textMain
                                        }

                                        Text {
                                            text: root.recallLikeLabel
                                            font.pixelSize: Theme.fontSizeSmall
                                            color: Theme.textMuted
                                        }
                                        Text {
                                            text: (root.recallLikeValue * 100).toFixed(2) + "%"
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamilyMono
                                            color: Theme.textMain
                                        }
                                    }

                                    // 分隔线
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 1
                                        color: Theme.dividerColor
                                    }

                                    // 检测时间 / 总共时间 / 已用时间
                                    GridLayout {
                                        columns: 2
                                        columnSpacing: Theme.spacingNormal
                                        rowSpacing: Theme.spacingSmall
                                        Layout.fillWidth: true

                                        Text {
                                            text: "检测时间"
                                            font.pixelSize: Theme.fontSizeSmall
                                            color: Theme.textMuted
                                        }
                                        Text {
                                            text: root.testMetrics.speed_inference ? root.testMetrics.speed_inference.toFixed(1) + "ms" : "N/A"
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamilyMono
                                            color: Theme.textMain
                                        }

                                        Text {
                                            text: "总共时间"
                                            font.pixelSize: Theme.fontSizeSmall
                                            color: Theme.textMuted
                                        }
                                        Text {
                                            text: root.testMetrics.speed_total ? root.testMetrics.speed_total.toFixed(1) + "ms" : "N/A"
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamilyMono
                                            color: Theme.textMain
                                        }

                                        Text {
                                            text: "已用时间"
                                            font.pixelSize: Theme.fontSizeSmall
                                            color: Theme.textMuted
                                        }
                                        Text {
                                            text: root.testMetrics.elapsed_time ? root.testMetrics.elapsed_time : "N/A"
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamilyMono
                                            color: Theme.textMain
                                        }
                                    }
                                }

                                // --- 列3: PR曲线 ---
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSmall

                                    Text {
                                        text: root.isAnomalyProject ? "评估曲线" : "PR曲线"
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.weight: Font.DemiBold
                                        font.family: Theme.fontFamily
                                        color: Theme.textSecondary
                                    }

                                    // PR曲线画布
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 200
                                        color: Theme.bgChart
                                        radius: Theme.radiusSmall
                                        border.color: Theme.borderColor
                                        border.width: 1

                                        Canvas {
                                            id: prCurveCanvas
                                            anchors.fill: parent
                                            anchors.margins: 4

                                            onPaint: {
                                                var ctx = getContext("2d")
                                                ctx.reset()

                                                var w = width
                                                var h = height
                                                var pad = 36
                                                var plotW = w - pad * 2
                                                var plotH = h - pad * 2

                                                // 网格线
                                                ctx.strokeStyle = Theme.chartGridLine
                                                ctx.lineWidth = 0.5
                                                for (var i = 0; i <= 5; i++) {
                                                    var x = pad + (plotW * i / 5)
                                                    var y = pad + (plotH * i / 5)
                                                    ctx.beginPath()
                                                    ctx.moveTo(x, pad)
                                                    ctx.lineTo(x, pad + plotH)
                                                    ctx.stroke()
                                                    ctx.beginPath()
                                                    ctx.moveTo(pad, y)
                                                    ctx.lineTo(pad + plotW, y)
                                                    ctx.stroke()
                                                }

                                                // 坐标轴标签
                                                ctx.fillStyle = Theme.textMuted
                                                ctx.font = Theme.fontSizeCaption + "px " + Theme.fontFamily
                                                ctx.textAlign = "center"
                                                ctx.fillText(root.isAnomalyProject ? "Score" : "Recall", w / 2, h - 4)
                                                ctx.save()
                                                ctx.translate(10, h / 2)
                                                ctx.rotate(-Math.PI / 2)
                                                ctx.fillText(root.isAnomalyProject ? "AUROC" : "Precision", 0, 0)
                                                ctx.restore()

                                                // 轴刻度
                                                ctx.textAlign = "center"
                                                for (var i = 0; i <= 5; i++) {
                                                    ctx.fillText((i * 0.2).toFixed(1), pad + plotW * i / 5, pad + plotH + 12)
                                                }
                                                ctx.textAlign = "right"
                                                for (var i = 0; i <= 5; i++) {
                                                    ctx.fillText((1 - i * 0.2).toFixed(1), pad - 4, pad + plotH * i / 5 + 4)
                                                }

                                                // PR曲线数据
                                                var data = root.prCurveData
                                                if (data.length > 0) {
                                                    ctx.beginPath()
                                                    ctx.strokeStyle = Theme.primaryGlow
                                                    ctx.lineWidth = 2
                                                    for (var i = 0; i < data.length; i++) {
                                                        var px = pad + (data[i].recall || 0) * plotW
                                                        var py = pad + (1 - (data[i].precision || 0)) * plotH
                                                        if (i === 0) ctx.moveTo(px, py)
                                                        else ctx.lineTo(px, py)
                                                    }
                                                    ctx.stroke()

                                                    // 填充区域
                                                    ctx.lineTo(pad + plotW, pad)
                                                    ctx.lineTo(pad, pad)
                                                    ctx.closePath()
                                                    ctx.fillStyle = Qt.alpha(Theme.primaryGlow, 0.1)
                                                    ctx.fill()
                                                } else if (root.isAnomalyProject) {
                                                    ctx.fillStyle = Theme.textMuted
                                                    ctx.font = Theme.fontSizeNormal + "px " + Theme.fontFamily
                                                    ctx.textAlign = "center"
                                                    ctx.fillText("异常检测当前显示摘要指标", w / 2, h / 2 - 12)
                                                    ctx.fillText(root.primaryMetricLabel + ": " + (root.primaryMetricValue * 100).toFixed(2) + "%", w / 2, h / 2 + 12)
                                                } else {
                                                    // 无数据提示
                                                    ctx.fillStyle = Theme.textMuted
                                                    ctx.font = Theme.fontSizeNormal + "px " + Theme.fontFamily
                                                    ctx.textAlign = "center"
                                                    ctx.fillText("暂无PR曲线数据", w / 2, h / 2)
                                                }
                                            }

                                            onVisibleChanged: if (visible) requestPaint()
                                            Connections {
                                                target: root
                                                function onPrCurveDataChanged() { prCurveCanvas.requestPaint() }
                                            }
                                        }

                                        // "复制"按钮
                                        Rectangle {
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.margins: 6
                                            width: 48
                                            height: 22
                                            radius: Theme.radiusSmall
                                            color: copyPrMouse.hovered ? Theme.bgHover : Qt.alpha(Theme.bgCard, 0.8)
                                            border.color: Theme.borderColor
                                            border.width: 1

                                            Text {
                                                anchors.centerIn: parent
                                                text: "复制"
                                                font.pixelSize: Theme.fontSizeCaption
                                                font.family: Theme.fontFamily
                                                color: Theme.textMuted
                                            }

                                            MouseArea {
                                                id: copyPrMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    // 复制PR曲线数据到剪贴板
                                                    var dataStr = JSON.stringify(root.prCurveData, null, 2)
                                                    clipboardHelper.setText(dataStr)
                                                    root.testActionMessage = "PR曲线数据已复制到剪贴板"
                                                    root.testActionTone = "success"
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // ========================================
                        // Card 3: 测试详情
                        // ========================================
                        CollapsibleSection {
                            title: "测试详情"
                            Layout.fillWidth: true
                            expanded: true

                            ColumnLayout {
                                width: parent.width
                                spacing: 0

                                    // 内部toolbar (38px)
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 38
                                        color: Theme.bgSide
                                        radius: Theme.radiusSmall

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: Theme.spacingNormal
                                            anchors.rightMargin: Theme.spacingNormal
                                            spacing: Theme.spacingSmall

                                            // 视图切换 tab
                                            Repeater {
                                                model: ["混淆矩阵", "检查图像"]

                                                delegate: Rectangle {
                                                    width: 80
                                                    height: 26
                                                    radius: Theme.radiusSmall
                                                    color: root.detailViewIndex === index ? Qt.alpha(Theme.primaryGlow, 0.15) : "transparent"
                                                    border.color: root.detailViewIndex === index ? Theme.primaryGlow : Theme.borderColor
                                                    border.width: 1

                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: modelData
                                                        font.pixelSize: Theme.fontSizeSmall
                                                        font.family: Theme.fontFamily
                                                        color: root.detailViewIndex === index ? Theme.primaryGlow : Theme.textMuted
                                                    }

                                                    MouseArea {
                                                        anchors.fill: parent
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: root.detailViewIndex = index
                                                    }
                                                }
                                            }

                                            Item { Layout.fillWidth: true }

                                            // 工具按钮
                                            Rectangle {
                                                width: 60
                                                height: 26
                                                radius: Theme.radiusSmall
                                                color: exportDetailMouse.hovered ? Theme.bgHover : Theme.bgCard
                                                border.color: Theme.borderColor
                                                border.width: 1

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "导出"
                                                    font.pixelSize: Theme.fontSizeSmall
                                                    font.family: Theme.fontFamily
                                                    color: Theme.textSecondary
                                                }

                                                MouseArea {
                                                    id: exportDetailMouse
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        // 导出测试结果为JSON文件
                                                        if (!root.selectedTaskId) {
                                                            root.testActionMessage = "请先选择测试任务"
                                                            root.testActionTone = "warning"
                                                            return
                                                        }
                                                        var results = testingService.getTestResults(root.selectedTaskId)
                                                        if (!results.taskId) {
                                                            root.testActionMessage = "无测试结果可导出"
                                                            root.testActionTone = "warning"
                                                            return
                                                        }
                                                        var exportData = {
                                                            "taskId": results.taskId,
                                                            "projectId": results.projectId,
                                                            "modelVersionId": results.modelVersionId,
                                                            "snapshotId": results.snapshotId,
                                                            "status": results.status,
                                                            "metrics": root.testMetrics,
                                                            "confusionMatrix": root.confusionMatrix,
                                                            "prCurve": root.prCurveData,
                                                            "createdAt": results.createdAt,
                                                            "startedAt": results.startedAt,
                                                            "finishedAt": results.finishedAt
                                                        }
                                                        var jsonStr = JSON.stringify(exportData, null, 2)
                                                        clipboardHelper.setText(jsonStr)
                                                        root.testActionMessage = "测试结果已复制到剪贴板（JSON格式）"
                                                        root.testActionTone = "success"
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // 3列内容区
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.minimumHeight: 300
                                        spacing: 0

                                        // --- 列1 (220px): 混淆矩阵表格 / 检查图像列表 ---
                                        Rectangle {
                                            Layout.preferredWidth: 220
                                            Layout.fillHeight: true
                                            color: Theme.bgCard
                                            border.color: Theme.borderColor
                                            border.width: 1

                                            StackLayout {
                                                id: detailLeftStack
                                                anchors.fill: parent
                                                anchors.margins: 4
                                                currentIndex: root.detailViewIndex

                                                // 混淆矩阵表格
                                                ScrollView {
                                                    clip: true
                                                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                                    Canvas {
                                                        id: confusionMatrixCanvas
                                                        width: detailLeftStack.width - 8
                                                        height: Math.max(detailLeftStack.height - 8, 200)

                                                        onPaint: {
                                                            var ctx = getContext("2d")
                                                            ctx.reset()

                                                            var w = width
                                                            var h = height
                                                            var cm = root.confusionMatrix
                                                            var matrix = cm.matrix || []
                                                            var names = cm.names || []

                                                            if (matrix.length === 0) {
                                                                ctx.fillStyle = Theme.textMuted
                                                                ctx.font = Theme.fontSizeSmall + "px " + Theme.fontFamily
                                                                ctx.textAlign = "center"
                                                                ctx.fillText("暂无混淆矩阵数据", w / 2, h / 2)
                                                                return
                                                            }

                                                            var n = matrix.length
                                                            var cellSize = Math.min((w - 40) / n, (h - 40) / n, 40)
                                                            var startX = (w - n * cellSize) / 2
                                                            var startY = (h - n * cellSize) / 2

                                                            // 找最大值用于归一化
                                                            var maxVal = 0
                                                            for (var i = 0; i < n; i++) {
                                                                for (var j = 0; j < n; j++) {
                                                                    if (matrix[i][j] > maxVal) maxVal = matrix[i][j]
                                                                }
                                                            }

                                                            // 绘制单元格
                                                            for (var i = 0; i < n; i++) {
                                                                for (var j = 0; j < n; j++) {
                                                                    var val = matrix[i][j]
                                                                    var ratio = maxVal > 0 ? val / maxVal : 0
                                                                    var x = startX + j * cellSize
                                                                    var y = startY + i * cellSize

                                                                    // 对角线绿色，非对角线红色
                                                                    if (i === j) {
                                                                        ctx.fillStyle = Qt.alpha(Theme.success, ratio * 0.8 + 0.05)
                                                                    } else {
                                                                        ctx.fillStyle = Qt.alpha(Theme.danger, ratio * 0.8 + 0.05)
                                                                    }
                                                                    ctx.fillRect(x, y, cellSize - 1, cellSize - 1)

                                                                    // 数值
                                                                    if (cellSize > 20) {
                                                                        ctx.fillStyle = ratio > 0.5 ? "#FFFFFF" : Theme.textMain
                                                                        ctx.font = Math.min(Theme.fontSizeCaption, cellSize * 0.35) + "px " + Theme.fontFamilyMono
                                                                        ctx.textAlign = "center"
                                                                        ctx.fillText(val, x + cellSize / 2, y + cellSize / 2 + 4)
                                                                    }
                                                                }
                                                            }

                                                            // 行/列标签
                                                            if (names.length > 0 && cellSize > 16) {
                                                                ctx.fillStyle = Theme.textMuted
                                                                ctx.font = Theme.fontSizeCaption + "px " + Theme.fontFamily
                                                                ctx.textAlign = "right"
                                                                for (var i = 0; i < Math.min(n, names.length); i++) {
                                                                    ctx.fillText(names[i], startX - 4, startY + i * cellSize + cellSize / 2 + 4)
                                                                }
                                                                ctx.textAlign = "center"
                                                                for (var j = 0; j < Math.min(n, names.length); j++) {
                                                                    ctx.save()
                                                                    ctx.translate(startX + j * cellSize + cellSize / 2, startY - 4)
                                                                    ctx.rotate(-Math.PI / 4)
                                                                    ctx.fillText(names[j], 0, 0)
                                                                    ctx.restore()
                                                                }
                                                            }
                                                        }

                                                        onVisibleChanged: if (visible) requestPaint()
                                                        Connections {
                                                            target: root
                                                            function onConfusionMatrixChanged() { confusionMatrixCanvas.requestPaint() }
                                                        }
                                                    }
                                                }

                                                // 检查图像列表
                                                ColumnLayout {
                                                    spacing: 0

                                                    // 标题栏
                                                    Rectangle {
                                                        Layout.fillWidth: true
                                                        Layout.preferredHeight: 28
                                                        color: Theme.bgSide

                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: "检查图像 (" + root.checkImageList.length + ")"
                                                            font.pixelSize: Theme.fontSizeCaption
                                                            font.font.weight: Font.DemiBold
                                                            font.family: Theme.fontFamily
                                                            color: Theme.textSecondary
                                                        }
                                                    }

                                                    // 图像列表
                                                    ListView {
                                                        id: checkImageListView
                                                        Layout.fillWidth: true
                                                        Layout.fillHeight: true
                                                        clip: true
                                                        spacing: 2

                                                        model: root.checkImageList
                                                        delegate: Rectangle {
                                                            width: checkImageListView.width
                                                            height: 32
                                                            color: {
                                                                if (root.selectedCheckImagePath === modelData.imagePath)
                                                                    return Qt.alpha(Theme.primaryGlow, 0.1)
                                                                if (checkItemMouse.containsMouse)
                                                                    return Theme.bgHover
                                                                return "transparent"
                                                            }

                                                            // 选中指示条
                                                            Rectangle {
                                                                visible: root.selectedCheckImagePath === modelData.imagePath
                                                                anchors.left: parent.left
                                                                anchors.top: parent.top
                                                                anchors.bottom: parent.bottom
                                                                width: 2
                                                                color: Theme.primaryGlow
                                                            }

                                                            Text {
                                                                anchors.fill: parent
                                                                anchors.leftMargin: 8
                                                                anchors.rightMargin: 8
                                                                text: {
                                                                    // 从路径提取文件名
                                                                    var path = modelData.imagePath || ""
                                                                    return path.split("/").pop().split("\\").pop()
                                                                }
                                                                font.pixelSize: Theme.fontSizeCaption
                                                                font.family: Theme.fontFamily
                                                                color: root.selectedCheckImagePath === modelData.imagePath ? Theme.primaryGlow : Theme.textMain
                                                                elide: Text.ElideRight
                                                                verticalAlignment: Text.AlignVCenter
                                                            }

                                                            MouseArea {
                                                                id: checkItemMouse
                                                                anchors.fill: parent
                                                                hoverEnabled: true
                                                                cursorShape: Qt.PointingHandCursor
                                                                onClicked: {
                                                                    root.selectedCheckImagePath = modelData.imagePath
                                                                    root.selectedCheckImageName = (modelData.imagePath || "").split("/").pop().split("\\").pop()
                                                                }
                                                            }
                                                        }

                                                        // 空状态提示
                                                        Text {
                                                            visible: root.checkImageList.length === 0
                                                            anchors.centerIn: parent
                                                            text: root.testStatus === "succeeded" ? "无测试集样本" : "请先完成测试"
                                                            font.pixelSize: Theme.fontSizeSmall
                                                            font.family: Theme.fontFamily
                                                            color: Theme.textMuted
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        // --- 列2 (flex): 图像预览区 ---
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            color: Theme.bgPreview
                                            border.color: Theme.borderColor
                                            border.width: 1

                                            // 有选中图像时显示图像
                                            Image {
                                                id: previewImage
                                                anchors.fill: parent
                                                anchors.margins: 4
                                                visible: root.selectedCheckImagePath !== ""
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                // 将本地路径转为file URL
                                                source: {
                                                    if (!root.selectedCheckImagePath) return ""
                                                    var path = root.selectedCheckImagePath
                                                    // Windows路径需要正斜杠
                                                    path = path.replace(/\\/g, "/")
                                                    if (!path.startsWith("file:///")) {
                                                        path = "file:///" + path
                                                    }
                                                    return path
                                                }

                                                // 加载中指示
                                                BusyIndicator {
                                                    anchors.centerIn: parent
                                                    running: previewImage.status === Image.Loading
                                                    visible: running
                                                }

                                                // 加载失败提示
                                                Text {
                                                    visible: previewImage.status === Image.Error
                                                    anchors.centerIn: parent
                                                    text: "图像加载失败"
                                                    font.pixelSize: Theme.fontSizeSmall
                                                    font.family: Theme.fontFamily
                                                    color: Theme.danger
                                                }
                                            }

                                            // 无选中图像时的占位提示
                                            Text {
                                                visible: root.selectedCheckImagePath === ""
                                                anchors.centerIn: parent
                                                text: root.checkImageList.length > 0 ? "请从左侧选择图像" : "图像预览区"
                                                font.pixelSize: Theme.fontSizeNormal
                                                font.family: Theme.fontFamily
                                                color: Theme.textDisabled
                                            }

                                            // 图像名称标签（底部叠加）
                                            Rectangle {
                                                visible: root.selectedCheckImagePath !== ""
                                                anchors.bottom: parent.bottom
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                height: 24
                                                color: Qt.alpha(Theme.bgMain, 0.85)

                                                Text {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 8
                                                    anchors.rightMargin: 8
                                                    text: root.selectedCheckImageName
                                                    font.pixelSize: Theme.fontSizeCaption
                                                    font.family: Theme.fontFamilyMono
                                                    color: Theme.textMuted
                                                    elide: Text.ElideMiddle
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                            }
                                        }

                                        // --- 列3 (220px): 实例详情面板 ---
                                        Rectangle {
                                            Layout.preferredWidth: 220
                                            Layout.fillHeight: true
                                            color: Theme.bgCard
                                            border.color: Theme.borderColor
                                            border.width: 1

                                            ColumnLayout {
                                                anchors.fill: parent
                                                anchors.margins: Theme.spacingNormal
                                                spacing: Theme.spacingSmall

                                                Text {
                                                    text: "实例详情"
                                                    font.pixelSize: Theme.fontSizeSmall
                                                    font.weight: Font.DemiBold
                                                    font.family: Theme.fontFamily
                                                    color: Theme.textSecondary
                                                }

                                                // 指标详情
                                                GridLayout {
                    columns: 2
                    columnSpacing: Theme.spacingNormal
                    rowSpacing: Theme.spacingSmall
                    Layout.fillWidth: true

                    Text {
                        text: root.primaryMetricLabel
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textMuted
                    }
                    Text {
                        text: (root.primaryMetricValue * 100).toFixed(2) + "%"
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamilyMono
                        font.weight: Font.Bold
                        color: Theme.primaryGlow
                    }

                    Text {
                        text: root.secondaryMetricLabel
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textMuted
                    }
                    Text {
                        text: (root.secondaryMetricValue * 100).toFixed(2) + "%"
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamilyMono
                        font.weight: Font.Bold
                        color: Theme.primaryGlow
                    }

                    Text {
                        text: "精确率"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textMuted
                    }
                    Text {
                        text: ((root.testMetrics.precision || 0) * 100).toFixed(2) + "%"
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamilyMono
                        color: Theme.textMain
                    }

                    Text {
                        text: "召回率"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textMuted
                    }
                    Text {
                        text: ((root.testMetrics.recall || 0) * 100).toFixed(2) + "%"
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamilyMono
                        color: Theme.textMain
                    }
                                                }

                                                Item { Layout.fillHeight: true }
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

    // 剪贴板辅助（隐藏TextEdit用于复制文本到剪贴板）
    TextEdit {
        id: clipboardHelper
        visible: false
        function setText(text) {
            clipboardHelper.text = text
            clipboardHelper.selectAll()
            clipboardHelper.copy()
            clipboardHelper.text = ""
        }
    }

    // ========================================
    // 导入外部模型 - 文件选择对话框
    // ========================================
    FileDialog {
        id: importFileDialog
        title: "选择外部模型文件"
        nameFilters: ["模型文件 (*.pt *.onnx *.engine)", "PyTorch 模型 (*.pt)", "ONNX 模型 (*.onnx)", "TensorRT 引擎 (*.engine)", "所有文件 (*)"]
        onAccepted: {
            var path = selectedFile.toString()
            // 移除 file:/// 前缀
            if (Qt.platform.os === "windows") {
                path = path.replace(/^file:\/\/\//, "")
            } else {
                path = path.replace(/^file:\/\//, "")
            }
            importModelDialog.selectedFilePath = path
            // 自动检测格式
            var ext = path.split(".").pop().toLowerCase()
            if (ext === "pt") importModelDialog.selectedFormat = "pt"
            else if (ext === "onnx") importModelDialog.selectedFormat = "onnx"
            else if (ext === "engine") importModelDialog.selectedFormat = "engine"
            // 自动填充模型名称为文件名（不含扩展名）
            var fileName = path.split("/").pop().split("\\").pop()
            importModelDialog.modelName = fileName.replace(/\.[^.]+$/, "")
            importModelDialog.open()
        }
    }

    // ========================================
    // 导入外部模型 - 配置对话框
    // ========================================
    Dialog {
        id: importModelDialog
        title: "导入外部模型"
        modal: true
        anchors.centerIn: parent
        width: 420
        padding: 24
        closePolicy: Popup.CloseOnEscape

        property string selectedFilePath: ""
        property string selectedFormat: "pt"
        property alias modelName: modelNameField.text

        background: Rectangle {
            color: Theme.bgCard
            radius: Theme.radiusNormal
            border.color: Theme.borderColor
            border.width: 1
        }

        header: Rectangle {
            color: "transparent"
            height: childrenRect.height + 16
            Text {
                text: "导入外部模型"
                font.pixelSize: Theme.fontSizeLarge
                font.weight: Font.Bold
                font.family: Theme.fontFamily
                color: Theme.textMain
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: 8
            }
        }

        contentItem: ColumnLayout {
            spacing: Theme.spacingNormal

            // 选择的文件路径
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                Text {
                    text: "文件路径："
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }

                Text {
                    text: importModelDialog.selectedFilePath.split("/").pop().split("\\").pop()
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: Theme.textMain
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                Text {
                    text: "更换"
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: Theme.fontFamily
                    color: Theme.primaryGlow
                    Layout.alignment: Qt.AlignRight

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: importFileDialog.open()
                    }
                }
            }

            // 模型名称
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: "模型名称"
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }

                TextField {
                    id: modelNameField
                    Layout.fillWidth: true
                    placeholderText: "输入模型名称"
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: Theme.bgInput
                        border.color: modelNameField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                        border.width: 1
                    }
                }
            }

            // 模型格式
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: "模型格式"
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }

                ComboBox {
                    id: importFormatCombo
                    Layout.fillWidth: true
                    model: [
                        { text: "PyTorch (.pt)", value: "pt" },
                        { text: "ONNX (.onnx)", value: "onnx" },
                        { text: "TensorRT (.engine)", value: "engine" }
                    ]
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: {
                        switch (importModelDialog.selectedFormat) {
                            case "pt": return 0
                            case "onnx": return 1
                            case "engine": return 2
                            default: return 0
                        }
                    }
                    contentItem: Text {
                        text: importFormatCombo.displayText
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily
                        color: Theme.textMain
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                    }
                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: Theme.bgInput
                        border.color: importFormatCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                        border.width: 1
                    }
                }
            }

            // 任务类型
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: "任务类型"
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }

                ComboBox {
                    id: importTaskTypeCombo
                    Layout.fillWidth: true
                    model: [
                        { text: "目标检测 (detect)", value: "detect" },
                        { text: "旋转框检测 (obb)", value: "obb" },
                        { text: "分类 (classify)", value: "classify" },
                        { text: "异常检测 (anomaly)", value: "anomaly" }
                    ]
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: {
                        switch (root.currentTaskType) {
                            case "detect": return 0
                            case "obb": return 1
                            case "classify": return 2
                            case "anomaly": return 3
                            default: return 0
                        }
                    }
                    contentItem: Text {
                        text: importTaskTypeCombo.displayText
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily
                        color: Theme.textMain
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                    }
                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: Theme.bgInput
                        border.color: importTaskTypeCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                        border.width: 1
                    }
                }
            }

            // 导入备注
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: "导入备注（可选）"
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }

                TextField {
                    id: importNotesField
                    Layout.fillWidth: true
                    placeholderText: "例如：供应商提供的预训练模型 v2.1"
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: Theme.bgInput
                        border.color: importNotesField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                        border.width: 1
                    }
                }
            }

            // 错误提示
            Text {
                id: importErrorText
                Layout.fillWidth: true
                visible: text !== ""
                font.pixelSize: Theme.fontSizeCaption
                font.family: Theme.fontFamily
                color: Theme.danger
                wrapMode: Text.WordWrap
            }
        }

        footer: RowLayout {
            spacing: Theme.spacingNormal

            Item { Layout.fillWidth: true }

            Button {
                text: "取消"
                onClicked: importModelDialog.reject()
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: parent.hovered ? Theme.bgHover : Theme.bgInput
                    border.color: Theme.borderColor
                    border.width: 1
                    implicitWidth: 80
                    implicitHeight: 32
                }
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: Theme.textMain
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                text: "导入"
                onClicked: {
                    importErrorText.text = ""
                    if (!importModelDialog.selectedFilePath) {
                        importErrorText.text = "请先选择模型文件"
                        return
                    }
                    if (!modelNameField.text.trim()) {
                        importErrorText.text = "请输入模型名称"
                        return
                    }

                    var versionId = modelRegistry.importExternalModel(
                        root.currentProjectId,
                        importModelDialog.selectedFilePath,
                        importFormatCombo.currentValue,
                        importTaskTypeCombo.currentValue,
                        modelNameField.text.trim(),
                        importNotesField.text.trim()
                    )

                    if (versionId && versionId !== "") {
                        importModelDialog.close()
                        modelVersionModel.setProjectId(root.currentProjectId)
                        root.selectedModelVersionId = versionId
                        root.testActionMessage = "模型导入成功"
                        root.testActionTone = "success"
                    } else {
                        importErrorText.text = "模型导入失败，请检查文件格式和路径"
                    }
                }
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: parent.pressed ? Qt.darker(Theme.primary, 1.3) : (parent.hovered ? Qt.lighter(Theme.primary, 1.1) : Theme.primary)
                    implicitWidth: 80
                    implicitHeight: 32
                }
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: Font.DemiBold
                    font.family: Theme.fontFamily
                    color: "#FFFFFF"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    // ========================================
    // 类别配置对话框
    // ========================================
    Dialog {
        id: classConfigDialog
        title: "类别配置"
        modal: true
        anchors.centerIn: parent
        width: 360
        padding: 24
        closePolicy: Popup.CloseOnEscape

        property var selectedClassIds: []

        background: Rectangle {
            color: Theme.bgCard
            radius: Theme.radiusNormal
            border.color: Theme.borderColor
            border.width: 1
        }

        header: Rectangle {
            color: "transparent"
            height: childrenRect.height + 16
            Text {
                text: "类别配置"
                font.pixelSize: Theme.fontSizeLarge
                font.weight: Font.Bold
                font.family: Theme.fontFamily
                color: Theme.textMain
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: 8
            }
        }

        contentItem: ColumnLayout {
            spacing: Theme.spacingNormal

            Text {
                text: "选择要在评估中包含的类别："
                font.pixelSize: Theme.fontSizeSmall
                font.family: Theme.fontFamily
                color: Theme.textMuted
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            // 类别列表
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 300)
                clip: true
                spacing: 4

                model: {
                    // 从混淆矩阵获取类别名
                    var names = root.confusionMatrix.names || []
                    if (names.length > 0) return names
                    // 从taxonomy获取类别
                    if (root.currentProjectId) {
                        var classes = taxonomyService.listClasses(root.currentProjectId)
                        return classes || []
                    }
                    return []
                }

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 32
                    radius: Theme.radiusSmall
                    color: classCheckMouse.containsMouse ? Theme.bgHover : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: Theme.spacingSmall

                        Rectangle {
                            width: 12
                            height: 12
                            radius: 2
                            color: classConfigDialog.selectedClassIds.indexOf(modelData) >= 0 ? Theme.primaryGlow : "transparent"
                            border.color: classConfigDialog.selectedClassIds.indexOf(modelData) >= 0 ? Theme.primaryGlow : Theme.borderColor
                            border.width: 1
                        }

                        Text {
                            text: typeof modelData === 'object' ? (modelData.name || JSON.stringify(modelData)) : modelData
                            font.pixelSize: Theme.fontSizeSmall
                            font.family: Theme.fontFamily
                            color: Theme.textMain
                            Layout.fillWidth: true
                        }
                    }

                    MouseArea {
                        id: classCheckMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var idx = classConfigDialog.selectedClassIds.indexOf(modelData)
                            var newIds = classConfigDialog.selectedClassIds.slice()
                            if (idx >= 0) {
                                newIds.splice(idx, 1)
                            } else {
                                newIds.push(modelData)
                            }
                            classConfigDialog.selectedClassIds = newIds
                        }
                    }
                }
            }

            Text {
                text: "不选择任何类别表示包含所有类别"
                font.pixelSize: Theme.fontSizeCaption
                font.family: Theme.fontFamily
                color: Theme.textMuted
                Layout.fillWidth: true
            }
        }

        footer: RowLayout {
            spacing: Theme.spacingNormal

            Item { Layout.fillWidth: true }

            Button {
                text: "关闭"
                onClicked: classConfigDialog.close()
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: parent.hovered ? Theme.bgHover : Theme.bgInput
                    border.color: Theme.borderColor
                    border.width: 1
                    implicitWidth: 80
                    implicitHeight: 32
                }
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: Theme.textMain
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
