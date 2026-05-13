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

    property string currentTaskType: "detect"
    property string gpuStatusText: "GPU: 检测中..."
    property color gpuStatusColor: Theme.textMuted

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

    Connections {
        target: ipcClient
        function onResponseReceived(response) {
            var cmd = response.command || ""
            if (response.success) {
                var result = response.result || {}
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
        function onEventReceived(event) {
            var eventType = event.event_type || ""
            var payload = event.payload || {}
            if (eventType === "task.progress") {
                var epoch = payload.epoch || 0
                var total = payload.total_epochs || 0
                var metrics = payload.metrics || {}
                logPanel.appendLog("[训练] Epoch " + epoch + "/" + total +
                    " box_loss=" + (metrics.box_loss || "?").toFixed(4) +
                    " cls_loss=" + (metrics.cls_loss || "?").toFixed(4))
            } else if (eventType === "task.succeeded") {
                logPanel.appendLog("[训练] 训练完成! epochs=" + (payload.epochs_completed || "?") +
                    " early_stopped=" + (payload.early_stopped || false))
                if (payload.metrics) {
                    logPanel.appendLog("[训练] mAP50=" + (payload.metrics.mAP50 || "?") +
                        " mAP50-95=" + (payload.metrics["mAP50-95"] || "?"))
                }
            } else if (eventType === "task.failed") {
                logPanel.appendLog("[训练] 训练失败: " + (payload.error || "未知错误"))
            }
        }
        function onBackendError(error) {
            logPanel.appendLog("[错误] " + error)
        }
    }

    function onTaskTypeChanged(taskType) {
        if (appController.projectOpen && taskType !== root.currentTaskType) {
            root.currentTaskType = taskType
            projectService.setTaskType(appController.currentProjectId, taskType)
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: Theme.navWidth
            Layout.fillHeight: true
            color: Theme.bgSecondary

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 0
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    color: Theme.bgTertiary

                    Label {
                        anchors.centerIn: parent
                        text: "标炬 LabelTorch"
                        font.pixelSize: Theme.fontSizeLarge
                        font.bold: true
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: appController.projectOpen ? 40 : 0
                    visible: appController.projectOpen
                    color: Theme.bgPrimary

                    Label {
                        anchors.centerIn: parent
                        text: appController.currentProjectName
                        font.pixelSize: Theme.fontSizeNormal
                        color: Theme.accentPrimary
                        font.family: Theme.fontFamily
                        elide: Text.ElideRight
                        width: parent.width - 16
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                ListView {
                    id: navList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.topMargin: Theme.spacingNormal
                    clip: true
                    model: [
                        { name: "项目管理", icon: "\u25C6", page: "project" },
                        { name: "类别体系", icon: "\u25B6", page: "taxonomy" },
                        { name: "数据导入", icon: "\u25BC", page: "dataset" },
                        { name: "标注工作台", icon: "\u270E", page: "annotation" },
                        { name: "训练工作台", icon: "\u2699", page: "training" },
                        { name: "版本中心", icon: "\u25C8", page: "model" },
                        { name: "导出中心", icon: "\u25A0", page: "export" }
                    ]
                    delegate: ItemDelegate {
                        width: navList.width
                        height: 44
                        highlighted: appController.currentPage === modelData.page
                        enabled: modelData.page === "project" || modelData.page === "taxonomy" || appController.projectOpen

                        contentItem: Row {
                            spacing: 10
                            leftPadding: 16
                            Label {
                                text: modelData.icon
                                font.pixelSize: Theme.fontSizeLarge
                                color: parent.parent.enabled ? Theme.textPrimary : Theme.textDisabled
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: modelData.name
                                font.pixelSize: Theme.fontSizeNormal
                                font.family: Theme.fontFamily
                                color: highlighted ? Theme.accentPrimary : (parent.parent.enabled ? Theme.textPrimary : Theme.textDisabled)
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        background: Rectangle {
                            color: highlighted ? Theme.bgSelected : (parent.hovered ? Theme.bgPrimary : "transparent")
                            radius: Theme.radiusSmall
                        }

                        onClicked: appController.currentPage = modelData.page
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    color: Theme.bgTertiary

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 4

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: ipcClient.connected ? Theme.accentSuccess : Theme.accentError
                        }

                        Label {
                            text: ipcClient.connected ? "后端已连接" : "后端未连接"
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgPrimary

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.statusBarHeight
                    color: Theme.bgSecondary

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingLarge
                        anchors.rightMargin: Theme.spacingLarge

                        Label {
                            text: {
                                switch(appController.currentPage) {
                                    case "project": return "项目管理"
                                    case "taxonomy": return "类别体系"
                                    case "dataset": return "数据导入"
                                    case "annotation": return "标注工作台"
                                    case "training": return "训练工作台"
                                    case "model": return "版本中心"
                                    case "export": return "导出中心"
                                    default: return "标炬"
                                }
                            }
                            font.pixelSize: Theme.fontSizeNormal
                            font.bold: true
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                        }

                        Loader {
                            visible: appController.projectOpen
                            source: "qrc:/qt/qml/LabelTorch/Project/qml/TaskTypeSwitcher.qml"
                            onLoaded: {
                                if (item) {
                                    item.taskType = root.currentTaskType
                                    item.switcherEnabled = appController.projectOpen
                                    item.taskTypeSelected.connect(function(tt) { root.onTaskTypeChanged(tt) })
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            text: gpuStatusText
                            font.pixelSize: Theme.fontSizeSmall
                            color: gpuStatusColor
                            font.family: Theme.fontFamily
                        }

                        Label {
                            visible: appController.projectOpen
                            text: appController.currentProjectName
                            font.pixelSize: Theme.fontSizeNormal
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                        }
                    }
                }

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

                LogPanel {
                    id: logPanel
                    Layout.fillWidth: true
                    Layout.preferredHeight: collapsed ? 28 : Theme.logPanelHeight
                }
            }
        }
    }

    Component.onCompleted: {
        logPanel.appendLog("[标炬] LabelTorch v0.1.0 启动")
        logPanel.appendLog("[标炬] 正在连接 Python 后端...")
    }
}
