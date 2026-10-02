// CheckPage.qml - 检查页（数据质量审核与缺陷图像网格浏览）
// 布局：左侧栏(240px, 图库/数据集/程度图像/标签类别) + 可拖拽分割线 + 中央缺陷图像网格
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme
import LabelTorch.Components
import LabelTorch.Shell

Item {
    id: root

    // === 属性 ===
    property var sampleListData: []       // 所有样本数据
    property var filteredSamples: []      // 过滤后的样本
    property var selectedClassIds: []     // 选中的类别ID列表
    property int totalSamples: 0          // 总样本数
    property int annotatedSamples: 0      // 已标注样本数
    property int datasetCount: 0          // 数据集数量
    property string selectedDatasetId: "" // 当前选中的数据集ID
    property bool severityMode: false     // 程度图像模式开关
    property real sidebarWidth: Theme.sidebarWidth  // 侧边栏宽度（可拖拽调整）
    property var classCounts: ({})        // 按类别样本数统计 {classIndex: count}

    // === 生命周期 ===
    Component.onCompleted: {
        if (appController.projectOpen) refreshData()
    }

    onVisibleChanged: {
        if (visible && appController.projectOpen) refreshData()
    }

    // === 刷新数据 ===
    function refreshData() {
        if (!appController.projectOpen) {
            sampleListData = []
            filteredSamples = []
            datasetCount = 0
            return
        }
        // 获取当前项目的数据集列表
        var datasets = datasetService.listDatasets(appController.currentProjectId)
        datasetCount = datasets.length

        var allSamples = []
        var annotated = 0
        for (var d = 0; d < datasets.length; d++) {
            var ds = datasets[d]
            // P1-22：单数据集最多拉 2000 条做质量抽检，避免万张级全量进内存
            var samples = annotationService.listSamples(ds.id, 0, 2000)
            for (var s = 0; s < samples.length; s++) {
                var sample = samples[s]
                sample.datasetName = ds.name
                sample.datasetId = ds.id
                // 从标签文件提取所有类别索引（用于类别过滤和统计）
                sample.classIndices = []
                if (sample.labelPath) {
                    var annotations = annotationService.loadAnnotations(sample.labelPath)
                    if (annotations && annotations.length > 0) {
                        // 收集所有类别索引（去重）
                        var seen = {}
                        for (var a = 0; a < annotations.length; a++) {
                            var cid = annotations[a].classIndex
                            if (seen[cid] === undefined) {
                                seen[cid] = true
                                sample.classIndices.push(cid)
                            }
                        }
                        // 兼容字段：保留第一个类别索引用于旧逻辑
                        sample.classIndex = annotations[0].classIndex
                    }
                    annotated++
                }
                allSamples.push(sample)
            }
        }
        sampleListData = allSamples
        totalSamples = allSamples.length
        annotatedSamples = annotated
        computeClassCounts()
        applyClassFilter()
    }

    // === 计算每个类别的样本数（含未标注行，classIndex = -1） ===
    function computeClassCounts() {
        var counts = {}
        var unlabeled = 0
        for (var i = 0; i < sampleListData.length; i++) {
            var sample = sampleListData[i]
            if (sample.classIndices && sample.classIndices.length > 0) {
                for (var j = 0; j < sample.classIndices.length; j++) {
                    var cid = sample.classIndices[j]
                    counts[cid] = (counts[cid] || 0) + 1
                }
            } else {
                unlabeled += 1
            }
        }
        counts[-1] = unlabeled
        classCounts = counts
    }

    // === 按数据集/类别过滤 ===
    function applyClassFilter() {
        var filtered = []
        for (var i = 0; i < sampleListData.length; i++) {
            var sample = sampleListData[i]
            // 数据集过滤：选中数据集后仅保留该数据集样本
            if (selectedDatasetId !== "" && sample.datasetId !== selectedDatasetId)
                continue
            // 类别过滤：未选类别时保留全部；-1 = 未标注的图像；其余按包含任意选中类别匹配
            if (selectedClassIds.length > 0) {
                var matched = false
                var wantUnlabeled = selectedClassIds.indexOf(-1) >= 0
                if (wantUnlabeled && (!sample.classIndices || sample.classIndices.length === 0)) {
                    matched = true
                }
                if (!matched && sample.classIndices && sample.classIndices.length > 0) {
                    for (var j = 0; j < sample.classIndices.length; j++) {
                        if (selectedClassIds.indexOf(sample.classIndices[j]) >= 0) {
                            matched = true
                            break
                        }
                    }
                }
                if (!matched)
                    continue
            }
            filtered.push(sample)
        }
        filteredSamples = filtered
    }

    // === 全局筛选接口：数据集过滤 ===
    function setDatasetFilter(dsId) {
        selectedDatasetId = dsId || ""
        applyClassFilter()
    }

    // === 全局筛选接口：标签类别过滤（classIndex < 0 表示不过滤） ===
    function setClassFilter(clsIndex) {
        selectedClassIds = (clsIndex !== undefined && clsIndex !== null && Number(clsIndex) >= 0)
            ? [Number(clsIndex)] : []
        applyClassFilter()
    }

    // === 获取类别名 ===
    function getClassName(classIndex) {
        // IndexRole = Qt.UserRole + 2，返回值即行号（与 classIndex 对应）
        if (classIndex >= 0 && classIndex < taxonomyModel.rowCount()) {
            var idx = taxonomyModel.index(classIndex, 0)
            return taxonomyModel.data(idx, Qt.UserRole + 1) || ("class_" + classIndex)
        }
        return "class_" + classIndex
    }

    // ================================================================
    // 主布局：左侧栏 + 分割线 + 中央画廊
    // ================================================================
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // === 左侧栏 ===
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
                    spacing: Theme.spacingLarge

                // 项目名标题
                Text {
                    Layout.fillWidth: true
                    text: appController.projectOpen ? appController.currentProjectName : "未打开项目"
                    font.pixelSize: Theme.fontSizeSubheading  // 15px
                    font.weight: Font.Bold
                    color: Theme.textMain
                    elide: Text.ElideRight
                }

                // === 图库选择器 ===
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSmall  // 4px

                    Text {
                        text: "图库"
                        font.pixelSize: Theme.fontSizeCaption  // 11px
                        color: Theme.textMuted
                    }

                    // 选择器条：bgCard+border, 左侧icon, 中间ComboBox, 右侧"+"按钮
                    Rectangle {
                        Layout.fillWidth: true
                        height: 32
                        color: Theme.bgCard
                        radius: Theme.radiusSmall
                        border.color: Theme.borderColor
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingNormal
                            anchors.rightMargin: Theme.spacingSmall
                            spacing: Theme.spacingSmall

                            // 图库图标
                            SvgIcon {
                                icon: "images"
                                width: 14
                                height: 14
                                color: Theme.textMuted
                                Layout.alignment: Qt.AlignVCenter
                            }

                            // 数据集下拉选择
                            ComboBox {
                                id: datasetCombo
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                model: datasetService.listDatasets(appController.currentProjectId || "")
                                textRole: "name"
                                valueRole: "id"
                                font.pixelSize: Theme.fontSizeSmall

                                background: Rectangle {
                                    color: "transparent"
                                }

                                contentItem: Text {
                                    text: datasetCombo.displayText
                                    font.pixelSize: Theme.fontSizeSmall
                                    color: Theme.textMain
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }

                                indicator: Text {
                                    text: "▾"
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.textMuted
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.rightMargin: Theme.spacingSmall
                                    visible: false  // 隐藏默认指示器，由外层处理
                                }

                                popup: Popup {
                                    y: datasetCombo.height
                                    width: datasetCombo.width
                                    implicitHeight: contentItem.implicitHeight
                                    padding: 1

                                    contentItem: ListView {
                                        clip: true
                                        implicitHeight: contentHeight
                                        model: datasetCombo.popup.visible ? datasetCombo.delegateModel : null
                                        currentIndex: datasetCombo.highlightedIndex

                                        ScrollIndicator.vertical: ScrollIndicator {}
                                    }

                                    background: Rectangle {
                                        color: Theme.bgInputDropdown
                                        border.color: Theme.borderColor
                                        radius: Theme.radiusSmall
                                    }
                                }

                                onActivated: function(index) {
                                    var ds = datasetCombo.model[index]
                                    if (ds) selectedDatasetId = ds.id || ""
                                    refreshData()
                                }
                            }

                            // "+" 添加按钮
                            Rectangle {
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                Layout.alignment: Qt.AlignVCenter
                                color: addBtnMouse.containsMouse ? Theme.bgHover : "transparent"
                                radius: Theme.radiusSmall

                                Text {
                                    anchors.centerIn: parent
                                    text: "+"
                                    font.pixelSize: Theme.fontSizeNormal
                                    font.weight: Font.Bold
                                    color: Theme.primaryGlow
                                }

                                MouseArea {
                                    id: addBtnMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        // 切换到导入页面
                                        appController.currentPage = "dataset"
                                    }
                                }
                            }
                        }
                    }
                }

                // === 数据集(N) 可折叠区块 ===
                CollapsibleSection {
                    id: datasetSection
                    Layout.fillWidth: true
                    title: "数据集(" + datasetCount + ")"

                    ColumnLayout {
                        width: parent.width
                        spacing: Theme.spacingSmall

                        // 数据集列表
                        ListView {
                            id: datasetList
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(contentHeight, 120)
                            clip: true
                            model: datasetService.listDatasets(appController.currentProjectId || "")
                            spacing: Theme.spacingSmall

                            delegate: Rectangle {
                                width: datasetList.width
                                height: 36
                                color: Theme.bgCard
                                radius: Theme.radiusSmall
                                border.color: Theme.borderColor
                                border.width: 1

                                property var dsData: modelData
                                property int sampleCount: dsData ? (dsData.sampleCount || 0) : 0
                                property int annotatedCount: dsData ? (dsData.annotatedCount || sampleCount) : 0

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.spacingNormal
                                    anchors.rightMargin: Theme.spacingNormal
                                    spacing: Theme.spacingSmall

                                    // 数据集名称
                                    Text {
                                        text: dsData ? dsData.name : ""
                                        font.pixelSize: Theme.fontSizeCaption
                                        color: Theme.textMain
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    // 进度条
                                    Rectangle {
                                        Layout.preferredWidth: 50
                                        Layout.preferredHeight: 6
                                        radius: 3
                                        color: Theme.bgInput

                                        Rectangle {
                                            width: parent.width * (annotatedCount > 0 && sampleCount > 0 ? Math.min(1, annotatedCount / sampleCount) : 0)
                                            height: parent.height
                                            radius: 3
                                            color: Theme.primaryGlow
                                        }
                                    }

                                    // 数量文字
                                    Text {
                                        text: annotatedCount + "/" + sampleCount
                                        font.pixelSize: 10
                                        font.family: Theme.fontFamilyMono
                                        color: Theme.textMuted
                                    }

                                    // 定位图标
                                    Text {
                                        text: "⊙"
                                        font.pixelSize: Theme.fontSizeCaption
                                        color: Theme.textMuted
                                        Layout.alignment: Qt.AlignVCenter

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                // 定位到该数据集并应用过滤
                                                selectedDatasetId = dsData ? dsData.id : ""
                                                applyClassFilter()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // hr 分割线
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.dividerColor
                }

                // === 程度图像 可折叠区块 ===
                CollapsibleSection {
                    id: severitySection
                    Layout.fillWidth: true
                    title: "程度图像"

                    ColumnLayout {
                        width: parent.width
                        spacing: Theme.spacingNormal

                        // 程度图像模式: label + ToggleSwitch
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingNormal

                            Text {
                                text: "程度图像模式"
                                font.pixelSize: Theme.fontSizeCaption
                                color: Theme.textMain
                                Layout.fillWidth: true
                            }

                            ToggleSwitch {
                                id: severityToggle
                                checked: root.severityMode
                                onToggled: {
                                    root.severityMode = checked
                                }
                            }
                        }

                        // 标注程度模型按钮
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            color: severityBtnMouse.containsMouse ? Theme.bgHover : Theme.bgCard
                            radius: Theme.radiusSmall
                            border.color: Theme.borderColor
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "标注程度模型"
                                font.pixelSize: Theme.fontSizeCaption
                                color: Theme.textMuted
                            }

                            MouseArea {
                                id: severityBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    // 切换程度模式并跳转标注页
                                    root.severityMode = !root.severityMode
                                    if (root.severityMode) {
                                        appController.currentPage = "annotation"
                                    }
                                }
                            }
                        }
                    }
                }

                // hr 分割线
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.dividerColor
                }

                // === 标签类别 可折叠区块 ===
                CollapsibleSection {
                    id: classFilterSection
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "标签类别"

                    ColumnLayout {
                        width: parent.width
                        spacing: Theme.spacingSmall

                        // 副标题 + 清除按钮
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSmall

                            Text {
                                Layout.fillWidth: true
                                text: "若要显示实例的子集，请选择一个或多个标签类别"
                                font.pixelSize: 10
                                color: Theme.textMuted
                                wrapMode: Text.WordWrap
                                Layout.maximumHeight: 30
                            }

                            // ✕ 清除按钮
                            Rectangle {
                                Layout.preferredWidth: 18
                                Layout.preferredHeight: 18
                                color: clearBtnMouse.containsMouse ? Theme.bgHover : "transparent"
                                radius: Theme.radiusSmall
                                visible: selectedClassIds.length > 0

                                Text {
                                    z: 10
                                    anchors.centerIn: parent
                                    text: "✕"
                                    font.pixelSize: 10
                                    color: Theme.textMuted
                                }

                                MouseArea {
                                    id: clearBtnMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        selectedClassIds = []
                                        applyClassFilter()
                                    }
                                }
                            }
                        }

                        // 未标注行（对标 DLTools「未标注的图像 N」，classIndex = -1）
                        Rectangle {
                            id: unlabeledRow
                            width: classFilterList.width
                            height: 28
                            radius: Theme.radiusSmall
                            property bool isSelected: selectedClassIds.indexOf(-1) >= 0
                            color: isSelected ? Theme.bgSelected : (unlabeledMouse.containsMouse ? Theme.bgHover : "transparent")

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.spacingNormal
                                anchors.rightMargin: Theme.spacingNormal
                                spacing: Theme.spacingNormal

                                Rectangle {
                                    Layout.preferredWidth: 12
                                    Layout.preferredHeight: 12
                                    radius: 2
                                    color: "transparent"
                                    border.color: Theme.textMuted
                                    border.width: 1
                                    opacity: unlabeledRow.isSelected ? 1.0 : 0.5
                                }

                                Text {
                                    text: "未标注的图像"
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: unlabeledRow.isSelected ? Theme.primary : Theme.textMain
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    text: classCounts[-1] || 0
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.family: Theme.fontFamilyMono
                                    color: Theme.textMuted
                                }
                            }

                            MouseArea {
                                id: unlabeledMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var ids = selectedClassIds.slice()
                                    var pos = ids.indexOf(-1)
                                    if (pos >= 0) ids.splice(pos, 1)
                                    else ids.push(-1)
                                    selectedClassIds = ids
                                    applyClassFilter()
                                }
                            }
                        }

                        // 类别列表
                        ListView {
                            id: classFilterList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 40
                            clip: true
                            model: taxonomyModel
                            spacing: 1

                            delegate: Rectangle {
                                width: classFilterList.width
                                height: 28
                                color: isSelected ? Theme.bgSelected : (classItemMouse.containsMouse ? Theme.bgHover : "transparent")
                                radius: Theme.radiusSmall

                                property bool isSelected: selectedClassIds.indexOf(model.classIndex) >= 0

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.spacingNormal
                                    anchors.rightMargin: Theme.spacingNormal
                                    spacing: Theme.spacingNormal

                                    // 色块（12x12）
                                    Rectangle {
                                        Layout.preferredWidth: 12
                                        Layout.preferredHeight: 12
                                        radius: 2
                                        color: Theme.classColors[model.classIndex % Theme.classColors.length]
                                        opacity: isSelected ? 1.0 : 0.5

                                        // 选中边框
                                        Rectangle {
                                            anchors.fill: parent
                                            radius: 2
                                            border.color: Theme.textMain
                                            border.width: isSelected ? 2 : 0
                                            visible: isSelected
                                            color: "transparent"
                                        }
                                    }

                                    // 类别名
                                    Text {
                                        text: model.className
                                        font.pixelSize: Theme.fontSizeCaption
                                        color: isSelected ? Theme.primary : Theme.textMain
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    // 数量（monospace）- 按类别样本数统计
                                    Text {
                                        text: classCounts[model.classIndex] || 0
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamilyMono
                                        color: Theme.textMuted
                                        visible: !classItemMouse.containsMouse
                                    }

                                    // 行内编辑（✎）/删除（✕）：悬停时显示
                                    Text {
                                        z: 10
                                        visible: classItemMouse.containsMouse
                                        text: "\u270F"
                                        font.pixelSize: 11
                                        color: classItemMouse.containsMouse ? Theme.primary : Theme.textMuted

                                        MouseArea {
                                            anchors.fill: parent
                                            anchors.margins: -4
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: classStyleDialog.openFor(model.classIndex)
                                        }
                                    }

                                    Text {
                                        z: 10
                                        visible: classItemMouse.containsMouse
                                        text: "\u2715"
                                        font.pixelSize: 11
                                        color: delRowMouse.containsMouse ? Theme.danger : Theme.textMuted

                                        MouseArea {
                                            id: delRowMouse
                                            anchors.fill: parent
                                            anchors.margins: -4
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: taxonomyModel.removeClass(model.classIndex)
                                        }
                                    }
                                }

                                MouseArea {
                                    id: classItemMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var idx = selectedClassIds.indexOf(model.classIndex)
                                        var newIds = selectedClassIds.slice()
                                        if (idx >= 0) newIds.splice(idx, 1)
                                        else newIds.push(model.classIndex)
                                        selectedClassIds = newIds
                                        applyClassFilter()
                                    }
                                }
                            }

                            // "未标注的图像" 特殊项
                            footer: Rectangle {
                                width: classFilterList.width
                                height: 28
                                color: unannotatedMouse.containsMouse ? Theme.bgHover : "transparent"
                                radius: Theme.radiusSmall

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.spacingNormal
                                    anchors.rightMargin: Theme.spacingNormal
                                    spacing: Theme.spacingNormal

                                    // 黑色色块+border
                                    Rectangle {
                                        Layout.preferredWidth: 12
                                        Layout.preferredHeight: 12
                                        radius: 2
                                        color: Theme.bgChart
                                        border.color: Theme.borderColor
                                        border.width: 1
                                    }

                                    Text {
                                        text: "未标注的图像"
                                        font.pixelSize: Theme.fontSizeCaption
                                        color: Theme.textMuted
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: (totalSamples - annotatedSamples).toString()
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamilyMono
                                        color: Theme.textMuted
                                    }
                                }

                                MouseArea {
                                    id: unannotatedMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                }
                            }
                        }
                    }
                }
            }
        }
    }

        // === 可拖拽垂直分割线 (4px) ===
        Splitter {
            id: resizer
            Layout.fillHeight: true
            vertical: true
            minSize: Theme.sidebarMinWidth
            maxSize: 600
            targetItem: sidebar
        }

        // === 中央缺陷图像网格 ===
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgMain

            // 空状态：统一 EmptyState，给出导入/标注双入口
            EmptyState {
                anchors.centerIn: parent
                width: parent.width - Theme.spacingXLarge * 2
                visible: filteredSamples.length === 0
                icon: "images"
                title: "暂无样本"
                description: datasetCount === 0
                             ? "当前项目还没有数据集，请先导入数据"
                             : "当前筛选条件下没有匹配的样本"
                actionText: datasetCount === 0 ? "导入数据" : "清除筛选"
                secondaryText: datasetCount === 0 ? "前往标注" : "导入数据"
                onActionClicked: {
                    if (datasetCount === 0)
                        appController.currentPage = "dataset"
                    else {
                        selectedClassIds = []
                        severityMode = false
                        refreshData()
                    }
                }
                onSecondaryClicked: {
                    if (datasetCount === 0)
                        appController.currentPage = "annotation"
                    else
                        appController.currentPage = "dataset"
                }
            }

            // 缺陷图像网格（auto-fill, minmax(130px, 1fr)）
            GridView {
                id: imageGrid
                anchors.fill: parent
                anchors.margins: Theme.spacingXLarge  // 20px padding
                clip: true
                model: filteredSamples
                // auto-fill: cellWidth=130+12=142, 动态计算列数
                cellWidth: 142
                cellHeight: 154  // 130 + 底部标签空间
                visible: filteredSamples.length > 0

                // 动态调整cellWidth以实现auto-fill效果
                // columns/cellWidth 钳制：width 未就绪时 Math.floor(NaN) 会把 NaN 写进 cellWidth，
                // 进而污染整个 GridView 的布局几何（Qt 6.11 Debug 下触发 NaN ASSERT）
                property int columns: {
                    if (!isFinite(width) || width <= 0) return 1
                    return Math.max(1, Math.floor(width / 142))
                }
                onWidthChanged: {
                    if (!isFinite(width) || width <= 0) {
                        cellWidth = 142
                        cellHeight = 154
                        return
                    }
                    cellWidth = width / columns
                    cellHeight = cellWidth * 1.08  // 近似正方形+底部空间
                }

                delegate: Item {
                    width: imageGrid.cellWidth
                    height: imageGrid.cellHeight

                    property var sampleData: modelData
                    property string fileName: sampleData.imagePath ? sampleData.imagePath.split('/').pop().split('\\').pop() : ""

                    // 程度模式：读取样本的分类标注（程度分级存为 classification label）
                    property int sevClassId: {
                        if (!severityMode || !sampleData.labelPath) return -1
                        var labels = annotationService.loadClassificationLabels(sampleData.labelPath || "")
                        return labels.classId !== undefined ? labels.classId : -1
                    }

                    // 缩略图卡片：黑色背景, borderColor, 6px圆角
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingSmall + Theme.spacingTiny  // 6px间距
                        color: Theme.bgChart
                        radius: Theme.radiusNormal  // 6px
                        border.color: thumbMouse.containsMouse ? Theme.primaryGlow : Theme.borderColor
                        border.width: thumbMouse.containsMouse ? 2 : 1
                        clip: true

                        // 图片：width:140%, height:140%, object-fit:cover, 居中偏移
                        Image {
                            id: thumbnail
                            anchors.fill: parent
                            anchors.margins: 1
                            source: sampleData.imagePath ? ("file:///" + sampleData.imagePath.replace(/\\/g, "/")) : ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: true
                            layer.enabled: true
                            layer.mipmap: true
                            // 模拟140%缩放+居中裁切效果，向上偏移20%实现top:-20%
                            y: isFinite(height) ? -height * 0.2 : 0
                            // sourceSize 必须是有限正数：width/height 在布局未就绪时可能为 0/NaN，
                            // NaN 进入图像缩放管线会在 qCheckedFPConversionToInteger 处触发 Debug ASSERT
                            sourceSize.width: {
                                var w = width * 1.4
                                return (isFinite(w) && w > 0) ? Math.round(w) : 256
                            }
                            sourceSize.height: {
                                var h = height * 1.4
                                return (isFinite(h) && h > 0) ? Math.round(h) : 256
                            }

                            // 加载中占位
                            Rectangle {
                                anchors.fill: parent
                                color: Theme.bgInput
                                visible: thumbnail.status !== Image.Ready

                                Text {
                                    anchors.centerIn: parent
                                    text: "..."
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSizeLarge
                                }
                            }
                        }

                        // 底部徽章：程度模式显示程度分级（轻微/中等/严重），否则显示缺陷类别
                        Rectangle {
                            anchors.left: parent.left
                            anchors.bottom: parent.bottom
                            anchors.margins: Theme.spacingSmall
                            height: 16
                            width: badgeText.implicitWidth + Theme.spacingNormal
                            radius: Theme.radiusSmall - 1  // 3px
                            // 程度模式：色块按程度类别的 classIndex 取色
                            color: severityMode
                                   ? (sevClassId >= 0 ? Qt.alpha(Theme.classColors[sevClassId % Theme.classColors.length], 0.85)
                                                      : Qt.alpha(Theme.textMuted, 0.85))
                                   : (sampleData.classIndex !== undefined
                                      ? Qt.alpha(Theme.classColors[sampleData.classIndex % Theme.classColors.length], 0.85)
                                      : Qt.alpha(Theme.danger, 0.85))
                            visible: severityMode ? true
                                    : (sampleData.classIndex !== undefined && sampleData.classIndex >= 0)

                            Text {
                                id: badgeText
                                anchors.centerIn: parent
                                text: severityMode
                                      ? (sevClassId >= 0 ? getClassName(sevClassId) : "未标注程度")
                                      : getClassName(sampleData.classIndex || 0)
                                font.pixelSize: 10
                                color: Theme.logoBgText
                            }
                        }

                        // 悬停交互
                        MouseArea {
                            id: thumbMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor

                            onClicked: {
                                // 点击跳转到标注页
                                appController.currentPage = "annotation"
                            }

                            onDoubleClicked: {
                                // 双击打开标注页并加载该样本
                                appController.currentPage = "annotation"
                            }
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
            }

            // 底部状态栏
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: Theme.footerHeight
                color: Theme.bgCard
                z: 10

                // 顶部分割线
                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 1
                    color: Theme.dividerColor
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingNormal

                    Text {
                        text: "已筛选的图像: " + filteredSamples.length + "/" + totalSamples
                        font.pixelSize: Theme.fontSizeCaption
                        color: Theme.textMuted
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: selectedClassIds.length > 0 ? "已选 " + selectedClassIds.length + " 个类别" : "全部类别"
                        font.pixelSize: Theme.fontSizeCaption
                        color: selectedClassIds.length > 0 ? Theme.primary : Theme.textMuted
                    }
                }
            }
        }
    }

    // === 信号连接 ===
    Connections {
        target: appController
        function onCurrentProjectIdChanged() {
            refreshData()
        }
    }

    // === 类别样式编辑弹窗（行内 ✎ 唤起：改名/颜色/快捷键一体） ===
    ClassStyleDialog {
        id: classStyleDialog
    }
}
