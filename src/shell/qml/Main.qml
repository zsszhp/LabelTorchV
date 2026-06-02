// Main.qml - V4 主布局：赛博蓝科技风侧边栏 + 内容区 + 日志面板
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
    property bool hasRunningTraining: false

    ListModel {
        id: navModel
        ListElement { pageId: "project"; title: "项目管理"; icon: "project"; needsProject: false }
        ListElement { pageId: "taxonomy"; title: "类别体系"; icon: "taxonomy"; needsProject: false }
        ListElement { pageId: "dataset"; title: "数据导入"; icon: "dataset"; needsProject: true }
        ListElement { pageId: "annotation"; title: "标注工作台"; icon: "annotation"; needsProject: true }
        ListElement { pageId: "training"; title: "训练工作台"; icon: "training"; needsProject: true }
        ListElement { pageId: "model"; title: "版本中心"; icon: "model"; needsProject: true }
        ListElement { pageId: "export"; title: "导出中心"; icon: "export"; needsProject: true }
    }

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
            } else {
                if (cmd === "environment.check") {
                    gpuStatusText = "GPU: 检测失败"
                    gpuStatusColor = Theme.accentError
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

    // 主布局：顶部导航栏 + 内容区 + 日志面板
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // === 顶部导航栏 ===
        Rectangle {
            id: topNavigation
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: Theme.bgSecondary

            // 底部霓虹线
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.divider
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLarge
                anchors.rightMargin: Theme.spacingLarge
                spacing: Theme.spacingXLarge

                // 左侧 Logo + 标题
                RowLayout {
                    spacing: Theme.spacingNormal
                    Layout.alignment: Qt.AlignVCenter

                    Rectangle {
                        width: 36
                        height: 36
                        radius: Theme.radiusNormal
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Theme.accentPrimary }
                            GradientStop { position: 1.0; color: Theme.accentSecondary }
                        }

                        Label {
                            anchors.centerIn: parent
                            text: "LT"
                            font.pixelSize: 14
                            font.bold: true
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                        }
                    }

                    Label {
                        text: "标炬 LabelTorch"
                        font.pixelSize: Theme.fontSizeLarge
                        font.bold: true
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                    }
                }

                // 中间导航项
                RowLayout {
                    Layout.fillHeight: true
                    spacing: Theme.spacingSmall
                    Layout.alignment: Qt.AlignVCenter

                    Repeater {
                        model: navModel

                        delegate: ItemDelegate {
                            id: navDelegate
                            Layout.fillHeight: true
                            implicitWidth: 100
                            enabled: !model.needsProject || appController.projectOpen

                            contentItem: ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2

                                Label {
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
                                    font.pixelSize: 16
                                    horizontalAlignment: Text.AlignHCenter
                                    Layout.alignment: Qt.AlignHCenter
                                    color: {
                                        if (!navDelegate.enabled) return Theme.textDisabled
                                        if (appController.currentPage === model.pageId) return Theme.accentPrimary
                                        return Theme.textSecondary
                                    }
                                }

                                RowLayout {
                                    spacing: 4
                                    Layout.alignment: Qt.AlignHCenter

                                    Label {
                                        text: model.title
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.family: Theme.fontFamily
                                        font.bold: appController.currentPage === model.pageId
                                        horizontalAlignment: Text.AlignHCenter
                                        color: {
                                            if (!navDelegate.enabled) return Theme.textDisabled
                                            if (appController.currentPage === model.pageId) return Theme.textPrimary
                                            return Theme.textSecondary
                                        }
                                    }

                                    // 训练进行中呼吸态绿色圆点
                                    Rectangle {
                                        visible: model.pageId === "training" && root.hasRunningTraining
                                        width: 6
                                        height: 6
                                        radius: 3
                                        color: Theme.accentSuccess
                                        Layout.alignment: Qt.AlignVCenter

                                        SequentialAnimation on opacity {
                                            running: parent.visible
                                            loops: Animation.Infinite
                                            NumberAnimation { from: 1.0; to: 0.3; duration: 1000; easing.type: Easing.InOutQuad }
                                            NumberAnimation { from: 0.3; to: 1.0; duration: 1000; easing.type: Easing.InOutQuad }
                                        }
                                    }
                                }
                            }

                            background: Rectangle {
                                color: {
                                    if (!navDelegate.enabled) return "transparent"
                                    if (appController.currentPage === model.pageId) return Qt.alpha(Theme.accentPrimary, 0.08)
                                    if (navDelegate.hovered) return Theme.bgHover
                                    return "transparent"
                                }
                                radius: Theme.radiusSmall

                                // 底部霓虹线
                                Rectangle {
                                    visible: appController.currentPage === model.pageId
                                    height: 3
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    color: Theme.accentPrimary
                                    radius: 1.5
                                }
                            }

                            onClicked: {
                                if (enabled) appController.currentPage = model.pageId
                            }

                            ToolTip.visible: !enabled && hovered
                            ToolTip.text: "请先在项目管理中打开一个项目"
                            ToolTip.delay: 300
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // 右侧状态与信息
                RowLayout {
                    spacing: Theme.spacingLarge
                    Layout.alignment: Qt.AlignVCenter

                    // 当前打开项目标签
                    Label {
                        text: appController.projectOpen ? "当前项目: " + appController.currentProjectName : "未打开项目"
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily
                        color: appController.projectOpen ? Theme.accentPrimary : Theme.textDisabled
                        font.bold: appController.projectOpen
                    }

                    // GPU状态
                    RowLayout {
                        spacing: Theme.spacingSmall
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: gpuStatusColor

                            SequentialAnimation on opacity {
                                running: gpuStatusColor === Theme.accentSuccess
                                loops: Animation.Infinite
                                NumberAnimation { from: 1.0; to: 0.4; duration: 1000; easing.type: Easing.InOutQuad }
                                NumberAnimation { from: 0.4; to: 1.0; duration: 1000; easing.type: Easing.InOutQuad }
                            }
                        }
                        Label {
                            text: gpuStatusText
                            font.pixelSize: Theme.fontSizeCaption
                            font.family: Theme.fontFamily
                            color: gpuStatusColor
                        }
                    }

                    // Python后端连接
                    RowLayout {
                        spacing: Theme.spacingSmall
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: ipcClient.connected ? Theme.accentSuccess : Theme.accentError
                        }
                        Label {
                            text: ipcClient.connected ? "后端已就绪" : "后端断开"
                            font.pixelSize: Theme.fontSizeCaption
                            font.family: Theme.fontFamily
                            color: ipcClient.connected ? Theme.textSecondary : Theme.textDisabled
                        }
                    }
                }
            }
        }

        // === 主内容区与日志面板 ===
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

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

    property bool reallyClose: false

    onClosing: (close) => {
        if (!reallyClose) {
            close.accepted = false
            closeConfirmDialog.open()
        }
    }

    Dialog {
        id: closeConfirmDialog
        title: "确认退出"
        modal: true
        anchors.centerIn: parent
        width: 360
        standardButtons: Dialog.NoButton
        
        background: Rectangle {
            color: Theme.bgCard
            border.color: Theme.border
            border.width: 1
            radius: Theme.radiusLarge
            
            Rectangle {
                width: parent.width
                height: 4
                color: Theme.accentPrimary
                radius: Theme.radiusLarge
                anchors.top: parent.top
            }
        }
        
        header: Rectangle {
            color: "transparent"
            implicitHeight: 48
            
            Label {
                text: "⚠️ 确认退出"
                font.bold: true
                font.pixelSize: Theme.fontSizeSubheading
                font.family: Theme.fontFamily
                color: Theme.textPrimary
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacingLarge
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        
        contentItem: ColumnLayout {
            spacing: Theme.spacingLarge
            
            Label {
                text: "有未完成的任务或工作，您确定要关闭并退出软件吗？"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeNormal
                font.family: Theme.fontFamily
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingLarge
                Layout.rightMargin: Theme.spacingLarge
                Layout.topMargin: Theme.spacingNormal
                Layout.bottomMargin: Theme.spacingNormal
            }
            
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingLarge
                Layout.rightMargin: Theme.spacingLarge
                Layout.bottomMargin: Theme.spacingLarge
                spacing: Theme.spacingLarge
                
                Button {
                    text: "取消"
                    Layout.fillWidth: true
                    flat: true
                    background: Rectangle {
                        color: parent.hovered ? Theme.bgHover : Theme.bgTertiary
                        border.color: Theme.border
                        border.width: 1
                        radius: Theme.radiusSmall
                        implicitHeight: 36
                    }
                    contentItem: Label {
                        text: parent.text
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        closeConfirmDialog.close()
                    }
                }
                
                Button {
                    text: "确定退出"
                    Layout.fillWidth: true
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(Theme.accentError, 1.2) : (parent.hovered ? Qt.lighter(Theme.accentError, 1.1) : Theme.accentError)
                        radius: Theme.radiusSmall
                        implicitHeight: 36
                    }
                    contentItem: Label {
                        text: parent.text
                        color: Theme.textPrimary
                        font.bold: true
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        reallyClose = true
                        closeConfirmDialog.close()
                        root.close()
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        logPanel.appendLog("[标炬] LabelTorch v0.1.0 启动")
        logPanel.appendLog("[标炬] 正在连接 Python 后端...")
    }
}
