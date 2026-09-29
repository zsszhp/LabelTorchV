// Main.qml - V5 主布局：对标 Dihuge DLTools 工业缺陷检测平台
// 顶栏(50px) + 全宽中心内容 + 底栏(34px)
// 每个页面内部自行管理左侧边栏 + 分割线 + 中心内容
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import LabelTorch.Shell
import LabelTorch.Theme
import LabelTorch.Components

ApplicationWindow {
    id: root
    width: 1440
    height: 900
    minimumWidth: 1024
    minimumHeight: 680
    title: "标炬 LabelTorch"
    color: Theme.bgMain
    visible: true
    x: 100
    y: 100

    property string currentTaskType: "detect"
    property string gpuStatusText: "GPU: 检测中..."
    property color gpuStatusColor: Theme.textMuted
    property bool hasRunningTraining: false
    property string selectedFileName: ""
    property real annotationProgress: 0

    // === 全局筛选状态 ===
    // 数据集/标签类别接到页面真实过滤；TAG 无样本级过滤能力，仅作只读摘要展示
    property string globalFilterDatasetId: ""   // 空字符串 = 全部数据集
    property int globalFilterClassIndex: -1     // -1 = 不限类别
    property string globalFilterTagSummary: "全部数据集"

    // 计算标注进度：已标注样本数 / 总样本数 * 100
    // 全程钳制非法值：分母为 0 / 统计返回 undefined 时不得把 NaN 写进绑定
    function updateAnnotationProgress() {
        if (!appController.projectOpen) {
            root.annotationProgress = 0
            return
        }
        var datasets = datasetService.listDatasets(appController.currentProjectId)
        var totalSamples = 0
        var labeledSamples = 0
        for (var i = 0; i < datasets.length; i++) {
            var ds = datasets[i]
            var stats = datasetService.getSampleStats(ds.id)
            var t = Number(stats.totalSamples)
            var l = Number(stats.labeledSamples)
            if (isFinite(t) && t > 0) totalSamples += t
            if (isFinite(l) && l > 0) labeledSamples += l
        }
        if (!isFinite(totalSamples) || totalSamples <= 0) {
            root.annotationProgress = 0
            return
        }
        var pct = labeledSamples / totalSamples * 100
        root.annotationProgress = isFinite(pct) ? Math.min(100, Math.max(0, pct)) : 0
    }

    // 写入日志面板（错误时自动展开，确保用户可见）
    function appendLogLine(line, isError) {
        if (isError) {
            logPanel.collapsed = false
        }
        logPanel.appendLog(line)
    }

    // 重建数据集筛选下拉模型（首项为"全部数据集"）
    function rebuildDatasetFilterModel() {
        dsFilterModel.clear()
        dsFilterModel.append({ name: "全部数据集", dsId: "" })
        if (appController.projectOpen) {
            var list = datasetService.listDatasets(appController.currentProjectId || "")
            for (var i = 0; i < list.length; i++) {
                dsFilterModel.append({ name: list[i].name || ("数据集" + (i + 1)), dsId: list[i].id || "" })
            }
        }
        dsCombo.currentIndex = 0
    }

    // 重建标签类别筛选下拉模型（首项为"全部类别"）
    function rebuildClassFilterModel() {
        classFilterModel.clear()
        classFilterModel.append({ name: "全部类别", clsIndex: -1 })
        if (appController.projectOpen) {
            for (var i = 0; i < taxonomyModel.rowCount(); i++) {
                var idx = taxonomyModel.index(i, 0)
                // ClassNameRole = Qt.UserRole + 1，IndexRole = Qt.UserRole + 2
                var clsName = taxonomyModel.data(idx, Qt.UserRole + 1) || ("class_" + i)
                classFilterModel.append({ name: clsName, clsIndex: i })
            }
        }
        classCombo.currentIndex = 0
    }

    // 刷新 TAG 只读摘要：展示当前筛选数据集上的真实标签
    function updateTagSummary() {
        if (!appController.projectOpen || root.globalFilterDatasetId === "") {
            root.globalFilterTagSummary = "全部数据集"
            return
        }
        var tags = tagService.listTags(root.globalFilterDatasetId)
        if (tags.length === 0) {
            root.globalFilterTagSummary = "无标签"
            return
        }
        var names = []
        for (var i = 0; i < tags.length; i++) {
            names.push(tags[i].name || "")
        }
        root.globalFilterTagSummary = names.join(" / ")
    }

    // 将全局筛选条件推送到当前页面（各页面实现 setDatasetFilter/setClassFilter）
    function applyGlobalFiltersToCurrentPage() {
        var loader = contentStack.itemAt(contentStack.currentIndex)
        if (!loader || !loader.item)
            return
        var page = loader.item
        if (typeof page.setDatasetFilter === "function") {
            page.setDatasetFilter(root.globalFilterDatasetId)
        }
        if (typeof page.setClassFilter === "function") {
            page.setClassFilter(root.globalFilterClassIndex)
        }
    }

    // 切换项目或页面时重置并重发筛选条件
    function resetGlobalFilters() {
        root.globalFilterDatasetId = ""
        root.globalFilterClassIndex = -1
        rebuildDatasetFilterModel()
        rebuildClassFilterModel()
        updateTagSummary()
        applyGlobalFiltersToCurrentPage()
    }

    // === P1-A 导航收敛：8 个主页面 + 3 个分组 ===
    // group: data=数据 / train=训练评估 / deliver=交付
    // taskTypes: 允许显示的任务类型，空串 = 全部；用于按 detect/obb/classify/anomaly 裁剪入口
    // hidden 页面不进导航，由宿主页面的入口按钮跳转（导入向导 / 类别体系 / 数据冻结版 / 模型对比）
    ListModel {
        id: navModel
        // —— 数据 ——
        ListElement { pageId: "project"; title: "项目"; icon: "folder"; group: "data"; needsProject: false; taskTypes: "" }
        ListElement { pageId: "dataset"; title: "数据集"; icon: "images"; group: "data"; needsProject: true; taskTypes: "" }
        ListElement { pageId: "check"; title: "检查"; icon: "check"; group: "data"; needsProject: true; taskTypes: "" }
        ListElement { pageId: "annotation"; title: "标注"; icon: "edit"; group: "data"; needsProject: true; taskTypes: "detect,obb,classify" }
        // —— 训练评估 ——
        ListElement { pageId: "training"; title: "训练"; icon: "brain"; group: "train"; needsProject: true; taskTypes: "" }
        ListElement { pageId: "test"; title: "测试"; icon: "flask"; group: "train"; needsProject: true; taskTypes: "" }
        // —— 交付 ——
        ListElement { pageId: "assist"; title: "智能辅助"; icon: "scan"; group: "deliver"; needsProject: true; taskTypes: "" }
        ListElement { pageId: "export"; title: "导出"; icon: "export"; group: "deliver"; needsProject: true; taskTypes: "" }
    }

    // 分组标题（导航分组标签）
    readonly property var navGroups: [
        { key: "data", title: "数据" },
        { key: "train", title: "训练评估" },
        { key: "deliver", title: "交付" }
    ]

    // 某导航项在当前任务类型下是否可见
    function navItemVisible(item) {
        if (!item.taskTypes || item.taskTypes === "")
            return true
        return item.taskTypes.split(",").indexOf(root.currentTaskType) >= 0
    }

    // 主导航 8 页 + 孤页 5 页（导入向导/类别体系/数据冻结版/模型中心/指标对比）
    // 孤页不进 navModel，由宿主页面入口跳转，标题栏会显示「返回」面包屑
    readonly property var hiddenPages: [
        { pageId: "import", title: "数据导入向导", parent: "dataset" },
        { pageId: "classmap", title: "类别映射", parent: "dataset" },
        { pageId: "taxonomy", title: "类别体系", parent: "project" },
        { pageId: "snapshot", title: "数据冻结版", parent: "training" },
        { pageId: "model", title: "模型中心", parent: "export" },
        { pageId: "compare", title: "指标对比", parent: "export" }
    ]

    function hiddenPageInfo(pageId) {
        for (var i = 0; i < hiddenPages.length; i++) {
            if (hiddenPages[i].pageId === pageId)
                return hiddenPages[i]
        }
        return null
    }

    // 是否为孤页（非主导航页）
    function isHiddenPage(pageId) {
        return hiddenPageInfo(pageId) !== null
    }

    // 数据集筛选下拉模型（首项为全部）
    ListModel { id: dsFilterModel }

    // 标签类别筛选下拉模型（首项为全部）
    ListModel { id: classFilterModel }

    Connections {
        target: appController
        function onCurrentProjectIdChanged() {
            if (appController.projectOpen) {
                root.currentTaskType = projectService.getTaskType(appController.currentProjectId)
                updateAnnotationProgress()
            } else {
                root.currentTaskType = "detect"
                root.annotationProgress = 0
            }
            // 项目切换后重置全局筛选，避免残留无效数据集/类别条件
            root.resetGlobalFilters()
        }
        function onCurrentPageChanged() {
            // 切页后将筛选条件推送到新页面
            Qt.callLater(root.applyGlobalFiltersToCurrentPage)
        }
    }

    Connections {
        target: projectService
        function onTaskTypeChanged(projectId, taskType) {
            if (projectId === appController.currentProjectId) {
                root.currentTaskType = taskType
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
                        gpuStatusColor = Theme.success
                    } else {
                        gpuStatusText = "GPU: 不可用 (仅CPU)"
                        gpuStatusColor = Theme.warning
                    }
                }
            } else {
                if (cmd === "environment.check") {
                    gpuStatusText = "GPU: 检测失败"
                    gpuStatusColor = Theme.danger
                }
            }
        }
        function onConnectedChanged() {
            if (ipcClient.connected) {
                gpuStatusText = "GPU: 已连接，检测中..."
                gpuStatusColor = Theme.primary
                ipcClient.sendRequest("environment.check", {})
            } else {
                gpuStatusText = "Python 后端: 未连接"
                gpuStatusColor = Theme.danger
            }
        }
        function onEventReceived(event) {
            var eventType = event.event_type || ""
            var payload = event.payload || {}
            if (eventType === "task.progress") {
                root.hasRunningTraining = true
            } else if (eventType === "task.succeeded" || eventType === "task.failed" || eventType === "task.stopped") {
                root.hasRunningTraining = false
            }
            // 任务日志/警告/失败写入日志面板，保证用户可见
            if (eventType === "task.log") {
                var logMsg = payload.message || payload.msg || payload.log || JSON.stringify(payload)
                root.appendLogLine("[LOG] " + logMsg, false)
            } else if (eventType === "task.log_batch") {
                // P1-25：后端批量推送（100ms/50 行合并），一次写入避免刷爆
                var lines = payload.lines || []
                for (var li = 0; li < lines.length; li++) {
                    var item = lines[li] || {}
                    var batchMsg = item.message || item.msg || item.log || item.line || JSON.stringify(item)
                    root.appendLogLine("[LOG] " + batchMsg, false)
                }
            } else if (eventType === "task.warning") {
                var warnMsg = payload.message || payload.msg || JSON.stringify(payload)
                root.appendLogLine("[WARN] " + warnMsg, true)
            } else if (eventType === "task.failed") {
                var failMsg = payload.message || payload.error || payload.msg || JSON.stringify(payload)
                root.appendLogLine("[ERROR] 任务失败: " + failMsg, true)
            } else if (eventType === "task.succeeded") {
                var okMsg = payload.best_weight_path ? ("完成，最优权重: " + payload.best_weight_path) : "完成"
                root.appendLogLine("[INFO] 任务成功 " + okMsg, false)
            }
        }
        function onBackendError(error) {
            // Python 后端错误：记录到日志面板（不静默吞异常）
            console.error("[IPC] Backend error:", error)
            root.appendLogLine("[ERROR] Python 后端错误: " + error, true)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // === 顶栏 (50px) ===
        Rectangle {
            id: header
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.headerHeight
            color: Theme.bgSide

            // 底部分割线
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.borderColor
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 0
                anchors.rightMargin: Theme.spacingLarge
                spacing: 0

                // Logo 区域（对标参考UI：26x26渐变方块 + 渐变文字）
                Row {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.leftMargin: Theme.spacingLarge
                    Layout.rightMargin: 12
                    spacing: 10

                    // 渐变图标方块（对标参考UI: 26x26, linear-gradient(135deg, primary, primaryGlow)）
                    Rectangle {
                        width: 26; height: 26; radius: 6
                        anchors.verticalCenter: parent.verticalCenter
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Theme.primary }
                            GradientStop { position: 1.0; color: Theme.primaryGlow }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: "标"
                            color: Theme.logoBgText
                            font.pixelSize: 14
                            font.weight: Font.Bold
                            font.family: Theme.fontFamily
                        }
                        // 发光效果（对标 box-shadow: 0 0 10px rgba(0,229,255,0.3)）
                        layer.enabled: true
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowColor: Qt.alpha(Theme.glowCyanRaw, 0.3)
                            shadowBlur: 0.5
                        }
                    }

                    // 渐变文字（对标参考UI: linear-gradient(to right, #ffffff, #94A3B8)）
                    Text {
                        text: "标炬"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        font.family: Theme.fontFamily
                        anchors.verticalCenter: parent.verticalCenter
                        // QML Text 不支持渐变，用近似色模拟渐变中值
                        color: Theme.logoText
                    }
                }

                // 导航标签：8 项分三组，组间用分隔线 + 小标题弱分组
                Row {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 0

                    Repeater {
                        model: navGroups

                        delegate: Row {
                            id: groupRow
                            required property var modelData
                            required property int index
                            spacing: 0

                            visible: {
                                // 组内至少有一个可见项才显示分组
                                for (var i = 0; i < navModel.count; i++) {
                                    var it = navModel.get(i)
                                    if (it.group === groupRow.modelData.key && root.navItemVisible(it))
                                        return true
                                }
                                return false
                            }

                            // 组前分隔线（首组不加）
                            Rectangle {
                                visible: groupRow.index > 0
                                width: 1
                                height: Theme.headerHeight * 0.4
                                color: Theme.navGroupDivider
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.verticalCenterOffset: 0
                            }

                            // 组标题（弱化显示，点击无效）
                            Text {
                                visible: groupRow.index > 0
                                text: groupRow.modelData.title
                                color: Theme.textDisabled
                                font.pixelSize: 10
                                font.family: Theme.fontFamily
                                leftPadding: Theme.navGroupGap
                                rightPadding: Theme.spacingSmall
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            // 组内导航项
                            Repeater {
                                model: navModel

                                delegate: ItemDelegate {
                                    id: navDelegate
                                    height: Theme.headerHeight
                                    leftPadding: 16
                                    rightPadding: 16
                                    visible: model.group === groupRow.modelData.key && root.navItemVisible(model)
                                    enabled: !model.needsProject || appController.projectOpen

                                    contentItem: Row {
                                        id: navContentRow
                                        spacing: Theme.spacingSmall

                                        SvgIcon {
                                            icon: model.icon
                                            width: 14
                                            height: 14
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: {
                                                if (!navDelegate.enabled) return Theme.textDisabled
                                                if (appController.currentPage === model.pageId) return Theme.primaryGlow
                                                if (navDelegate.hovered) return Theme.textMain
                                                return Theme.textMuted
                                            }
                                            Behavior on color { ColorAnimation { duration: Theme.animDurationFast } }
                                        }

                                        Text {
                                            text: model.title
                                            font.pixelSize: 13
                                            font.weight: appController.currentPage === model.pageId ? Font.DemiBold : Font.Normal
                                            font.family: Theme.fontFamily
                                            color: {
                                                if (!navDelegate.enabled) return Theme.textDisabled
                                                if (appController.currentPage === model.pageId) return Theme.primaryGlow
                                                if (navDelegate.hovered) return Theme.textMain
                                                return Theme.textMuted
                                            }
                                            anchors.verticalCenter: parent.verticalCenter

                                            Behavior on color { ColorAnimation { duration: Theme.animDurationFast } }
                                        }

                                        // 训练中脉冲指示灯
                                        Rectangle {
                                            visible: model.pageId === "training" && root.hasRunningTraining
                                            width: 6
                                            height: 6
                                            radius: 3
                                            color: Theme.success
                                            anchors.verticalCenter: parent.verticalCenter

                                            SequentialAnimation on opacity {
                                                running: parent.visible
                                                loops: Animation.Infinite
                                                NumberAnimation { from: 1.0; to: 0.3; duration: 1000; easing.type: Easing.InOutQuad }
                                                NumberAnimation { from: 0.3; to: 1.0; duration: 1000; easing.type: Easing.InOutQuad }
                                            }
                                        }

                                        // 选中的标签文字外发光效果
                                        layer.enabled: navDelegate.enabled && appController.currentPage === model.pageId
                                        layer.effect: MultiEffect {
                                            shadowEnabled: true
                                            shadowColor: Theme.primaryGlow
                                            shadowBlur: 0.3
                                        }
                                    }

                                    background: Rectangle {
                                        color: !navDelegate.enabled ? "transparent" : (navDelegate.hovered && appController.currentPage !== model.pageId ? Qt.alpha(Theme.textMain, 0.02) : "transparent")

                                        // 激活状态下：垂直亮青渐变背景
                                        gradient: Gradient {
                                            GradientStop { position: 0.0; color: "transparent" }
                                            GradientStop { position: 0.7; color: "transparent" }
                                            GradientStop { position: 1.0; color: (navDelegate.enabled && appController.currentPage === model.pageId) ? Qt.rgba(0, 0.898, 1, 0.05) : "transparent" }
                                        }

                                        // 激活标签底部指示线
                                        Rectangle {
                                            visible: navDelegate.enabled && appController.currentPage === model.pageId
                                            height: 2
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            color: Theme.primaryGlow
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
                    }

                    // 孤页面包屑：进入导入向导/类别体系等子页时显示「← 返回宿主页」
                    Row {
                        visible: root.isHiddenPage(appController.currentPage)
                        spacing: Theme.spacingSmall
                        anchors.verticalCenter: parent.verticalCenter

                        Rectangle {
                            width: 1
                            height: Theme.headerHeight * 0.4
                            color: Theme.navGroupDivider
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: "← 返回"
                            color: Theme.primaryGlow
                            font.pixelSize: 12
                            font.family: Theme.fontFamily
                            anchors.verticalCenter: parent.verticalCenter
                            MouseArea {
                                anchors.fill: parent
                                anchors.margins: -6
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var info = root.hiddenPageInfo(appController.currentPage)
                                    appController.currentPage = info ? info.parent : "project"
                                }
                            }
                        }

                        Text {
                            text: {
                                var info = root.hiddenPageInfo(appController.currentPage)
                                return info ? info.title : ""
                            }
                            color: Theme.textMain
                            font.pixelSize: 12
                            font.family: Theme.fontFamily
                            font.weight: Font.DemiBold
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // 右侧工具图标
                Row {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 18
                    Layout.rightMargin: Theme.spacingLarge

                    SvgIcon {
                        icon: "signal"
                        width: 14
                        height: 14
                        color: signalMouse.containsMouse ? Theme.textMain : Theme.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                        MouseArea {
                            id: signalMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: connectionInfoDialog.open()
                        }
                        ToolTip.visible: signalMouse.containsMouse
                        ToolTip.text: ipcClient.connected ? "后端已连接" : "后端未连接"
                        ToolTip.delay: 500
                    }
                    SvgIcon {
                        icon: "gear"
                        width: 14
                        height: 14
                        color: gearMouse.containsMouse ? Theme.textMain : Theme.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                        MouseArea {
                            id: gearMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: settingsDialog.open()
                        }
                        ToolTip.visible: gearMouse.containsMouse
                        ToolTip.text: "设置"
                        ToolTip.delay: 500
                    }
                    SvgIcon {
                        icon: "user"
                        width: 14
                        height: 14
                        color: userMouse.containsMouse ? Theme.textMain : Theme.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                        MouseArea {
                            id: userMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: aboutDialog.open()
                        }
                        ToolTip.visible: userMouse.containsMouse
                        ToolTip.text: "关于"
                        ToolTip.delay: 500
                    }

                    // 分割线
                    Rectangle {
                        width: 1
                        height: 14
                        color: Theme.borderColor
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // GPU 状态指示灯
                    Row {
                        spacing: Theme.spacingSmall
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            anchors.verticalCenter: parent.verticalCenter
                            color: gpuStatusColor

                            SequentialAnimation on opacity {
                                running: gpuStatusColor === Theme.success
                                loops: Animation.Infinite
                                NumberAnimation { from: 1.0; to: 0.4; duration: 1000; easing.type: Easing.InOutQuad }
                                NumberAnimation { from: 0.4; to: 1.0; duration: 1000; easing.type: Easing.InOutQuad }
                            }
                        }
                        Text {
                            text: gpuStatusText
                            font.pixelSize: Theme.fontSizeCaption
                            font.family: Theme.fontFamily
                            color: gpuStatusColor
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // Python 后端连接状态
                    Row {
                        spacing: Theme.spacingSmall
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            anchors.verticalCenter: parent.verticalCenter
                            color: ipcClient.connected ? Theme.success : Theme.danger

                            SequentialAnimation on opacity {
                                running: ipcClient.connected
                                loops: Animation.Infinite
                                NumberAnimation { from: 1.0; to: 0.4; duration: 1000; easing.type: Easing.InOutQuad }
                                NumberAnimation { from: 0.4; to: 1.0; duration: 1000; easing.type: Easing.InOutQuad }
                            }
                        }
                        Text {
                            text: ipcClient.connected ? "后端就绪" : "后端断开"
                            font.pixelSize: Theme.fontSizeCaption
                            font.family: Theme.fontFamily
                            color: ipcClient.connected ? Theme.textSecondary : Theme.textDisabled
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }

        // === 全局筛选栏 (FilterBar) ===
        // 数据集/标签类别接到页面真实过滤；TAG 无样本级过滤能力，改为只读摘要
        Rectangle {
            id: globalFilterBar
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: Theme.bgMain
            visible: ["dataset", "annotation", "check"].indexOf(appController.currentPage) >= 0

            // 底部分割线
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.borderColor
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                spacing: 15

                // 数据集筛选（真实过滤：推送到当前页面）
                Rectangle {
                    height: 26
                    implicitWidth: dsLabel.implicitWidth + dsCombo.implicitWidth + 30
                    color: Theme.bgSide
                    border.color: Theme.borderColor
                    radius: 6

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Text {
                            id: dsLabel
                            text: "数据集"
                            font.pixelSize: 11
                            color: Theme.textMuted
                            verticalAlignment: Text.AlignVCenter
                        }

                        ComboBox {
                            id: dsCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: 20
                            model: dsFilterModel
                            textRole: "name"
                            valueRole: "dsId"
                            currentIndex: 0

                            background: Rectangle { color: "transparent" }
                            contentItem: Text {
                                text: dsCombo.displayText
                                font.pixelSize: 12
                                color: Theme.textMain
                                verticalAlignment: Text.AlignVCenter
                            }
                            indicator: Item { width: 0; height: 0 }

                            onActivated: {
                                root.globalFilterDatasetId = dsCombo.currentValue || ""
                                root.updateTagSummary()
                                root.applyGlobalFiltersToCurrentPage()
                            }
                        }
                    }
                }

                // TAG 摘要（只读：无样本级标签过滤能力，展示当前数据集真实标签）
                Rectangle {
                    height: 26
                    implicitWidth: tagLabel.implicitWidth + tagValue.implicitWidth + 30
                    color: Theme.bgSide
                    border.color: Theme.borderColor
                    radius: 6
                    opacity: 0.9

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Text {
                            id: tagLabel
                            text: "TAG"
                            font.pixelSize: 11
                            color: Theme.textMuted
                            verticalAlignment: Text.AlignVCenter
                        }

                        Text {
                            id: tagValue
                            text: root.globalFilterTagSummary
                            font.pixelSize: 12
                            color: Theme.textSecondary
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            Layout.maximumWidth: 180
                        }
                    }
                }

                // 标签类别筛选（真实过滤：推送到当前页面）
                Rectangle {
                    height: 26
                    implicitWidth: classLabel.implicitWidth + classCombo.implicitWidth + 30
                    color: Theme.bgSide
                    border.color: Theme.borderColor
                    radius: 6

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Text {
                            id: classLabel
                            text: "标签类别"
                            font.pixelSize: 11
                            color: Theme.textMuted
                            verticalAlignment: Text.AlignVCenter
                        }

                        ComboBox {
                            id: classCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: 20
                            model: classFilterModel
                            textRole: "name"
                            valueRole: "clsIndex"
                            currentIndex: 0

                            background: Rectangle { color: "transparent" }
                            contentItem: Text {
                                text: classCombo.displayText
                                font.pixelSize: 12
                                color: Theme.textMain
                                verticalAlignment: Text.AlignVCenter
                            }
                            indicator: Item { width: 0; height: 0 }

                            onActivated: {
                                var val = classCombo.currentValue
                                root.globalFilterClassIndex = (val === undefined || val === null || val === "") ? -1 : Number(val)
                                root.applyGlobalFiltersToCurrentPage()
                            }
                        }
                    }
                }
            }
        }

        // === 主内容区：全宽 StackLayout，各页面内部自行管理侧边栏 ===
        // 索引 0-7 = 主导航 8 页；8-13 = 孤页（导入向导/类别映射/类别体系/数据冻结版/模型中心/指标对比）
        StackLayout {
            id: contentStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: {
                switch(appController.currentPage) {
                    case "project": return 0
                    case "dataset": return 1
                    case "check": return 2
                    case "annotation": return 3
                    case "training": return 4
                    case "test": return 5
                    case "assist": return 6
                    case "export": return 7
                    // 孤页
                    case "import": return 8
                    case "classmap": return 9
                    case "taxonomy": return 10
                    case "snapshot": return 11
                    case "model": return 12
                    case "compare": return 13
                    default: return 0
                }
            }

            property var pageSources: [
                "qrc:/qt/qml/LabelTorch/Project/qml/ProjectPage.qml",
                "qrc:/qt/qml/LabelTorch/Dataset/qml/DatasetPage.qml",
                "qrc:/qt/qml/LabelTorch/Dataset/qml/CheckPage.qml",
                "qrc:/qt/qml/LabelTorch/Annotation/qml/AnnotationPage.qml",
                "qrc:/qt/qml/LabelTorch/Training/qml/TrainingPage.qml",
                "qrc:/qt/qml/LabelTorch/Testing/qml/TestingPage.qml",
                "qrc:/qt/qml/LabelTorch/Inference/qml/AssistPage.qml",
                "qrc:/qt/qml/LabelTorch/Export/qml/ExportPage.qml",
                // 孤页：按需加载
                "qrc:/qt/qml/LabelTorch/Dataset/qml/ImportPage.qml",
                "qrc:/qt/qml/LabelTorch/Dataset/qml/ClassMappingPage.qml",
                "qrc:/qt/qml/LabelTorch/Project/qml/TaxonomyPage.qml",
                "qrc:/qt/qml/LabelTorch/Training/qml/SnapshotPage.qml",
                "qrc:/qt/qml/LabelTorch/Model/qml/ModelPage.qml",
                "qrc:/qt/qml/LabelTorch/Model/qml/ComparePage.qml"
            ]

            property var loadedFlags: [true, false, false, false, false, false, false, false, false, false, false, false, false, false]

            onCurrentIndexChanged: {
                if (currentIndex >= 0 && currentIndex < pageSources.length) {
                    var loader = itemAt(currentIndex)
                    if (loader && !loader.source.toString() && !loadedFlags[currentIndex] && pageSources[currentIndex]) {
                        loader.source = pageSources[currentIndex]
                        loadedFlags[currentIndex] = true
                    }
                }
            }

            // 14 个 Loader：主 8 页 + 孤 6 页
            Loader {
                asynchronous: true
                source: contentStack.pageSources[0]
                onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
            }
            Loader {
                asynchronous: true
                onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
            }
            Loader {
                asynchronous: true
                onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
            }
            Loader {
                asynchronous: true
                onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
            }
            Loader {
                asynchronous: true
                onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
            }
            Loader {
                asynchronous: true
                onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
            }
            Loader {
                asynchronous: true
                onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
            }
            Loader {
                asynchronous: true
                onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
            }
            Loader {
                asynchronous: true
                onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
            }
            Loader {
                asynchronous: true
                onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
            }
            Loader {
                asynchronous: true
                onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
            }
            Loader {
                asynchronous: true
                onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
            }
            Loader {
                asynchronous: true
                onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
            }
            Loader {
                asynchronous: true
                onLoaded: if (item) item.opacity = 0, fadeInAnim.target = item, fadeInAnim.start()
            }
        }

        // === 日志面板（可折叠，展开高度使用 Theme.logPanelHeight） ===
        LogPanel {
            id: logPanel
            Layout.fillWidth: true
            // 折叠时仅保留 32px 标题栏，展开时使用主题配置高度
            Layout.preferredHeight: collapsed ? 32 : Theme.logPanelHeight
        }

        // === 底栏 (34px) ===
        Rectangle {
            visible: appController.currentPage !== "annotation"
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.footerHeight
            color: Theme.bgSide

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: Theme.borderColor
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLarge
                anchors.rightMargin: Theme.spacingLarge
                spacing: Theme.spacingNormal

                // 左侧：工作区名称 + 选中文件（对标参考UI: "工作区: **Battery_v1** | 选中: CAM_001.png"）
                Text {
                    text: "工作区: "
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }

                Text {
                    text: appController.projectOpen ? appController.currentProjectName : "未打开项目"
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: appController.projectOpen ? Theme.primaryGlow : Theme.textMuted
                    font.weight: Font.DemiBold
                }

                Text {
                    visible: root.selectedFileName !== ""
                    text: " | 选中: " + root.selectedFileName
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }

                Item { Layout.fillWidth: true }

                // NaN 异常告警：降级只是掩盖，几何可能已脏，超阈值建议重启
                // 仅在 Debug 累计超阈值时出现，平时不占位
                Rectangle {
                    visible: appController.nanRestartRecommended
                    Layout.alignment: Qt.AlignVCenter
                    Layout.rightMargin: Theme.spacingNormal
                    height: 22
                    radius: 4
                    color: Qt.alpha(Theme.danger, 0.18)
                    border.color: Theme.danger
                    border.width: 1
                    implicitWidth: nanWarnText.implicitWidth + Theme.spacingLarge * 2

                    Row {
                        anchors.centerIn: parent
                        spacing: Theme.spacingSmall

                        Rectangle {
                            width: 6; height: 6; radius: 3
                            color: Theme.danger
                            anchors.verticalCenter: parent.verticalCenter
                            // 脉冲提醒：几何已脏风险，避免用户忽略
                            SequentialAnimation on opacity {
                                running: appController.nanRestartRecommended
                                loops: Animation.Infinite
                                NumberAnimation { from: 1.0; to: 0.35; duration: 700 }
                                NumberAnimation { from: 0.35; to: 1.0; duration: 700 }
                            }
                        }

                        Text {
                            id: nanWarnText
                            text: "数值异常 " + appController.nanAssertCount + " 次，建议保存后重启"
                            font.pixelSize: Theme.fontSizeCaption
                            font.family: Theme.fontFamily
                            color: Theme.danger
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    ToolTip.visible: nanWarnMouse.containsMouse
                    ToolTip.text: "Qt 布局引擎产生了 NaN 几何，已被拦截但状态可能不一致。\n累计 " +
                                  appController.nanAssertCount + " 次，建议保存工作后重启应用。"
                    ToolTip.delay: 300

                    MouseArea {
                        id: nanWarnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }

                // 右侧：标注进度条 + 百分比（对标参考UI: "标注进度:" + progress bar + percentage）
                Row {
                    spacing: Theme.spacingSmall
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        text: "标注进度:"
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily
                        color: Theme.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // 进度条（对标参考UI: 150px, 6px, bgMain背景, 渐变填充+glow shadow）
                    // 宽度绑定兜底 NaN：annotationProgress 理论上已钳制，此处再防一次布局污染
                    Rectangle {
                        width: 150
                        height: 6
                        radius: 3
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.bgMain  // 对标参考UI background:var(--bg-main)

                        Rectangle {
                            width: {
                                var p = root.annotationProgress
                                if (!isFinite(p) || p < 0) p = 0
                                if (p > 100) p = 100
                                return parent.width * (appController.projectOpen ? p / 100 : 0)
                            }
                            height: parent.height
                            radius: 3
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: Theme.primary }
                                GradientStop { position: 1.0; color: Theme.primaryGlow }
                            }

                            // 发光效果（对标参考UI box-shadow: 0 0 6px var(--primary-glow)）
                            layer.enabled: appController.projectOpen && root.annotationProgress > 0
                            layer.effect: MultiEffect {
                                shadowEnabled: true
                                shadowColor: Theme.primaryGlow
                                shadowBlur: 0.3
                                shadowVerticalOffset: 0
                                shadowHorizontalOffset: 0
                            }
                        }
                    }

                    Text {
                        // 百分比显示同样兜底：Math.round(NaN) 会写进 Text 触发布局异常
                        text: (function() {
                            var p = root.annotationProgress
                            if (!isFinite(p) || p < 0) p = 0
                            if (p > 100) p = 100
                            return (appController.projectOpen ? Math.round(p) : 0) + "%"
                        })()
                        font.pixelSize: Theme.fontSizeCaption
                        font.family: Theme.fontFamilyMono
                        font.weight: Font.Bold
                        color: Theme.primaryGlow
                        anchors.verticalCenter: parent.verticalCenter

                        layer.enabled: appController.projectOpen
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowColor: Theme.primaryGlow
                            shadowBlur: 0.3
                        }
                    }
                }
            }
        }
    }

    NumberAnimation {
        id: fadeInAnim
        property: "opacity"
        from: 0.0
        to: 1.0
        duration: Theme.animDuration
        easing.type: Easing.OutCubic
    }

    // === P1-B 全局 Toast 层（接收 ToastBus 通知） ===
    Toast {
        id: toastHost
        anchors.fill: parent
        z: 1000
    }

    // === 快捷键 ===
    // F1：打开帮助；?（Shift+/）：快捷键卡片；Ctrl+1~8：切到主导航第 N 页
    Shortcut {
        sequence: "F1"
        context: Qt.ApplicationShortcut
        onActivated: helpDialog.open()
    }
    Shortcut {
        // "?" = Shift+/，直接写 "?" 作为序列
        sequence: "?"
        context: Qt.ApplicationShortcut
        onActivated: shortcutCardDialog.open()
    }
    Shortcut { sequence: "Ctrl+1"; context: Qt.ApplicationShortcut; onActivated: root.gotoNavIndex(0) }
    Shortcut { sequence: "Ctrl+2"; context: Qt.ApplicationShortcut; onActivated: root.gotoNavIndex(1) }
    Shortcut { sequence: "Ctrl+3"; context: Qt.ApplicationShortcut; onActivated: root.gotoNavIndex(2) }
    Shortcut { sequence: "Ctrl+4"; context: Qt.ApplicationShortcut; onActivated: root.gotoNavIndex(3) }
    Shortcut { sequence: "Ctrl+5"; context: Qt.ApplicationShortcut; onActivated: root.gotoNavIndex(4) }
    Shortcut { sequence: "Ctrl+6"; context: Qt.ApplicationShortcut; onActivated: root.gotoNavIndex(5) }
    Shortcut { sequence: "Ctrl+7"; context: Qt.ApplicationShortcut; onActivated: root.gotoNavIndex(6) }
    Shortcut { sequence: "Ctrl+8"; context: Qt.ApplicationShortcut; onActivated: root.gotoNavIndex(7) }

    // 按可见导航顺序跳转（任务类型裁剪后下标仍保持稳定，因为 navModel 顺序固定）
    function gotoNavIndex(index) {
        // 只统计当前任务类型下可见的导航项
        var visible = []
        for (var i = 0; i < navModel.count; i++) {
            var it = navModel.get(i)
            if (root.navItemVisible(it))
                visible.push(it)
        }
        if (index >= 0 && index < visible.length) {
            var item = visible[index]
            if (!item.needsProject || appController.projectOpen) {
                appController.currentPage = item.pageId
            } else {
                ToastBus.info("请先打开项目后再切换到「" + item.title + "」")
            }
        }
    }

    property bool reallyClose: false

    onClosing: (close) => {
        if (!reallyClose) {
            close.accepted = false
            closeConfirmDialog.open()
        }
    }

    ModalDialog {
        id: closeConfirmDialog
        title: "确认退出"
        dialogWidth: 360

        ColumnLayout {
            width: parent.width - Theme.spacingLarge * 2
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacingLarge

            Text {
                text: "是否确实关闭"
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeNormal
                font.family: Theme.fontFamily
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingLarge
                Layout.bottomMargin: Theme.spacingLarge
            }
        }

        footerContent: Row {
            spacing: Theme.spacingLarge
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            Button {
                text: "取消"
                width: 90
                background: Rectangle {
                    color: parent.hovered ? Theme.bgHover : Theme.bgCard
                    border.color: Theme.borderColor
                    border.width: 1
                    radius: Theme.radiusSmall
                    implicitHeight: 32
                }
                contentItem: Text {
                    text: parent.text
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeNormal
                    font.family: Theme.fontFamily
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: closeConfirmDialog.close()
            }

            Button {
                text: "确定退出"
                width: 90
                background: Rectangle {
                    color: parent.pressed ? Qt.darker(Theme.danger, 1.2) : (parent.hovered ? Qt.lighter(Theme.danger, 1.1) : Theme.danger)
                    radius: Theme.radiusSmall
                    implicitHeight: 32
                }
                contentItem: Text {
                    text: parent.text
                    color: Theme.textMain
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

    // 连接状态详情对话框
    Dialog {
        id: connectionInfoDialog
        title: "连接状态"
        modal: true
        anchors.centerIn: parent
        width: 360
        height: 200

        palette.window: Theme.bgMain
        palette.windowText: Theme.textMain
        palette.base: Theme.bgInput
        palette.text: Theme.textMain
        palette.button: Theme.bgCard
        palette.buttonText: Theme.textMain

        background: Rectangle {
            color: Theme.bgMain
            radius: Theme.radiusLarge
            border.color: Theme.borderColor
            border.width: 1
        }

        header: Rectangle {
            color: Theme.bgInput
            height: 44
            radius: Theme.radiusLarge

            Label {
                anchors.centerIn: parent
                text: "连接状态"
                color: Theme.primary
                font.pixelSize: Theme.fontSizeSubheading
                font.bold: true
            }
        }

        contentItem: Rectangle {
            color: Theme.bgMain

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacingNormal

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "Python 后端："
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeNormal
                    }
                    Label {
                        text: ipcClient.connected ? "已连接" : "未连接"
                        color: ipcClient.connected ? Theme.success : Theme.danger
                        font.pixelSize: Theme.fontSizeNormal
                        font.bold: true
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "GPU 状态："
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeNormal
                    }
                    Label {
                        text: gpuStatusText
                        color: gpuStatusColor
                        font.pixelSize: Theme.fontSizeNormal
                        font.bold: true
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        footer: Rectangle {
            color: Theme.bgInput
            height: 48
            radius: Theme.radiusLarge

            Button {
                anchors.centerIn: parent
                text: "关闭"
                flat: true
                contentItem: Label {
                    text: parent.text
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeNormal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: connectionInfoDialog.close()
            }
        }
    }

    // 帮助对话框（F1）
    ModalDialog {
        id: helpDialog
        title: "使用帮助"
        dialogWidth: 480

        ColumnLayout {
            width: parent.width - Theme.spacingLarge * 2
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacingNormal

            Text {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingNormal
                text: "标炬 LabelTorch — 工业缺陷检测数据治理与模型闭环"
                color: Theme.textMain
                font.pixelSize: Theme.fontSizeSubheading
                font.weight: Font.DemiBold
                font.family: Theme.fontFamily
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                text: "推荐工作流：导入数据集 → 检查/标注 → 训练 → 测试 → 智能辅助 → 导出"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeNormal
                font.family: Theme.fontFamily
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingNormal
                text: "· 项目：创建/打开项目，配置类别体系\n" +
                      "· 数据集：导入图片与标签，查看样本统计\n" +
                      "· 检查：数据质量核查与异常样本定位\n" +
                      "· 标注：HBB/OBB/分类标注与修订追踪\n" +
                      "· 训练：基于数据冻结版启动训练，查看曲线\n" +
                      "· 测试：模型评估、PR 曲线与混淆矩阵\n" +
                      "· 智能辅助：辅助标注、难例挖掘、异常检测、视频推理\n" +
                      "· 导出：导出 pt/onnx 产物并验证"
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSmall
                font.family: Theme.fontFamily
                wrapMode: Text.WordWrap
                lineHeight: 1.5
            }

            Text {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingNormal
                Layout.bottomMargin: Theme.spacingNormal
                text: "按 ? 查看快捷键，按 F1 随时打开本帮助。"
                color: Theme.primaryGlow
                font.pixelSize: Theme.fontSizeSmall
                font.family: Theme.fontFamily
            }
        }

        footerContent: Row {
            spacing: Theme.spacingLarge
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            Button {
                text: "快捷键"
                width: 90
                background: Rectangle {
                    color: parent.hovered ? Theme.bgHover : Theme.bgCard
                    border.color: Theme.borderColor
                    border.width: 1
                    radius: Theme.radiusSmall
                    implicitHeight: 32
                }
                contentItem: Text {
                    text: parent.text
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeNormal
                    font.family: Theme.fontFamily
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    helpDialog.close()
                    shortcutCardDialog.open()
                }
            }

            Button {
                text: "关闭"
                width: 90
                background: Rectangle {
                    color: parent.pressed ? Qt.darker(Theme.primary, 1.2) : Theme.primary
                    radius: Theme.radiusSmall
                    implicitHeight: 32
                }
                contentItem: Text {
                    text: parent.text
                    color: Theme.bgMain
                    font.pixelSize: Theme.fontSizeNormal
                    font.bold: true
                    font.family: Theme.fontFamily
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: helpDialog.close()
            }
        }
    }

    // 快捷键卡片（?）
    ModalDialog {
        id: shortcutCardDialog
        title: "快捷键"
        dialogWidth: 420

        ColumnLayout {
            width: parent.width - Theme.spacingLarge * 2
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacingSmall

            Repeater {
                model: [
                    { key: "F1", desc: "打开使用帮助" },
                    { key: "?", desc: "打开快捷键卡片" },
                    { key: "Ctrl+1 ~ Ctrl+8", desc: "切换主导航页面" },
                    { key: "Esc", desc: "关闭弹窗 / 取消绘制" },
                    { key: "Ctrl+S", desc: "保存当前标注" },
                    { key: "Delete", desc: "删除选中标注" },
                    { key: "Ctrl+Z / Ctrl+Y", desc: "撤销 / 重做标注" }
                ]

                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.spacingSmall
                    spacing: Theme.spacingLarge

                    Rectangle {
                        Layout.preferredWidth: 130
                        Layout.preferredHeight: 24
                        radius: Theme.radiusSmall
                        color: Theme.bgInput
                        border.color: Theme.borderColor
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: modelData.key
                            color: Theme.primaryGlow
                            font.pixelSize: Theme.fontSizeCaption
                            font.family: Theme.fontFamilyMono
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.desc
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily
                    }
                }
            }

            Item { Layout.preferredHeight: Theme.spacingNormal }
        }

        footerContent: Row {
            spacing: Theme.spacingLarge
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter

            Button {
                text: "关闭"
                width: 90
                background: Rectangle {
                    color: parent.pressed ? Qt.darker(Theme.primary, 1.2) : Theme.primary
                    radius: Theme.radiusSmall
                    implicitHeight: 32
                }
                contentItem: Text {
                    text: parent.text
                    color: Theme.bgMain
                    font.pixelSize: Theme.fontSizeNormal
                    font.bold: true
                    font.family: Theme.fontFamily
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: shortcutCardDialog.close()
            }
        }
    }

    // 设置对话框
    Dialog {
        id: settingsDialog
        title: "设置"
        modal: true
        anchors.centerIn: parent
        width: 420
        height: 280

        palette.window: Theme.bgMain
        palette.windowText: Theme.textMain
        palette.base: Theme.bgInput
        palette.text: Theme.textMain
        palette.button: Theme.bgCard
        palette.buttonText: Theme.textMain

        background: Rectangle {
            color: Theme.bgMain
            radius: Theme.radiusLarge
            border.color: Theme.borderColor
            border.width: 1
        }

        header: Rectangle {
            color: Theme.bgInput
            height: 44
            radius: Theme.radiusLarge

            Label {
                anchors.centerIn: parent
                text: "设置"
                color: Theme.primary
                font.pixelSize: Theme.fontSizeSubheading
                font.bold: true
            }
        }

        contentItem: Rectangle {
            color: Theme.bgMain

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacingNormal

                Label {
                    text: "Python 路径："
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeNormal
                }

                TextField {
                    id: pythonPathField
                    Layout.fillWidth: true
                    text: typeof appSettings !== "undefined" ? (appSettings.pythonPath || "C:/A/anaconda/envs/labeltorch/python.exe") : "C:/A/anaconda/envs/labeltorch/python.exe"
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeNormal
                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: pythonPathField.activeFocus ? Theme.primary : Theme.borderColor
                        border.width: 1
                        implicitHeight: 32
                    }
                }

                Label {
                    text: "日志级别："
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeNormal
                }

                ComboBox {
                    id: logLevelCombo
                    Layout.fillWidth: true
                    model: ["DEBUG", "INFO", "WARNING", "ERROR"]
                    currentIndex: 1

                    contentItem: Label {
                        text: logLevelCombo.displayText
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: Theme.spacingSmall
                    }

                    background: Rectangle {
                        color: Theme.bgCard
                        radius: Theme.radiusSmall
                        border.color: logLevelCombo.activeFocus ? Theme.primary : Theme.borderColor
                        border.width: 1
                        implicitHeight: 32
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        footer: Rectangle {
            color: Theme.bgInput
            height: 52
            radius: Theme.radiusLarge

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLarge
                anchors.rightMargin: Theme.spacingLarge
                spacing: Theme.spacingNormal

                Button {
                    text: "取消"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    flat: true
                    contentItem: Label {
                        text: parent.text
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: settingsDialog.reject()
                }

                Button {
                    text: "保存"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    contentItem: Label {
                        text: parent.text
                        color: Theme.bgMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(Theme.primary, 1.2) : Theme.primary
                        radius: Theme.radiusSmall
                    }
                    onClicked: {
                        if (typeof appSettings !== "undefined") {
                            appSettings.pythonPath = pythonPathField.text
                        }
                        // 日志级别立即生效：按级别过滤日志面板已显示内容的展示阈值
                        // C++ 侧 Log::setLevel 未暴露给 QML，这里先作用于日志面板显示层
                        logPanel.minLevel = logLevelCombo.currentIndex
                        // Python 路径需重启后端才生效，明确提示用户
                        ToastBus.success("设置已保存：日志级别 = " + logLevelCombo.currentText + "（立即生效）")
                        ToastBus.info("Python 路径将在重启 Python 后端后生效")
                        settingsDialog.accept()
                    }
                }
            }
        }
    }

    // 关于对话框
    Dialog {
        id: aboutDialog
        title: "关于"
        modal: true
        anchors.centerIn: parent
        width: 380
        height: 260

        palette.window: Theme.bgMain
        palette.windowText: Theme.textMain
        palette.base: Theme.bgInput
        palette.text: Theme.textMain
        palette.button: Theme.bgCard
        palette.buttonText: Theme.textMain

        background: Rectangle {
            color: Theme.bgMain
            radius: Theme.radiusLarge
            border.color: Theme.borderColor
            border.width: 1
        }

        header: Rectangle {
            color: Theme.bgInput
            height: 44
            radius: Theme.radiusLarge

            Label {
                anchors.centerIn: parent
                text: "关于"
                color: Theme.primary
                font.pixelSize: Theme.fontSizeSubheading
                font.bold: true
            }
        }

        contentItem: Rectangle {
            color: Theme.bgMain

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacingNormal

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "标炬 LabelTorch"
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeLarge
                    font.bold: true
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "版本 0.1.0"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeNormal
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "工业缺陷检测智能一体化平台"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeCaption
                    wrapMode: Text.WordWrap
                }

                Item { Layout.fillHeight: true }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Qt 6.11 + QML + C++17 + Python 3.11"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: Theme.fontFamilyMono
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Ultralytics + Anomalib"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: Theme.fontFamilyMono
                }
            }
        }

        footer: Rectangle {
            color: Theme.bgInput
            height: 48
            radius: Theme.radiusLarge

            Button {
                anchors.centerIn: parent
                text: "关闭"
                flat: true
                contentItem: Label {
                    text: parent.text
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeNormal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: aboutDialog.close()
            }
        }
    }

    Timer {
        id: preloadTimer
        // 延迟预加载：窗口首帧布局完成前不实例化其余页面，
        // 避开布局引擎初始化竞态期的 NaN 几何（Qt 6.11 Debug ASSERT 主因之一）
        interval: 600
        repeat: true
        running: true
        property int nextIndex: 1
        onTriggered: {
            if (nextIndex < contentStack.pageSources.length) {
                var loader = contentStack.itemAt(nextIndex)
                if (loader && !loader.source.toString() && !contentStack.loadedFlags[nextIndex]) {
                    loader.source = contentStack.pageSources[nextIndex]
                    contentStack.loadedFlags[nextIndex] = true
                }
                nextIndex++
            } else {
                running = false
            }
        }
    }

    Component.onCompleted: {
        // 初始化全局筛选模型；若已打开项目则恢复筛选条件
        root.rebuildDatasetFilterModel()
        root.rebuildClassFilterModel()
        root.updateTagSummary()
    }
}
