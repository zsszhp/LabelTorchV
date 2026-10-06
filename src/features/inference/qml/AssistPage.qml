// AssistPage.qml - 智能辅助聚合页
// 将辅助标注 / 难例挖掘 / 异常检测 / 视频推理收敛为同一页面下的标签页，
// 由主导航「智能辅助」统一进入；按任务类型裁剪不相关标签
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme
import LabelTorch.Components
import LabelTorch.Shell

Item {
    id: root

    // 当前任务类型：detect / obb / classify / anomaly
    readonly property string currentTaskType: {
        if (typeof appController === "undefined" || !appController.projectOpen)
            return "detect"
        return projectService.getTaskType(appController.currentProjectId) || "detect"
    }

    // 固定四个能力的元数据；visible 按任务类型裁剪
    // detect/obb：辅助标注 + 难例挖掘 + 视频推理 + 异常检测
    // classify：辅助标注 + 难例挖掘
    // anomaly：异常检测 + 难例挖掘
    readonly property var tabDefs: [
        { key: "assist",  title: "辅助标注", src: "qrc:/qt/qml/LabelTorch/Inference/qml/AssistedLabelPanel.qml",
          visible: currentTaskType === "detect" || currentTaskType === "obb" || currentTaskType === "classify" },
        { key: "hardcase", title: "难例挖掘", src: "qrc:/qt/qml/LabelTorch/Inference/qml/ActiveLearningPage.qml",
          visible: true },
        { key: "video",   title: "视频推理", src: "qrc:/qt/qml/LabelTorch/Inference/qml/VideoInferencePage.qml",
          visible: currentTaskType === "detect" || currentTaskType === "obb" },
        { key: "anomaly", title: "异常检测", src: "qrc:/qt/qml/LabelTorch/Inference/qml/AnomalyInferPanel.qml",
          visible: currentTaskType === "anomaly" || currentTaskType === "detect" }
    ]

    // 过滤后的可见标签列表（标签栏用）
    readonly property var visibleTabs: tabDefs.filter(function (t) { return t.visible })

    // 当前选中的可见标签下标
    property int currentTab: 0

    // 可见标签对应的 StackLayout 页序（固定 4 槽，按 key 映射）
    readonly property var keyToSlot: ({ assist: 0, hardcase: 1, video: 2, anomaly: 3 })

    function slotIndexForKey(key) {
        return keyToSlot[key] !== undefined ? keyToSlot[key] : 0
    }

    // 当前应显示的 StackLayout 槽位
    readonly property int currentSlot: {
        if (visibleTabs.length === 0)
            return -1
        var idx = Math.min(currentTab, visibleTabs.length - 1)
        return slotIndexForKey(visibleTabs[idx].key)
    }

    onVisibleTabsChanged: {
        if (currentTab >= visibleTabs.length)
            currentTab = 0
    }

    Connections {
        target: appController
        function onCurrentProjectIdChanged() {
            root.currentTab = 0
            injectPanelContext()
        }
    }

    // === 面板上下文注入 ===
    // 各能力面板自身不读 appController，须由本页注入项目/数据集/权重上下文，
    // 否则批次列表恒空、加载模型按钮永久禁用。
    function resolveDefaultDatasetId() {
        if (typeof appController === "undefined" || !appController.projectOpen)
            return ""
        var dss = datasetService.listDatasets(appController.currentProjectId)
        return dss.length > 0 ? dss[0].id : ""
    }

    function resolveBestWeight() {
        if (typeof appController === "undefined" || !appController.projectOpen)
            return ""
        var versions = modelRegistry.listModelVersions(appController.currentProjectId)
        for (var i = 0; i < versions.length; ++i) {
            if (versions[i].bestWeightPath && versions[i].bestWeightPath !== "")
                return versions[i].bestWeightPath
        }
        return ""
    }

    function injectPanelContext() {
        var pid = (typeof appController !== "undefined" && appController.projectOpen)
                  ? appController.currentProjectId : ""
        var dsId = resolveDefaultDatasetId()
        if (assistLoader.item) {
            assistLoader.item.currentProjectId = pid
            assistLoader.item.currentDatasetId = dsId
        }
        if (hardcaseLoader.item)
            hardcaseLoader.item.currentProjectId = pid
        if (anomalyLoader.item) {
            anomalyLoader.item.currentProjectId = pid
            anomalyLoader.item.currentWeightPath = resolveBestWeight()
        }
    }

    // === 未打开项目空态 ===
    EmptyState {
        anchors.centerIn: parent
        visible: typeof appController !== "undefined" && !appController.projectOpen
        icon: "scan"
        title: "请先打开一个项目"
        description: "智能辅助提供辅助标注、难例挖掘、异常检测与视频推理能力"
        actionText: "前往项目管理"
        onActionClicked: appController.currentPage = "project"
    }

    // === 主布局：子标签栏 + 标签内容 ===
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: typeof appController !== "undefined" && appController.projectOpen

        // 子标签栏
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.subTabHeight
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

                Repeater {
                    model: root.visibleTabs.length

                    delegate: Rectangle {
                        id: tabBtn
                        required property int index
                        Layout.preferredHeight: 28
                        Layout.preferredWidth: tabLabel.implicitWidth + Theme.spacingXLarge
                        radius: Theme.radiusSmall
                        color: {
                            if (root.currentTab === index)
                                return Theme.primary
                            return tabMouse.containsMouse ? Theme.bgHover : Theme.bgCard
                        }
                        border.color: root.currentTab === index ? Theme.primaryGlow : Theme.borderColor
                        border.width: 1

                        Text {
                            id: tabLabel
                            anchors.centerIn: parent
                            text: root.visibleTabs[index].title
                            color: root.currentTab === index ? Theme.textMain : Theme.textMuted
                            font.pixelSize: Theme.fontSizeNormal
                            font.family: Theme.fontFamily
                            font.weight: root.currentTab === index ? Font.DemiBold : Font.Normal
                        }

                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.currentTab = tabBtn.index
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // 当前任务类型提示
                Text {
                    text: "任务类型: " + root.currentTaskType
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: Theme.fontFamilyMono
                }
            }
        }

        // 标签内容区：固定 4 槽，按 key 映射，避免裁剪后下标错位
        // Loader 必须同步加载：Qt 6.11 异步 Loader 有卡 Loading 风险（Main.qml 同类问题已改同步）
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentSlot

            Loader {
                id: assistLoader
                asynchronous: false
                source: root.visibleTabs.some(function (t) { return t.key === "assist" })
                        ? root.tabDefs[0].src : ""
                onLoaded: root.injectPanelContext()
            }
            Loader {
                id: hardcaseLoader
                asynchronous: false
                source: root.visibleTabs.some(function (t) { return t.key === "hardcase" })
                        ? root.tabDefs[1].src : ""
                onLoaded: root.injectPanelContext()
            }
            Loader {
                id: videoLoader
                asynchronous: false
                source: root.visibleTabs.some(function (t) { return t.key === "video" })
                        ? root.tabDefs[2].src : ""
            }
            Loader {
                id: anomalyLoader
                asynchronous: false
                source: root.visibleTabs.some(function (t) { return t.key === "anomaly" })
                        ? root.tabDefs[3].src : ""
                onLoaded: root.injectPanelContext()
            }
        }
    }
}
