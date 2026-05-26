// Main.qml - V2 主布局：左侧可折叠导航栏 + 内容区 + 日志面板
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Shell
import LabelTorch.Theme

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

    property string currentTaskType: "detect"
    property string gpuStatusText: "GPU: 检测中..."
    property color gpuStatusColor: Theme.textMuted
    property bool sidebarExpanded: true
    property bool hasRunningTraining: false

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
                var boxLoss = (typeof metrics.box_loss === 'number' && isFinite(metrics.box_loss)) ? metrics.box_loss.toFixed(4) : "?"
                var clsLoss = (typeof metrics.cls_loss === 'number' && isFinite(metrics.cls_loss)) ? metrics.cls_loss.toFixed(4) : "?"
                logPanel.appendLog("[训练] Epoch " + epoch + "/" + total +
                    " box_loss=" + boxLoss +
                    " cls_loss=" + clsLoss)
                root.hasRunningTraining = true
            } else if (eventType === "task.succeeded") {
                logPanel.appendLog("[训练] 训练完成! epochs=" + (payload.epochs_completed || "?") +
                    " early_stopped=" + (payload.early_stopped || false))
                if (payload.metrics) {
                    var map50 = (typeof payload.metrics.mAP50 === 'number' && isFinite(payload.metrics.mAP50)) ? payload.metrics.mAP50.toFixed(4) : "?"
                    var map5095 = (typeof payload.metrics["mAP50-95"] === 'number' && isFinite(payload.metrics["mAP50-95"])) ? payload.metrics["mAP50-95"].toFixed(4) : "?"
                    logPanel.appendLog("[训练] mAP50=" + map50 +
                        " mAP50-95=" + map5095)
                }
                root.hasRunningTraining = false
            } else if (eventType === "task.failed") {
                logPanel.appendLog("[训练] 训练失败: " + (payload.error || "未知错误"))
                root.hasRunningTraining = false
            } else if (eventType === "task.stopped") {
                root.hasRunningTraining = false
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

    // 主布局：左侧导航栏 + 内容区 + 日志面板
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // === 左侧可折叠导航栏 ===
            Rectangle {
                id: sidebar
                Layout.fillHeight: true
                Layout.preferredWidth: root.sidebarExpanded ? Theme.sidebarExpandedWidth : Theme.sidebarCollapsedWidth
                color: Theme.bgSecondary

                Behavior on Layout.preferredWidth {
                    NumberAnimation { duration: Theme.animDurationSlow; easing.type: Easing.InOutQuad }
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // 顶部：Logo + 折叠按钮
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        color: "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingNormal
                            anchors.rightMargin: Theme.spacingNormal
                            spacing: Theme.spacingSmall

                            // 应用图标
                            Label {
                                visible: root.sidebarExpanded
                                text: "标炬"
                                font.pixelSize: Theme.fontSizeLarge
                                font.bold: true
                                color: Theme.accentPrimary
                                font.family: Theme.fontFamily
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Item { Layout.fillWidth: true }

                            // 折叠/展开按钮
                            ToolButton {
                                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                                icon.source: root.sidebarExpanded ? "qrc:/qt/qml/LabelTorch/Shell/icons/sidebar-collapse.svg" : "qrc:/qt/qml/LabelTorch/Shell/icons/sidebar-expand.svg"
                                icon.width: 16
                                icon.height: 16
                                icon.color: Theme.textSecondary
                                onClicked: root.sidebarExpanded = !root.sidebarExpanded

                                ToolTip.visible: hovered
                                ToolTip.text: root.sidebarExpanded ? "收起侧边栏" : "展开侧边栏"
                                ToolTip.delay: 500
                            }
                        }
                    }

                    // 分割线
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.divider
                    }

                    // 导航项列表
                    ListView {
                        id: navList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.topMargin: Theme.spacingSmall
                        clip: true
                        spacing: 2
                        interactive: false

                        model: ListModel {
                            id: navModel
                            ListElement { pageId: "project"; title: "项目管理"; icon: "project"; needsProject: false }
                            ListElement { pageId: "taxonomy"; title: "类别体系"; icon: "taxonomy"; needsProject: false }
                            ListElement { pageId: "dataset"; title: "数据导入"; icon: "dataset"; needsProject: true }
                            ListElement { pageId: "annotation"; title: "标注工作台"; icon: "annotation"; needsProject: true }
                            ListElement { pageId: "training"; title: "训练工作台"; icon: "training"; needsProject: true }
                            ListElement { pageId: "model"; title: "版本中心"; icon: "model"; needsProject: true }
                            ListElement { pageId: "export"; title: "导出中心"; icon: "export"; needsProject: true }
                        }

                        delegate: ItemDelegate {
                            width: ListView.view.width
                            height: 40
                            enabled: !model.needsProject || appController.projectOpen
                            highlighted: appController.currentPage === model.pageId

                            background: Rectangle {
                                color: {
                                    if (!parent.enabled) return "transparent"
                                    if (parent.highlighted) return Theme.bgTertiary
                                    if (parent.hovered) return Theme.bgHover
                                    return "transparent"
                                }
                                // 左侧高亮指示线
                                Rectangle {
                                    visible: parent.parent.highlighted
                                    width: 3
                                    height: parent.height
                                    color: Theme.accentPrimary
                                    radius: 1
                                }
                            }

                            contentItem: RowLayout {
                                spacing: Theme.spacingNormal

                                // 图标占位（使用文字替代SVG图标）
                                Label {
                                    Layout.preferredWidth: 24
                                    Layout.preferredHeight: 24
                                    Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 14
                                    font.family: Theme.fontFamily
                                    color: {
                                        if (!enabled) return Theme.textDisabled
                                        if (highlighted) return Theme.accentPrimary
                                        return Theme.textSecondary
                                    }
                                    text: {
                                        switch(model.icon) {
                                            case "project": return "📂"
                                            case "taxonomy": return "🏷"
                                            case "dataset": return "📁"
                                            case "annotation": return "✏"
                                            case "training": return "🎯"
                                            case "model": return "📦"
                                            case "export": return "📤"
                                            default: return "●"
                                        }
                                    }
                                }

                                // 导航文字
                                Label {
                                    visible: root.sidebarExpanded
                                    text: model.title
                                    font.pixelSize: Theme.fontSizeNormal
                                    font.family: Theme.fontFamily
                                    font.bold: highlighted
                                    color: {
                                        if (!enabled) return Theme.textDisabled
                                        if (highlighted) return Theme.textPrimary
                                        return Theme.textSecondary
                                    }
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                // 训练运行中呼吸态绿色圆点
                                Rectangle {
                                    visible: model.pageId === "training" && root.hasRunningTraining && root.sidebarExpanded
                                    width: 8
                                    height: 8
                                    radius: 4
                                    color: Theme.accentSuccess
                                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight

                                    SequentialAnimation on opacity {
                                        running: parent.visible
                                        loops: Animation.Infinite
                                        NumberAnimation { from: 1.0; to: 0.3; duration: 1000; easing.type: Easing.InOutQuad }
                                        NumberAnimation { from: 0.3; to: 1.0; duration: 1000; easing.type: Easing.InOutQuad }
                                    }
                                }
                            }

                            onClicked: {
                                if (enabled) appController.currentPage = model.pageId
                            }

                            ToolTip.visible: !root.sidebarExpanded && hovered
                            ToolTip.text: model.title
                            ToolTip.delay: 300
                        }
                    }

                    // 底部分割线
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.divider
                    }

                    // 底部：GPU状态与后端连接
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.sidebarExpanded ? 56 : 40
                        color: "transparent"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingSmall
                            spacing: 2

                            // GPU 状态
                            Label {
                                visible: root.sidebarExpanded
                                text: gpuStatusText
                                font.pixelSize: Theme.fontSizeCaption
                                font.family: Theme.fontFamilyMono
                                color: gpuStatusColor
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            // Python 后端连接状态
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSmall

                                Rectangle {
                                    width: 6
                                    height: 6
                                    radius: 3
                                    color: ipcClient.connected ? Theme.accentSuccess : Theme.accentError
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                Label {
                                    visible: root.sidebarExpanded
                                    text: ipcClient.connected ? "Python 后端已连接" : "Python 后端未连接"
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.family: Theme.fontFamily
                                    color: ipcClient.connected ? Theme.textSecondary : Theme.textMuted
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }

            // 右侧分割线
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: Theme.divider
            }

            // === 主内容区 ===
            StackLayout {
                id: contentStack
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

                property var pageSources: [
                    "qrc:/qt/qml/LabelTorch/Project/qml/ProjectPage.qml",
                    "qrc:/qt/qml/LabelTorch/Project/qml/TaxonomyPage.qml",
                    "qrc:/qt/qml/LabelTorch/Dataset/qml/ImportPage.qml",
                    "qrc:/qt/qml/LabelTorch/Annotation/qml/AnnotationPage.qml",
                    "qrc:/qt/qml/LabelTorch/Training/qml/TrainingPage.qml",
                    "qrc:/qt/qml/LabelTorch/Model/qml/ModelPage.qml",
                    "qrc:/qt/qml/LabelTorch/Export/qml/ExportPage.qml"
                ]

                // 记录各页面是否已加载过，已加载的页面保留不卸载
                property var loadedFlags: [true, false, false, false, false, false, false]

                onCurrentIndexChanged: {
                    if (currentIndex >= 0 && currentIndex < pageSources.length) {
                        var loader = itemAt(currentIndex)
                        if (loader && !loader.source.toString() && !loadedFlags[currentIndex]) {
                            loader.source = pageSources[currentIndex]
                            loadedFlags[currentIndex] = true
                        }
                    }
                }

                Loader {
                    property bool wasLoaded: false
                    source: contentStack.pageSources[0]
                    onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
                }
                Loader {
                    property bool wasLoaded: false
                    onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
                }
                Loader {
                    property bool wasLoaded: false
                    onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
                }
                Loader {
                    property bool wasLoaded: false
                    onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
                }
                Loader {
                    property bool wasLoaded: false
                    onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
                }
                Loader {
                    property bool wasLoaded: false
                    onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
                }
                Loader {
                    property bool wasLoaded: false
                    onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
                }
            }
        }

        // 底部日志面板
        LogPanel {
            id: logPanel
            Layout.fillWidth: true
            Layout.preferredHeight: logPanel.collapsed ? 28 : Theme.logPanelHeight
        }
    }

    // 页面切换淡入动画
    NumberAnimation {
        id: fadeInAnim
        property: "opacity"
        from: 0.0
        to: 1.0
        duration: Theme.animDuration
        easing.type: Easing.OutCubic
    }

    Component.onCompleted: {
        logPanel.appendLog("[标炬] LabelTorch v0.1.0 启动")
        logPanel.appendLog("[标炬] 正在连接 Python 后端...")
    }
}
