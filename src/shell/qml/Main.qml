import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Shell

ApplicationWindow {
    id: root
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    title: "标炬 LabelTorch"
    color: Theme.bgPrimary
    visible: true
    x: 100
    y: 100

    // 当前任务类型（检测/分割等）
    property string currentTaskType: "detect"
    // GPU 状态文本
    property string gpuStatusText: "GPU: 检测中..."
    // GPU 状态颜色
    property color gpuStatusColor: Theme.textMuted

    // 监听项目切换，更新任务类型
    Connections {
        target: appController
        function onCurrentProjectIdChanged() {
            if (appController.projectOpen) {
                root.currentTaskType = projectService.getTaskType(appController.currentProjectId)
            } else {
                root.currentTaskType = "detect"
            }
        }
    }

    // 监听 IPC 响应和事件
    Connections {
        target: ipcClient
        function onResponseReceived(response) {
            var cmd = response.command || ""
            if (response.success) {
                var result = response.result || {}
                // 处理环境检测结果
                if (result.cuda_available !== undefined) {
                    if (result.cuda_available) {
                        var gpuName = result.gpu_name || "Unknown GPU"
                        var cudaVer = result.cuda_version || result.torch_cuda || "?"
                        gpuStatusText = "GPU: " + gpuName + " (CUDA " + cudaVer + ")"
                        gpuStatusColor = Theme.accentSuccess
                    } else {
                        gpuStatusText = "GPU: 不可用 (仅CPU)"
                        gpuStatusColor = Theme.accentWarning
                    }
                    logPanel.appendLog("[环境] Python " + (result.python_version || result.torch_version || "?"))
                    logPanel.appendLog("[环境] PyTorch " + (result.torch_version || "?"))
                    logPanel.appendLog("[环境] Ultralytics " + (result.ultralytics_version || "?"))
                    logPanel.appendLog("[环境] CUDA " + (result.cuda_available ? "可用" : "不可用"))
                }
            }
        }
        // 后端连接状态变化
        function onConnectedChanged() {
            if (ipcClient.connected) {
                gpuStatusText = "GPU: 已连接，检测中..."
                gpuStatusColor = Theme.accentPrimary
                ipcClient.sendRequest("environment.check", {})
            } else {
                gpuStatusText = "Python 后端: 未连接"
                gpuStatusColor = Theme.accentError
            }
        }
        // 处理后端推送事件（训练进度等）
        function onEventReceived(event) {
            var eventType = event.event_type || ""
            var payload = event.payload || {}
            // 训练进度事件
            if (eventType === "task.progress") {
                var epoch = payload.epoch || 0
                var total = payload.total_epochs || 0
                var metrics = payload.metrics || {}
                var boxLoss = (typeof metrics.box_loss === 'number' && isFinite(metrics.box_loss)) ? metrics.box_loss.toFixed(4) : "?"
                var clsLoss = (typeof metrics.cls_loss === 'number' && isFinite(metrics.cls_loss)) ? metrics.cls_loss.toFixed(4) : "?"
                logPanel.appendLog("[训练] Epoch " + epoch + "/" + total +
                    " box_loss=" + boxLoss +
                    " cls_loss=" + clsLoss)
            // 训练完成事件
            } else if (eventType === "task.succeeded") {
                logPanel.appendLog("[训练] 训练完成! epochs=" + (payload.epochs_completed || "?") +
                    " early_stopped=" + (payload.early_stopped || false))
                if (payload.metrics) {
                    var map50 = (typeof payload.metrics.mAP50 === 'number' && isFinite(payload.metrics.mAP50)) ? payload.metrics.mAP50.toFixed(4) : "?"
                    var map5095 = (typeof payload.metrics["mAP50-95"] === 'number' && isFinite(payload.metrics["mAP50-95"])) ? payload.metrics["mAP50-95"].toFixed(4) : "?"
                    logPanel.appendLog("[训练] mAP50=" + map50 +
                        " mAP50-95=" + map5095)
                }
            // 训练失败事件
            } else if (eventType === "task.failed") {
                logPanel.appendLog("[训练] 训练失败: " + (payload.error || "未知错误"))
            }
        }
        // 后端错误
        function onBackendError(error) {
            logPanel.appendLog("[错误] " + error)
        }
    }

    // 任务类型切换回调
    function onTaskTypeChanged(taskType) {
        if (appController.projectOpen && taskType !== root.currentTaskType) {
            root.currentTaskType = taskType
            projectService.setTaskType(appController.currentProjectId, taskType)
        }
    }

    // 主布局：顶部 Tab 栏 + 内容区 + 日志面板
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 顶部标题栏（含 TabBar）
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: Theme.bgSecondary

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLarge
                anchors.rightMargin: Theme.spacingLarge
                spacing: Theme.spacingNormal

                // 应用标题
                Label {
                    text: "标炬 LabelTorch"
                    font.pixelSize: Theme.fontSizeLarge
                    font.bold: true
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    Layout.alignment: Qt.AlignVCenter
                }

                // 页面 Tab 栏
                TabBar {
                    id: tabBar
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    background: Rectangle { color: "transparent" }

                    // 自定义 TabBar 指示器（底部高亮线）
                    indicator: Rectangle {
                        y: parent.height - 2
                        width: parent.itemAt(tabBar.currentIndex) ? parent.itemAt(tabBar.currentIndex).width : 0
                        x: parent.itemAt(tabBar.currentIndex) ? parent.itemAt(tabBar.currentIndex).x : 0
                        height: 2
                        color: Theme.accentPrimary
                        radius: 1
                    }

                    // 项目管理
                    TabButton {
                        text: "项目管理"
                        enabled: true
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily
                        font.bold: tabBar.currentIndex === 0
                        // 选中时文字用强调色，未选中用次要文字色
                        contentItem: Label {
                            text: parent.text
                            font: parent.font
                            color: tabBar.currentIndex === 0 ? Theme.accentPrimary : Theme.textMuted
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        // 选中时背景用输入框色，未选中时透明
                        background: Rectangle {
                            color: tabBar.currentIndex === 0 ? Theme.bgInput : "transparent"
                            radius: Theme.radiusSmall
                        }
                        onClicked: appController.currentPage = "project"
                    }

                    // 类别体系
                    TabButton {
                        text: "类别体系"
                        enabled: true
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily
                        font.bold: tabBar.currentIndex === 1
                        contentItem: Label {
                            text: parent.text
                            font: parent.font
                            color: tabBar.currentIndex === 1 ? Theme.accentPrimary : Theme.textMuted
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: tabBar.currentIndex === 1 ? Theme.bgInput : "transparent"
                            radius: Theme.radiusSmall
                        }
                        onClicked: appController.currentPage = "taxonomy"
                    }

                    // 数据导入（需要打开项目）
                    TabButton {
                        text: "数据导入"
                        enabled: appController.projectOpen
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily
                        font.bold: tabBar.currentIndex === 2
                        contentItem: Label {
                            text: parent.text
                            font: parent.font
                            color: {
                                if (!parent.enabled) return Theme.textDisabled
                                return tabBar.currentIndex === 2 ? Theme.accentPrimary : Theme.textMuted
                            }
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: tabBar.currentIndex === 2 ? Theme.bgInput : "transparent"
                            radius: Theme.radiusSmall
                        }
                        onClicked: if (appController.projectOpen) appController.currentPage = "dataset"
                    }

                    // 标注工作台（需要打开项目）
                    TabButton {
                        text: "标注工作台"
                        enabled: appController.projectOpen
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily
                        font.bold: tabBar.currentIndex === 3
                        contentItem: Label {
                            text: parent.text
                            font: parent.font
                            color: {
                                if (!parent.enabled) return Theme.textDisabled
                                return tabBar.currentIndex === 3 ? Theme.accentPrimary : Theme.textMuted
                            }
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: tabBar.currentIndex === 3 ? Theme.bgInput : "transparent"
                            radius: Theme.radiusSmall
                        }
                        onClicked: if (appController.projectOpen) appController.currentPage = "annotation"
                    }

                    // 训练工作台（需要打开项目）
                    TabButton {
                        text: "训练工作台"
                        enabled: appController.projectOpen
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily
                        font.bold: tabBar.currentIndex === 4
                        contentItem: Label {
                            text: parent.text
                            font: parent.font
                            color: {
                                if (!parent.enabled) return Theme.textDisabled
                                return tabBar.currentIndex === 4 ? Theme.accentPrimary : Theme.textMuted
                            }
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: tabBar.currentIndex === 4 ? Theme.bgInput : "transparent"
                            radius: Theme.radiusSmall
                        }
                        onClicked: if (appController.projectOpen) appController.currentPage = "training"
                    }

                    // 版本中心（需要打开项目）
                    TabButton {
                        text: "版本中心"
                        enabled: appController.projectOpen
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily
                        font.bold: tabBar.currentIndex === 5
                        contentItem: Label {
                            text: parent.text
                            font: parent.font
                            color: {
                                if (!parent.enabled) return Theme.textDisabled
                                return tabBar.currentIndex === 5 ? Theme.accentPrimary : Theme.textMuted
                            }
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: tabBar.currentIndex === 5 ? Theme.bgInput : "transparent"
                            radius: Theme.radiusSmall
                        }
                        onClicked: if (appController.projectOpen) appController.currentPage = "model"
                    }

                    // 导出中心（需要打开项目）
                    TabButton {
                        text: "导出中心"
                        enabled: appController.projectOpen
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily
                        font.bold: tabBar.currentIndex === 6
                        contentItem: Label {
                            text: parent.text
                            font: parent.font
                            color: {
                                if (!parent.enabled) return Theme.textDisabled
                                return tabBar.currentIndex === 6 ? Theme.accentPrimary : Theme.textMuted
                            }
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: tabBar.currentIndex === 6 ? Theme.bgInput : "transparent"
                            radius: Theme.radiusSmall
                        }
                        onClicked: if (appController.projectOpen) appController.currentPage = "export"
                    }

                    // TabBar 当前索引跟随 appController.currentPage 同步
                    currentIndex: {
                        switch(appController.currentPage) {
                            case "project": return 0
                            case "taxonomy": return 1
                            case "dataset": return 2
                            case "annotation": return 3
                            case "training": return 4
                            case "model": return 5
                            case "export": return 6
                            default: return 0
                        }
                    }
                }

                // 任务类型切换器
                Loader {
                    visible: appController.projectOpen
                    Layout.alignment: Qt.AlignVCenter
                    source: "qrc:/qt/qml/LabelTorch/Project/qml/TaskTypeSwitcher.qml"
                    onLoaded: {
                        if (item) {
                            item.taskType = root.currentTaskType
                            item.switcherEnabled = appController.projectOpen
                            item.taskTypeSelected.connect(function(tt) { root.onTaskTypeChanged(tt) })
                        }
                    }
                }

                // 弹性空间
                Item { Layout.fillWidth: true }

                // GPU 状态
                Label {
                    text: gpuStatusText
                    font.pixelSize: Theme.fontSizeSmall
                    color: gpuStatusColor
                    font.family: Theme.fontFamily
                    Layout.alignment: Qt.AlignVCenter
                }

                // 当前项目名
                Label {
                    visible: appController.projectOpen
                    text: appController.currentProjectName
                    font.pixelSize: Theme.fontSizeNormal
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }

        // 页面内容区
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: {
                switch(appController.currentPage) {
                    case "project": return 0
                    case "taxonomy": return 1
                    case "dataset": return 2
                    case "annotation": return 3
                    case "training": return 4
                    case "model": return 5
                    case "export": return 6
                    default: return 0
                }
            }

            Loader { source: "qrc:/qt/qml/LabelTorch/Project/qml/ProjectPage.qml" }
            Loader { source: "qrc:/qt/qml/LabelTorch/Project/qml/TaxonomyPage.qml" }
            Loader { source: "qrc:/qt/qml/LabelTorch/Dataset/qml/ImportPage.qml" }
            Loader { source: "qrc:/qt/qml/LabelTorch/Annotation/qml/AnnotationPage.qml" }
            Loader { source: "qrc:/qt/qml/LabelTorch/Training/qml/TrainingPage.qml" }
            Loader { source: "qrc:/qt/qml/LabelTorch/Model/qml/ModelPage.qml" }
            Loader { source: "qrc:/qt/qml/LabelTorch/Export/qml/ExportPage.qml" }
        }

        // 底部日志面板
        LogPanel {
            id: logPanel
            Layout.fillWidth: true
            Layout.preferredHeight: collapsed ? 28 : Theme.logPanelHeight
        }
    }

    Component.onCompleted: {
        logPanel.appendLog("[标炬] LabelTorch v0.1.0 启动")
        logPanel.appendLog("[标炬] 正在连接 Python 后端...")
    }
}
