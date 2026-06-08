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

    ListModel {
        id: navModel
        ListElement { pageId: "project"; title: "项目"; icon: "folder"; needsProject: false }
        ListElement { pageId: "dataset"; title: "数据集"; icon: "images"; needsProject: true }
        ListElement { pageId: "annotation"; title: "标注"; icon: "edit"; needsProject: true }
        ListElement { pageId: "check"; title: "检查"; icon: "check"; needsProject: true }
        ListElement { pageId: "training"; title: "训练"; icon: "brain"; needsProject: true }
        ListElement { pageId: "test"; title: "测试"; icon: "flask"; needsProject: true }
        ListElement { pageId: "export"; title: "导出"; icon: "export"; needsProject: true }
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
        }
        function onBackendError(error) {}
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



                    // 渐变文字（对标参考UI: linear-gradient(to right, #ffffff, #94A3B8)）
                    Text {
                        text: "标炬"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        font.family: Theme.fontFamily
                        anchors.verticalCenter: parent.verticalCenter
                        // QML Text 不支持渐变，用近似色 #C8D4E0 模拟渐变中值
                        color: "#C8D4E0"
                    }
                }

                // 导航标签
                Row {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 0

                    Repeater {
                        model: navModel

                        delegate: ItemDelegate {
                            id: navDelegate
                            height: Theme.headerHeight
                            leftPadding: 22
                            rightPadding: 22
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
                                    Behavior on color { ColorAnimation { duration: 150 } }
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

                                    Behavior on color { ColorAnimation { duration: 150 } }
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
                        MouseArea { id: signalMouse; anchors.fill: parent; hoverEnabled: true }
                    }
                    SvgIcon {
                        icon: "gear"
                        width: 14
                        height: 14
                        color: gearMouse.containsMouse ? Theme.textMain : Theme.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                        MouseArea { id: gearMouse; anchors.fill: parent; hoverEnabled: true }
                    }
                    SvgIcon {
                        icon: "user"
                        width: 14
                        height: 14
                        color: userMouse.containsMouse ? Theme.textMain : Theme.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                        MouseArea { id: userMouse; anchors.fill: parent; hoverEnabled: true }
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

        // === 全局筛选显示栏 (FilterBar) ===
        Rectangle {
            id: globalFilterBar
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: Theme.bgMain
            visible: appController.currentPage === "check" // 数据集和标注页已内部实现

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

                // 数据集筛选
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
                            model: appController.projectOpen ? datasetService.listDatasets(appController.currentProjectId || "") : []
                            textRole: "name"
                            valueRole: "id"
                            currentIndex: -1

                            background: Rectangle { color: "transparent" }
                            contentItem: Text {
                                text: dsCombo.displayText
                                font.pixelSize: 12
                                color: Theme.textMain
                                verticalAlignment: Text.AlignVCenter
                            }
                            indicator: Item { width: 0; height: 0 }
                        }
                    }
                }

                // Tag 过滤
                Rectangle {
                    height: 26
                    implicitWidth: tagLabel.implicitWidth + tagCombo.implicitWidth + 30
                    color: Theme.bgSide
                    border.color: Theme.borderColor
                    radius: 6

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Text {
                            id: tagLabel
                            text: "TAG 过滤"
                            font.pixelSize: 11
                            color: Theme.textMuted
                            verticalAlignment: Text.AlignVCenter
                        }

                        ComboBox {
                            id: tagCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: 20
                            model: ["全选", "默认", "良品", "漏检", "误检", "待定", "重要"]
                            currentIndex: 0

                            background: Rectangle { color: "transparent" }
                            contentItem: Text {
                                text: tagCombo.displayText
                                font.pixelSize: 12
                                color: Theme.textMain
                                verticalAlignment: Text.AlignVCenter
                            }
                            indicator: Item { width: 0; height: 0 }
                        }
                    }
                }

                // 标签类别筛选
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
                            model: appController.projectOpen ? taxonomyModel : []
                            textRole: "className"
                            valueRole: "classIndex"
                            currentIndex: -1

                            background: Rectangle { color: "transparent" }
                            contentItem: Text {
                                text: classCombo.displayText === "" ? "未指定过滤" : classCombo.displayText
                                font.pixelSize: 12
                                color: Theme.textMain
                                verticalAlignment: Text.AlignVCenter
                            }
                            indicator: Item { width: 0; height: 0 }
                        }
                    }
                }
            }
        }

        // === 主内容区：全宽 StackLayout，各页面内部自行管理侧边栏 ===
        StackLayout {
            id: contentStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: {
                switch(appController.currentPage) {
                    case "project": return 0
                    case "dataset": return 1
                    case "annotation": return 2
                    case "check": return 3
                    case "training": return 4
                    case "test": return 5
                    case "export": return 6
                    default: return 0
                }
            }

            property var pageSources: [
                "qrc:/qt/qml/LabelTorch/Project/qml/ProjectPage.qml",
                "qrc:/qt/qml/LabelTorch/Dataset/qml/DatasetPage.qml",
                "qrc:/qt/qml/LabelTorch/Annotation/qml/AnnotationPage.qml",
                "qrc:/qt/qml/LabelTorch/Dataset/qml/CheckPage.qml",
                "qrc:/qt/qml/LabelTorch/Training/qml/TrainingPage.qml",
                "qrc:/qt/qml/LabelTorch/Testing/qml/TestingPage.qml",
                "qrc:/qt/qml/LabelTorch/Export/qml/ExportPage.qml"
            ]

            property var loadedFlags: [true, false, false, false, false, false, false]

            onCurrentIndexChanged: {
                if (currentIndex >= 0 && currentIndex < pageSources.length) {
                    var loader = itemAt(currentIndex)
                    if (loader && !loader.source.toString() && !loadedFlags[currentIndex] && pageSources[currentIndex]) {
                        loader.source = pageSources[currentIndex]
                        loadedFlags[currentIndex] = true
                    }
                }
            }

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
                    Rectangle {
                        width: 150
                        height: 6
                        radius: 3
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.bgMain  // 对标参考UI background:var(--bg-main)

                        Rectangle {
                            width: parent.width * (appController.projectOpen ? root.annotationProgress / 100 : 0)
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
                        text: (appController.projectOpen ? Math.round(root.annotationProgress) : 0) + "%"
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
                color: Theme.textSecondary
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
                    color: Theme.textSecondary
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

    Timer {
        id: preloadTimer
        interval: 300
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
    }
}
