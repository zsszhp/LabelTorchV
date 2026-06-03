// TestingPage.qml - V5 测试页：模型评估与结果查看
// 像素级复刻参考UI：左侧模型列表(240px) + 任务选择栏 + 3个可折叠卡片（设置/测试/测试详情）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
    // 测试状态：idle/draft/preparing/running/succeeded/failed/cancelled
    property string testStatus: "idle"
    // 测试详情子视图索引：0=混淆矩阵, 1=检查图像
    property int detailViewIndex: 0

    // 测试参数别名
    property alias batchSize: batchSizeStepper.value
    property alias iouThreshold: iouThresholdStepper.value
    property alias confThreshold: confThresholdStepper.value
    property alias testDevice: deviceCombo.currentText
    property alias testWeight: weightCombo.currentIndex

    // 项目切换时刷新数据
    onCurrentProjectIdChanged: {
        if (currentProjectId !== "") {
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
        }
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

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ========================================
        // 左侧模型列表 (240px, padding:0)
        // ========================================
        Rectangle {
            id: sidebarLeft
            Layout.preferredWidth: 240
            Layout.fillHeight: true
            color: Theme.bgSide

            // 右侧边线
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Theme.borderColor
            }

            ColumnLayout {
                anchors.fill: parent
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
                                text: model.bestWeightPath ? model.bestWeightPath.split("/").pop().split("\\").pop() : "模型 " + model.versionId.substring(0, 8)
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
                                        if (metrics.mAP50 !== undefined) return "mAP50: " + (metrics.mAP50 * 100).toFixed(1) + "%"
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
                            if (!root.selectedModelVersionId) return
                            if (root.testStatus === "running") {
                                testingService.stopTestTask(root.selectedTaskId)
                            } else if (root.selectedTaskId && root.testStatus === "draft") {
                                testingService.startTestTask(root.selectedTaskId)
                            } else {
                                createNewTestTask()
                                if (root.selectedTaskId) {
                                    testingService.startTestTask(root.selectedTaskId)
                                }
                            }
                        }
                    }
                }
            }
        }

        // 垂直分割线 (4px)
        Splitter {
            Layout.preferredWidth: 4
            Layout.fillHeight: true
            vertical: true
            minSize: 120
            maxSize: 400
            targetItem: sidebarLeft
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
                        Row {
                            spacing: Theme.spacingSmall
                            visible: root.testStatus !== "idle"
                            Layout.alignment: Qt.AlignVCenter

                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: {
                                    switch (root.testStatus) {
                                        case "running": return Theme.primaryGlow
                                        case "succeeded": return Theme.success
                                        case "failed": return Theme.danger
                                        case "cancelled": return Theme.textMuted
                                        default: return Theme.warning
                                    }
                                }
                            }

                            Text {
                                text: {
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
                                font.pixelSize: Theme.fontSizeCaption
                                font.family: Theme.fontFamily
                                color: Theme.textSecondary
                            }
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
                                            model: ["auto", "cpu", "0", "1"]
                                            Layout.fillWidth: true
                                        }
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
                                            value: (1 - (root.testMetrics.recall || 0)) * 100
                                            ringColor: Theme.danger
                                            centerText: ((1 - (root.testMetrics.recall || 0)) * 100).toFixed(1) + "%"
                                            labelText: "漏检率"
                                        }

                                        // 误检率
                                        RingProgress {
                                            value: (1 - (root.testMetrics.precision || 0)) * 100
                                            ringColor: Theme.warning
                                            centerText: ((1 - (root.testMetrics.precision || 0)) * 100).toFixed(1) + "%"
                                            labelText: "误检率"
                                        }
                                    }

                                    // F1 分数
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.spacingNormal

                                        Text {
                                            text: "F1 分数"
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
                                            text: "超检率"
                                            font.pixelSize: Theme.fontSizeSmall
                                            color: Theme.textMuted
                                        }
                                        Text {
                                            text: ((1 - (root.testMetrics.precision || 0)) * 100).toFixed(2) + "%"
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
                                        text: "PR曲线"
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
                                                ctx.fillText("Recall", w / 2, h - 4)
                                                ctx.save()
                                                ctx.translate(10, h / 2)
                                                ctx.rotate(-Math.PI / 2)
                                                ctx.fillText("Precision", 0, 0)
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

                                                    Text {
                                                        text: "检查图像功能将在后续版本中实现"
                                                        font.pixelSize: Theme.fontSizeSmall
                                                        font.family: Theme.fontFamily
                                                        color: Theme.textMuted
                                                        Layout.alignment: Qt.AlignHCenter
                                                        Layout.topMargin: Theme.spacingXLarge
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

                                            Text {
                                                anchors.centerIn: parent
                                                text: "图像预览区"
                                                font.pixelSize: Theme.fontSizeNormal
                                                font.family: Theme.fontFamily
                                                color: Theme.textDisabled
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
                        text: "mAP50"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textMuted
                    }
                    Text {
                        text: ((root.testMetrics.mAP50 || 0) * 100).toFixed(2) + "%"
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamilyMono
                        font.weight: Font.Bold
                        color: Theme.primaryGlow
                    }

                    Text {
                        text: "mAP50-95"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textMuted
                    }
                    Text {
                        text: ((root.testMetrics["mAP50-95"] || 0) * 100).toFixed(2) + "%"
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

    Component.onCompleted: {
        if (currentProjectId !== "") {
            testingModel.setProjectId(currentProjectId)
            modelVersionModel.setProjectId(currentProjectId)
            snapshotModel.setProjectId(currentProjectId)
        }
    }
}
