// DatasetPage.qml - V6 数据集页（像素级复刻参考UI设计）
// 左侧sidebar(240px) + resizer-v(4px) + 中心缩略图网格
// 布局：图库选择器 → 数据集卡片列表 → 图像属性 → 标签
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LabelTorch.Theme
import LabelTorch.Components
import LabelTorch.Shell

Item {
    id: pageRoot

    // === 当前选中状态 ===
    property string currentDatasetId: ""
    property string currentDatasetName: ""
    property var selectedSample: null
    property int totalSamples: 0
    property int labeledSamples: 0
    property string selectedTag: "默认"
    property string galleryFilterLabel: "全部图像"
    property var rawSamples: []
    property int filterClassIndex: -1   // 全局类别过滤（-1 = 不限）
    // P1-21：当前项目缩略图缓存目录（cache/thumbnails）
    property string thumbCacheDir: ""
    // 图像 Tag 体系：tagId → 名称 映射（供筛选与角标显示）
    property var tagNameById: ({})
    // Tag 筛选选项模型："全部Tag" / "无Tag" / 各 Tag 名称
    property var sampleTagOptions: ["全部Tag"]

    // P1-21：解析缩略图路径——已生成则走缩略图，否则回退原图并靠 sourceSize 限解码
    function resolveThumbSource(imagePath) {
        if (!imagePath) return ""
        var raw = String(imagePath).replace(/^file:\/\/\//, "").replace(/\\/g, "/")
        if (pageRoot.thumbCacheDir) {
            var thumb = thumbnailGenerator.resolve(raw, pageRoot.thumbCacheDir)
            if (thumb && thumb.length > 0) {
                return "file:///" + thumb.replace(/\\/g, "/")
            }
        }
        return "file:///" + raw
    }

    // === 页面初始化与可见性刷新 ===
    Component.onCompleted: {
        if (appController.currentProjectId !== "") {
            datasetModel.setProjectId(appController.currentProjectId)
        }
    }

    onVisibleChanged: {
        if (visible && appController.currentProjectId !== "") {
            datasetModel.setProjectId(appController.currentProjectId)
        }
    }

    // === 监听项目切换，清空状态 ===
    Connections {
        target: appController
        function onCurrentProjectIdChanged() {
            datasetModel.setProjectId(appController.currentProjectId)
            sampleListModel.clear()
            rawSamples = []
            currentDatasetId = ""
            currentDatasetName = ""
            selectedSample = null
            tagModel.setDatasetId("")  // A6：清空标签列表
        }
    }

    // A6：当前数据集变化时刷新标签列表
    onCurrentDatasetIdChanged: {
        // 图像 Tag 体系：数据集首次使用时播种内置评审 Tag（默认/良品/漏检/误检/待定/重要）
        if (currentDatasetId !== "") {
            tagService.ensureBuiltinTags(currentDatasetId)
        }
        tagModel.setDatasetId(currentDatasetId)
        tagModel.refresh()
        pageRoot.rebuildTagMaps()
    }

    // 从 tagModel 重建 tagId→名称 映射与筛选下拉选项
    function rebuildTagMaps() {
        var nameById = {}
        var options = ["全部Tag", "无Tag"]
        if (typeof tagModel !== "undefined") {
            for (var i = 0; i < tagModel.rowCount(); i++) {
                var idx = tagModel.index(i, 0)
                var id = tagModel.data(idx, Qt.UserRole + 1)    // IdRole
                var n = tagModel.data(idx, Qt.UserRole + 2)     // NameRole
                if (id && n) {
                    nameById[id] = n
                    if (options.indexOf(n) < 0) options.push(n)
                }
            }
        }
        pageRoot.tagNameById = nameById
        pageRoot.sampleTagOptions = options
        galleryFilterBar.sampleTagModel = options
    }

    // 将 Tag 指派给样本（tagName 为空串表示清除），落库并刷新本地行
    function assignSampleTag(sampleId, tagName) {
        if (!sampleId) return
        var tagId = ""
        if (tagName !== "") {
            for (var tid in tagNameById) {
                if (tagNameById[tid] === tagName) { tagId = tid; break }
            }
            if (!tagId) {
                if (typeof ToastBus !== "undefined") ToastBus.error("未找到标签：" + tagName)
                return
            }
        }
        if (!tagService.setSampleTag(sampleId, tagId)) {
            if (typeof ToastBus !== "undefined") ToastBus.error("标记失败：" + tagName)
            return
        }
        // 同步内存中的样本行与选中态
        var name = tagName
        for (var i = 0; i < rawSamples.length; ++i) {
            if (rawSamples[i].sampleId === sampleId) {
                rawSamples[i].tagId = tagId
                break
            }
        }
        if (selectedSample && selectedSample.sampleId === sampleId) {
            selectedSample.tagId = tagId
            selectedTag = name
        }
        applySampleFilters()
    }

    // === 监听扫描完成信号 ===
    Connections {
        target: datasetService
        function onScanFolderFinished(result) { importDialogRoot.isScanning = false; importDialogRoot.scanResult = result }
        function onScanSeparateFinished(result) { importDialogRoot.isScanning = false; importDialogRoot.scanResult = result }
    }

    // ================================================================
    // 主布局：SplitView (sidebar + center)
    // ================================================================
    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        handle: Rectangle {
            implicitWidth: 4
            color: SplitHandle.pressed ? Theme.primaryGlow : (SplitHandle.hovered ? Theme.primaryGlow : Theme.borderColor)
            Behavior on color { ColorAnimation { duration: Theme.animDurationFast } }
        }

        // ============================================================
        // 左侧边栏 (240px, padding:12px, gap:16px)
        // ============================================================
        Rectangle {
            id: sidebarRect
            SplitView.preferredWidth: Theme.sidebarWidth
            SplitView.minimumWidth: Theme.sidebarMinWidth
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
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width - 24
                    anchors.leftMargin: Theme.spacingLarge - Theme.spacingNormal  // 12px
                    anchors.rightMargin: Theme.spacingLarge - Theme.spacingNormal
                    spacing: Theme.spacingLarge  // 16px gap

                    // === 项目名标题 (15px bold, letter-spacing:0.5px) ===
                    Text {
                        Layout.fillWidth: true
                        text: appController.projectOpen ? "标炬 · 数据集" : "请先打开项目"
                        font.pixelSize: Theme.fontSizeSubheading  // 15px
                        font.weight: Font.Bold
                        font.family: Theme.fontFamily
                        font.letterSpacing: 0.5
                        color: appController.projectOpen ? Theme.textMain : Theme.textMuted
                        elide: Text.ElideRight
                    }

                    // === 图库选择器 ===
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSmall  // 4px

                        // label "图库" (11px muted)
                        Text {
                            text: "图库"
                            font.pixelSize: Theme.fontSizeCaption  // 11px
                            font.family: Theme.fontFamily
                            color: Theme.textMuted
                        }

                        // selector bar: bgCard+border, 左icon + ComboBox + 右"+"按钮
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            color: Theme.bgCard
                            radius: Theme.radiusSmall
                            border.color: datasetCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.spacingNormal
                                anchors.rightMargin: Theme.spacingSmall
                                spacing: Theme.spacingSmall

                                // 左侧图库图标
                                SvgIcon {
                                    icon: "images"
                                    width: 14
                                    height: 14
                                    color: Theme.primaryGlow
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                // 中间ComboBox
                                ComboBox {
                                    id: datasetCombo
                                    Layout.fillWidth: true
                                    model: datasetModel
                                    textRole: "name"
                                    valueRole: "datasetId"
                                    currentIndex: -1

                                    background: Rectangle { color: "transparent" }

                                    contentItem: Text {
                                        text: datasetCombo.displayText
                                        color: Theme.textMain
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.family: Theme.fontFamily
                                        leftPadding: 0
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                    }

                                    delegate: ItemDelegate {
                                        width: datasetCombo.width
                                        contentItem: Text {
                                            text: model.name
                                            color: highlighted ? Theme.textMain : Theme.textMuted
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamily
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        highlighted: datasetCombo.highlightedIndex === index
                                        background: Rectangle { color: highlighted ? Theme.bgHover : Theme.bgInputDropdown }
                                    }

                                    indicator: Canvas {
                                        width: 10
                                        height: 6
                                        anchors.verticalCenter: parent.verticalCenter
                                        onPaint: {
                                            var ctx = getContext("2d")
                                            ctx.reset()
                                            ctx.fillStyle = Theme.textMuted.toString()
                                            ctx.moveTo(0, 0)
                                            ctx.lineTo(width, 0)
                                            ctx.lineTo(width / 2, height)
                                            ctx.closePath()
                                            ctx.fill()
                                        }
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
                                        }

                                        background: Rectangle {
                                            color: Theme.bgInputDropdown
                                            border.color: Theme.borderColor
                                            radius: Theme.radiusSmall
                                        }
                                    }

                                    onActivated: {
                                        if (currentIndex >= 0) {
                                            var dsId = datasetModel.data(datasetModel.index(currentIndex), 257)
                                            selectDataset(dsId)
                                        }
                                    }
                                }

                                // 右侧"+"按钮
                                Rectangle {
                                    Layout.preferredWidth: 22
                                    Layout.preferredHeight: 22
                                    Layout.alignment: Qt.AlignVCenter
                                    radius: Theme.radiusSmall
                                    color: addDsBtnMouse.containsMouse ? Theme.primary : "transparent"
                                    border.color: Theme.primary
                                    border.width: 1

                                    SvgIcon {
                                        anchors.centerIn: parent
                                        icon: "plus"
                                        width: 10
                                        height: 10
                                        color: addDsBtnMouse.containsMouse ? Theme.textMain : Theme.primary
                                    }

                                    MouseArea {
                                        id: addDsBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: importDialogRoot.open()
                                    }
                                }
                            }
                        }
                    }

                    // === 数据集(N) 可折叠区块 ===
                    CollapsibleSection {
                        Layout.fillWidth: true
                        title: "数据集(" + datasetModel.rowCount() + ")"
                        expanded: true

                        ColumnLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            spacing: Theme.spacingSmall

                            // header 右侧图标行（眼睛/添加/导入）
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSmall

                                Item { Layout.fillWidth: true }

                                // 眼睛图标
                                SvgIcon {
                                    icon: "eye"
                                    width: 12
                                    height: 12
                                    color: visBtn.containsMouse ? Theme.primaryGlow : Theme.textMuted
                                    MouseArea {
                                        id: visBtn
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                    }
                                }
                                // 添加图标
                                SvgIcon {
                                    icon: "plus"
                                    width: 12
                                    height: 12
                                    color: addBtn.containsMouse ? Theme.primaryGlow : Theme.textMuted
                                    MouseArea {
                                        id: addBtn
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: importDialogRoot.open()
                                    }
                                }
                                // 导入向导（完整导入页，含扫描分析与类别映射入口）
                                Text {
                                    text: "导入向导"
                                    color: wizardBtn.containsMouse ? Theme.primaryGlow : Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.family: Theme.fontFamily
                                    verticalAlignment: Text.AlignVCenter
                                    MouseArea {
                                        id: wizardBtn
                                        anchors.fill: parent
                                        anchors.margins: -4
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: appController.currentPage = "import"
                                    }
                                }
                                // 类别映射（源 schema → 类别体系）
                                Text {
                                    text: "类别映射"
                                    color: mapBtn.containsMouse ? Theme.primaryGlow : Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.family: Theme.fontFamily
                                    verticalAlignment: Text.AlignVCenter
                                    MouseArea {
                                        id: mapBtn
                                        anchors.fill: parent
                                        anchors.margins: -4
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: appController.currentPage = "classmap"
                                    }
                                }
                                // 导入图标
                                SvgIcon {
                                    icon: "export"
                                    width: 12
                                    height: 12
                                    color: impBtn.containsMouse ? Theme.primaryGlow : Theme.textMuted
                                    MouseArea {
                                        id: impBtn
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: importDialogRoot.open()
                                    }
                                }
                            }

                            // 数据集卡片列表
                            ListView {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(contentHeight, 200)
                                clip: true
                                model: datasetModel
                                spacing: Theme.spacingSmall

                                delegate: Rectangle {
                                    width: ListView.view.width
                                    height: 44
                                    color: {
                                        if (currentDatasetId === model.datasetId) return Theme.bgSelected
                                        if (dsItemMouse.containsMouse) return Theme.bgHover
                                        return Theme.bgCard
                                    }
                                    radius: Theme.radiusSmall
                                    border.color: currentDatasetId === model.datasetId ? Theme.primaryGlow : Theme.borderColor
                                    border.width: 1

                                    // 选中态左侧指示条
                                    Rectangle {
                                        visible: currentDatasetId === model.datasetId
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: 3
                                        radius: 1
                                        color: Theme.primaryGlow
                                    }

                                    MouseArea {
                                        id: dsItemMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: selectDataset(model.datasetId)
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: Theme.spacingNormal
                                        anchors.rightMargin: Theme.spacingNormal
                                        spacing: Theme.spacingSmall

                                        // 数据集名称
                                        Text {
                                            text: model.name
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamily
                                            font.bold: currentDatasetId === model.datasetId
                                            color: currentDatasetId === model.datasetId ? Theme.primaryGlow : Theme.textMain
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        // 进度条 (50x6px, bgMain底 + primaryGlow填充)
                                        Rectangle {
                                            Layout.preferredWidth: 50
                                            Layout.preferredHeight: 6
                                            radius: 3
                                            color: Theme.bgMain

                                            Rectangle {
                                                width: parent.width * (labeledSamples > 0 && totalSamples > 0 ? labeledSamples / totalSamples : 0)
                                                height: parent.height
                                                radius: 3
                                                gradient: Gradient {
                                                    orientation: Gradient.Horizontal
                                                    GradientStop { position: 0.0; color: Theme.primary }
                                                    GradientStop { position: 1.0; color: Theme.primaryGlow }
                                                }
                                            }
                                        }

                                        // 样本数 "32/32"
                                        Text {
                                            text: model.sampleCount + "/" + model.sampleCount
                                            font.pixelSize: Theme.fontSizeCaption
                                            font.family: Theme.fontFamilyMono
                                            color: Theme.textMuted
                                        }

                                        // 定位图标
                                        SvgIcon {
                                            icon: "marker"
                                            width: 12
                                            height: 12
                                            color: Theme.textMuted
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // === hr 分割线 ===
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.dividerColor
                    }

                    // === 图像属性 可折叠区块 ===
                    CollapsibleSection {
                        Layout.fillWidth: true
                        title: "图像属性"
                        expanded: true

                        ColumnLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            spacing: Theme.spacingTiny

                            // 未选中提示
                            Text {
                                visible: !selectedSample
                                text: "选择图像查看属性"
                                font.pixelSize: Theme.fontSizeSmall
                                font.family: Theme.fontFamily
                                color: Theme.textMuted
                            }

                            // key-value 属性对
                            ColumnLayout {
                                visible: selectedSample !== null
                                Layout.fillWidth: true
                                spacing: Theme.spacingTiny

                                // 名称
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSmall
                                    Text {
                                        text: "名称"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: Theme.textMuted
                                        Layout.preferredWidth: 48
                                    }
                                    Text {
                                        text: selectedSample ? (selectedSample.fileName || "") : ""
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: Theme.textMain
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }
                                // 路径
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSmall
                                    Text {
                                        text: "路径"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: Theme.textMuted
                                        Layout.preferredWidth: 48
                                    }
                                    Text {
                                        text: selectedSample ? (selectedSample.imagePath || "") : ""
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamilyMono
                                        color: Theme.textMuted
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                }
                                // 大小
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSmall
                                    Text {
                                        text: "大小"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: Theme.textMuted
                                        Layout.preferredWidth: 48
                                    }
                                    Text {
                                        text: selectedSample ? (selectedSample.width || 0) + "×" + (selectedSample.height || 0) : ""
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamilyMono
                                        color: Theme.textMuted
                                    }
                                }
                                // 标签实例
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSmall
                                    Text {
                                        text: "标签"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: Theme.textMuted
                                        Layout.preferredWidth: 48
                                    }
                                    Text {
                                        text: selectedSample ? (selectedSample.labelCount || 0) + " 个实例" : ""
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: Theme.textMuted
                                    }
                                }
                                // 类别
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSmall
                                    Text {
                                        text: "类别"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: Theme.textMuted
                                        Layout.preferredWidth: 48
                                    }
                                    Text {
                                        text: currentDatasetName || "—"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: Theme.textMuted
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }

                    // === hr 分割线 ===
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.dividerColor
                    }

                    // === 标签可折叠区块 ===
                    CollapsibleSection {
                        Layout.fillWidth: true
                        title: "标签"
                        expanded: true

                        ColumnLayout {
                            id: tagsSection
                            anchors.left: parent.left
                            anchors.right: parent.right
                            spacing: Theme.spacingSmall

                            // header 右侧"+"按钮
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSmall

                                Text {
                                    text: currentDatasetName ? "所属数据集: " + currentDatasetName : "请先选择数据集"
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.family: Theme.fontFamily
                                    color: Theme.textMuted
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }

                                // "+"按钮
                                Rectangle {
                                    Layout.preferredWidth: 20
                                    Layout.preferredHeight: 20
                                    radius: Theme.radiusSmall
                                    color: addTagBtnMouse.containsMouse ? Theme.primary : "transparent"
                                    border.color: Theme.primary
                                    border.width: 1

                                    SvgIcon {
                                        anchors.centerIn: parent
                                        icon: "plus"
                                        width: 10
                                        height: 10
                                        color: addTagBtnMouse.containsMouse ? Theme.textMain : Theme.primary
                                    }

                                    MouseArea {
                                        id: addTagBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: addTagDialog.open()
                                    }
                                }
                            }

                            // 2列grid按钮：优先展示库内 Tag，无数据时回退内置默认标签
                            // 避免写死列表导致「添加 Tag 后看不见」
                            property var fallbackTags: ["默认", "良品", "漏检", "误检", "待定", "重要"]
                            property var displayTags: {
                                var names = []
                                if (typeof tagModel !== "undefined") {
                                    for (var i = 0; i < tagModel.rowCount(); i++) {
                                        var idx = tagModel.index(i, 0)
                                        var n = tagModel.data(idx, Qt.UserRole + 2) // NameRole
                                        if (n && names.indexOf(n) < 0) names.push(n)
                                    }
                                }
                                return names.length > 0 ? names : fallbackTags
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                rowSpacing: Theme.spacingTiny
                                columnSpacing: Theme.spacingTiny

                                Repeater {
                                    model: tagsSection.displayTags

                                    Rectangle {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 26
                                        radius: Theme.radiusSmall
                                        color: selectedTag === modelData ? Theme.primary : Theme.bgCard
                                        border.color: selectedTag === modelData ? Theme.primary : Theme.borderColor
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            // 选中图像时显示其当前 Tag，否则显示待指派 Tag（带快捷键）
                                            text: {
                                                var cur = selectedSample && selectedSample.tagId ? (tagNameById[selectedSample.tagId] || "") : ""
                                                if (cur) return cur === modelData ? ("✓ " + modelData) : modelData
                                                return tagChipLabel(modelData)
                                            }
                                            font.pixelSize: Theme.fontSizeCaption
                                            font.family: Theme.fontFamily
                                            color: selectedTag === modelData ? Theme.textMain : Theme.textMuted
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                // 落库打标：仅当选中了样本（无选中时只做本地高亮）
                                                if (selectedSample && selectedSample.sampleId) {
                                                    assignSampleTag(selectedSample.sampleId, selectedTag === modelData ? "" : modelData)
                                                } else {
                                                    selectedTag = selectedTag === modelData ? "" : modelData
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // === 底部弹性空间 ===
                    Item { Layout.fillHeight: true }
                }
            }
        }

        // ============================================================
        // 中心缩略图画廊网格
        // ============================================================
        Rectangle {
            SplitView.fillWidth: true
            color: Theme.bgMain

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                FilterBar {
                    id: galleryFilterBar
                    Layout.fillWidth: true
                    visible: appController.projectOpen && currentDatasetId !== ""
                    datasetModel: currentDatasetName ? [currentDatasetName] : ["全部数据集"]
                    tagModel: ["全部状态", "已标注", "未标注", "有效", "异常"]
                    labelModel: ["全部划分", "train", "val", "test", "unspecified"]
                }

                Rectangle {
                    visible: appController.projectOpen && currentDatasetId !== ""
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.filterBarHeight
                    color: Theme.bgSide

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingLarge
                        anchors.rightMargin: Theme.spacingLarge
                        spacing: Theme.spacingNormal

                        Text {
                            text: currentDatasetName ? (currentDatasetName + " / " + galleryFilterLabel) : "样本画廊"
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: Font.DemiBold
                            font.family: Theme.fontFamily
                            color: Theme.textMain
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        StatusTag {
                            text: sampleListModel.count + " 张图像"
                            tone: sampleListModel.count > 0 ? "info" : "neutral"
                        }

                        StatusTag {
                            text: labeledSamples + "/" + totalSamples + " 已标注"
                            tone: labeledSamples > 0 ? "success" : "warning"
                        }

                        // 缩略图大小滑块（对标 DLTools 数据集页）
                        RowLayout {
                            spacing: Theme.spacingTiny
                            visible: currentDatasetId !== ""
                            Text {
                                text: "缩略图"
                                font.pixelSize: Theme.fontSizeCaption
                                font.family: Theme.fontFamily
                                color: Theme.textMuted
                            }
                            Slider {
                                id: thumbSizeSlider
                                from: 60
                                to: 180
                                stepSize: 10
                                value: 90
                                implicitWidth: 96
                            }
                        }
                    }
                }

                // 空状态提示：未打开项目
                EmptyState {
                    visible: !appController.projectOpen
                    icon: "folder"
                    title: "请先打开项目"
                    description: "在项目管理中创建或打开项目后即可管理数据集"
                    actionText: "前往项目管理"
                    onActionClicked: appController.currentPage = "project"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Theme.spacingXLarge
                }

                // 空状态提示：未选择数据集
                EmptyState {
                    visible: appController.projectOpen && currentDatasetId === ""
                    icon: "images"
                    title: "请选择数据集"
                    description: "从左侧图库列表中选择一个数据集开始浏览"
                    actionText: "导入数据"
                    onActionClicked: importDialogRoot.open()
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Theme.spacingXLarge
                }

                // 空状态提示：数据集无图片
                EmptyState {
                    visible: appController.projectOpen && currentDatasetId !== "" && rawSamples.length === 0
                    icon: "images"
                    title: "该数据集暂无图片"
                    description: "可重新导入数据集，或检查图片目录是否正确"
                    actionText: "导入数据"
                    onActionClicked: importDialogRoot.open()
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Theme.spacingXLarge
                }

                // 空状态提示：数据集无标签（有图但筛选后无标签样本）
                EmptyState {
                    visible: appController.projectOpen && currentDatasetId !== ""
                             && rawSamples.length > 0 && sampleListModel.count === 0
                    icon: "brain"
                    title: labeledSamples === 0 ? "该数据集暂无标签" : "无符合筛选条件的样本"
                    description: labeledSamples === 0
                                 ? "可前往标注页为图片添加标签"
                                 : "调整筛选条件后查看其他样本"
                    actionText: labeledSamples === 0 ? "前往标注" : ""
                    onActionClicked: appController.currentPage = "annotation"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Theme.spacingXLarge
                }

                // === gallery-grid: auto-fill minmax(90px, 1fr), gap:10px, padding:16px ===
                GridView {
                    id: thumbnailGrid
                    visible: appController.projectOpen && currentDatasetId !== ""
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: Theme.spacingLarge  // 16px padding
                    clip: true
                    // auto-fill minmax(90px, 1fr) → cellWidth=缩略图+gap, cellHeight=缩略图+gap+label
                    cellWidth: thumbSizeSlider.value + 10
                    cellHeight: thumbSizeSlider.value + 10 + 14
                    model: sampleListModel

                    ScrollBar.vertical: ScrollBar {
                        active: true
                        policy: ScrollBar.AsNeeded
                        contentItem: Rectangle {
                            implicitWidth: 6
                            radius: 3
                            color: Theme.borderColor
                        }
                    }

                    delegate: Item {
                        width: thumbnailGrid.cellWidth - 10  // 减去gap
                        height: thumbnailGrid.cellHeight - 10

                        // thumb-card: bgSide+border, aspect-ratio:1, 图片fill, 底部标签(9px)
                        Rectangle {
                            anchors.fill: parent
                            color: {
                                if (selectedSample && selectedSample.sampleId === model.sampleId) return Theme.bgSelected
                                if (thumbMouse.containsMouse) return Theme.bgHover
                                return Theme.bgSide
                            }
                            radius: Theme.radiusSmall
                            border.color: {
                                if (selectedSample && selectedSample.sampleId === model.sampleId) return Theme.primaryGlow
                                if (thumbMouse.containsMouse) return Theme.borderHover
                                return Theme.borderColor
                            }
                            border.width: selectedSample && selectedSample.sampleId === model.sampleId ? 2 : 1

                            // active thumb: box-shadow glow
                            Rectangle {
                                visible: selectedSample && selectedSample.sampleId === model.sampleId
                                anchors.fill: parent
                                anchors.margins: -6
                                radius: Theme.radiusSmall + 2
                                color: Theme.glowCyan
                                z: -1
                            }

                            MouseArea {
                                id: thumbMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: function(mouse) {
                                    selectedSample = {
                                        sampleId: model.sampleId,
                                        fileName: model.fileName,
                                        imagePath: model.imagePath,
                                        width: model.imgWidth,
                                        height: model.imgHeight,
                                        labelCount: model.labelCount,
                                        tagId: model.tagId || ""
                                    }
                                    // 选中态同步显示样本当前 Tag（空串=未打标）
                                    selectedTag = model.tagId ? (tagNameById[model.tagId] || "") : ""
                                    if (mouse.button === Qt.RightButton && selectedSample)
                                        sampleTagMenu.popup()
                                }
                                onDoubleClicked: {
                                    appController.currentPage = "annotation"
                                }
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 2
                                spacing: 0

                                // 图片区域 (aspect-ratio:1, fill)
                                Image {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.minimumHeight: 0
                                    // P1-21：优先缩略图，回退原图；sourceSize 限制解码像素
                                    source: pageRoot.resolveThumbSource(model.imagePath)
                                    sourceSize.width: 256
                                    sourceSize.height: 256
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: true
                                    clip: true
                                }

                                // 底部文件名标签 (9px)
                                Text {
                                    text: model.fileName
                                    font.pixelSize: 9
                                    font.family: Theme.fontFamily
                                    color: Theme.textMuted
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                    leftPadding: 2
                                    rightPadding: 2
                                }
                            }

                            // Tag 角标（右上角小胶囊，颜色按语义映射）
                            Rectangle {
                                visible: model.tagId !== undefined && model.tagId !== "" && (tagNameById[model.tagId] || "") !== ""
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.topMargin: 3
                                anchors.rightMargin: 3
                                radius: 6
                                height: 13
                                width: tagBadgeText.implicitWidth + 8
                                color: sampleTagColor(tagNameById[model.tagId] || "")
                                border.color: Theme.glassBorderGlow
                                border.width: 1

                                Text {
                                    id: tagBadgeText
                                    anchors.centerIn: parent
                                    text: model.tagId ? (tagNameById[model.tagId] || "") : ""
                                    font.pixelSize: 8
                                    font.family: Theme.fontFamily
                                    color: "#FFFFFF"
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ================================================================
    // 样本数据模型
    // ================================================================
    ListModel {
        id: sampleListModel
    }

    function applySampleFilters() {
        sampleListModel.clear()

        var statusFilter = galleryFilterBar.tagFilter || "全部状态"
        var splitFilter = galleryFilterBar.labelFilter || "全部划分"
        var sampleTagFilter = galleryFilterBar.sampleTagFilter || "全部Tag"
        var filteredCount = 0

        for (var index = 0; index < rawSamples.length; ++index) {
            var sample = rawSamples[index]
            var hasLabel = sample.labelPath !== ""
            var validationStatus = sample.validationStatus || ""
            var splitValue = sample.split || "unspecified"

            var matchStatus = true
            if (statusFilter === "已标注")
                matchStatus = hasLabel
            else if (statusFilter === "未标注")
                matchStatus = !hasLabel
            else if (statusFilter === "有效")
                matchStatus = validationStatus === "valid" || validationStatus === "good" || validationStatus === "defective"
            else if (statusFilter === "异常")
                matchStatus = validationStatus !== "" && validationStatus !== "valid" && validationStatus !== "good" && validationStatus !== "defective"

            var matchSplit = splitFilter === "全部划分" || splitValue === splitFilter

            // 类别过滤：仅保留包含目标类别的样本
            var matchClass = true
            if (filterClassIndex >= 0) {
                var cis = sample.classIndices || []
                matchClass = cis.indexOf(filterClassIndex) >= 0
            }

            // 图像 Tag 过滤：无Tag = 未打标样本，具体 Tag = 按名称匹配
            var sampleTagName = sample.tagId ? (tagNameById[sample.tagId] || "") : ""
            var matchSampleTag = true
            if (sampleTagFilter === "无Tag")
                matchSampleTag = sampleTagIdOf(sample) === ""
            else if (sampleTagFilter !== "全部Tag")
                matchSampleTag = sampleTagName === sampleTagFilter

            if (!matchStatus || !matchSplit || !matchClass || !matchSampleTag)
                continue

            sampleListModel.append(sample)
            filteredCount += 1
        }

        galleryFilterLabel = statusFilter === "全部状态" && splitFilter === "全部划分" && filterClassIndex < 0 && sampleTagFilter === "全部Tag"
            ? "全部图像"
            : (statusFilter + " / " + sampleTagFilter + " / " + splitFilter + (filterClassIndex >= 0 ? " / 类别" + filterClassIndex : ""))
    }

    function sampleTagIdOf(sample) {
        return sample && sample.tagId ? String(sample.tagId) : ""
    }

    // Tag 名称 → 语义色（内置 Tag 映射系统色，自定义 Tag 中性灰）
    function sampleTagColor(name) {
        if (name === "良品") return Theme.success
        if (name === "漏检") return Theme.warning
        if (name === "误检") return Theme.danger
        if (name === "待定") return Theme.primary
        if (name === "重要") return Theme.tagImportant
        return Theme.textMuted
    }

    // 查询 Tag 的快捷键（供 chip 显示提示）
    function tagShortcutFor(name) {
        if (typeof tagModel === "undefined") return ""
        for (var i = 0; i < tagModel.rowCount(); i++) {
            var idx = tagModel.index(i, 0)
            if (tagModel.data(idx, Qt.UserRole + 2) === name)
                return tagModel.data(idx, Qt.UserRole + 3) || ""
        }
        return ""
    }

    function tagChipLabel(name) {
        var sc = tagShortcutFor(name)
        return sc ? (name + "  " + sc) : name
    }

    // 右键打标菜单：标记为各 Tag / 清除
    ContextMenu {
        id: sampleTagMenu
        Repeater {
            model: typeof tagsSection !== "undefined" ? tagsSection.displayTags : []
            MenuItem {
                required property var modelData
                text: "标记为 " + modelData
                onTriggered: assignSampleTag(selectedSample ? selectedSample.sampleId : "", modelData)
            }
        }
        MenuSeparator {}
        MenuItem {
            text: "清除 Tag"
            onTriggered: assignSampleTag(selectedSample ? selectedSample.sampleId : "", "")
        }
    }

    // === 全局筛选接口：数据集过滤 ===
    function setDatasetFilter(dsId) {
        if (dsId) {
            selectDataset(dsId)
        } else {
            // 清空选择，回到"请选择数据集"状态
            currentDatasetId = ""
            currentDatasetName = ""
            selectedSample = null
            sampleListModel.clear()
            rawSamples = []
            tagModel.setDatasetId("")
        }
    }

    // === 全局筛选接口：标签类别过滤（classIndex < 0 表示不过滤） ===
    function setClassFilter(clsIndex) {
        filterClassIndex = (clsIndex !== undefined && clsIndex !== null && Number(clsIndex) >= 0)
            ? Number(clsIndex) : -1
        applySampleFilters()
    }

    // ================================================================
    // 选择数据集：加载样本列表与统计信息
    // ================================================================
    function selectDataset(dsId) {
        currentDatasetId = dsId
        selectedSample = null
        sampleListModel.clear()
        rawSamples = []
        // P1-21：解析缩略图目录
        pageRoot.thumbCacheDir = projectService.thumbnailCacheDir(appController.currentProjectId)

        // 从listDatasets获取数据集名称
        var datasets = datasetService.listDatasets(appController.currentProjectId)
        for (var i = 0; i < datasets.length; i++) {
            if (datasets[i].id === dsId) {
                currentDatasetName = datasets[i].name
                break
            }
        }

        // 获取样本统计
        var stats = datasetService.getSampleStats(dsId)
        totalSamples = stats.totalSamples || 0
        labeledSamples = stats.labeledSamples || 0

        // 加载样本列表到ListModel
        var samples = datasetService.listSamples(dsId, 0, 500)
        var rawPaths = []
        for (var j = 0; j < samples.length; j++) {
            var s = samples[j]
            var imgPath = s.image_path || s.imagePath || ""
            var fileName = imgPath.split("/").pop().split("\\").pop()
            // 收集样本类别索引，供全局类别过滤使用
            var classIndices = []
            if (s.labelPath) {
                var annotations = annotationService.loadAnnotations(s.labelPath)
                for (var k = 0; k < annotations.length; k++) {
                    var cid = annotations[k].classIndex
                    if (classIndices.indexOf(cid) < 0)
                        classIndices.push(cid)
                }
            }
            rawSamples.push({
                "sampleId": s.id || "",
                "fileName": fileName,
                "imagePath": "file:///" + imgPath.replace(/\\/g, "/"),
                "imgWidth": s.width || 0,
                "imgHeight": s.height || 0,
                "labelCount": s.labelPath ? 1 : 0,
                "labelPath": s.labelPath || "",
                "validationStatus": s.validationStatus || "",
                "split": s.split || "unspecified",
                "classIndices": classIndices,
                "tagId": s.tagId || ""
            })
            if (imgPath) rawPaths.push(imgPath.replace(/\\/g, "/"))
        }

        // P1-21：后台补齐本页缩略图
        if (pageRoot.thumbCacheDir && rawPaths.length > 0) {
            thumbnailGenerator.generate(rawPaths, pageRoot.thumbCacheDir)
        }

        applySampleFilters()
    }

    Connections {
        target: galleryFilterBar
        function onTagFilterChanged() {
            pageRoot.applySampleFilters()
        }
        function onLabelFilterChanged() {
            pageRoot.applySampleFilters()
        }
        function onSampleTagFilterChanged() {
            pageRoot.applySampleFilters()
        }
    }

    // 图像 Tag 体系：Tag 增删改时重建映射与筛选选项；样本指派变化时刷新本页行
    Connections {
        target: tagService
        function onTagsChanged(datasetId) {
            if (datasetId === currentDatasetId) pageRoot.rebuildTagMaps()
        }
        function onSampleTagsChanged(datasetId) {
            if (datasetId === currentDatasetId) refreshSampleTagIds()
        }
    }

    function refreshSampleTagIds() {
        for (var i = 0; i < rawSamples.length; ++i) {
            rawSamples[i].tagId = tagService.getSampleTagId(rawSamples[i].sampleId) || ""
        }
        if (selectedSample && selectedSample.sampleId) {
            selectedSample.tagId = tagService.getSampleTagId(selectedSample.sampleId) || ""
            selectedTag = selectedSample.tagId ? (tagNameById[selectedSample.tagId] || "") : ""
        }
        applySampleFilters()
    }

    // ================================================================
    // 添加标签弹窗
    // ================================================================
    Dialog {
        id: addTagDialog
        title: "添加标签"
        modal: true
        // 挂到 Overlay 才能稳定居中显示，否则点 + 看起来“没反应”
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 440
        standardButtons: Dialog.NoButton

        background: Rectangle {
            color: Theme.bgCard
            border.color: Theme.borderColor
            border.width: 1
            radius: Theme.radiusLarge
        }

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingLarge

            Text {
                text: "添加新的图像标签"
                font.pixelSize: Theme.fontSizeSubheading
                font.bold: true
                font.family: Theme.fontFamily
                color: Theme.textMain
            }

            // 标签名称输入
            FormField {
                label: "标签名称"
                required: true
                Layout.fillWidth: true

                TextField {
                    id: tagNameField
                    anchors.fill: parent
                    placeholderText: "输入标签名称"
                    placeholderTextColor: Theme.textDisabled
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeNormal
                    font.family: Theme.fontFamily

                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: tagNameField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                        border.width: 1
                    }
                }
            }

            // 快捷键输入
            FormField {
                label: "快捷键"
                Layout.fillWidth: true

                TextField {
                    id: tagShortcutField
                    anchors.fill: parent
                    placeholderText: "按下一个键作为快捷键"
                    placeholderTextColor: Theme.textDisabled
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeNormal
                    font.family: Theme.fontFamily

                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: tagShortcutField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                        border.width: 1
                    }

                    Keys.onPressed: function(event) {
                        event.accepted = true
                        tagShortcutField.text = event.text.toUpperCase()
                    }
                }
            }

            // 按钮行
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingNormal

                Item { Layout.fillWidth: true }

                Button {
                    Layout.preferredHeight: 36
                    Layout.preferredWidth: 80
                    text: "取消"
                    font.pixelSize: Theme.fontSizeNormal
                    font.family: Theme.fontFamily

                    background: Rectangle {
                        color: parent.hovered ? Theme.bgHover : Theme.bgCard
                        border.color: Theme.borderColor
                        border.width: 1
                        radius: Theme.radiusSmall
                    }

                    contentItem: Text {
                        text: parent.text
                        color: Theme.textMain
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: addTagDialog.reject()
                }

                Button {
                    Layout.preferredHeight: 36
                    Layout.preferredWidth: 100
                    text: "添加"
                    font.pixelSize: Theme.fontSizeNormal
                    font.bold: true
                    font.family: Theme.fontFamily

                    background: Rectangle {
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Theme.primary }
                            GradientStop { position: 1.0; color: Theme.primaryDark }
                        }
                        radius: Theme.radiusSmall
                    }

                    contentItem: Text {
                        text: parent.text
                        color: Theme.textMain
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: addTagDialog.accept()
                }
            }
        }

        onAccepted: {
            // 持久化标签并刷新列表，给出明确成功/失败反馈
            var name = tagNameField.text.trim()
            var shortcut = tagShortcutField.text.trim()
            if (!name) {
                if (typeof ToastBus !== "undefined") ToastBus.error("请输入标签名称")
                return
            }
            if (!currentDatasetId) {
                if (typeof ToastBus !== "undefined") ToastBus.error("请先选择数据集")
                return
            }
            var tagId = tagService.addTag(currentDatasetId, name, shortcut)
            if (tagId) {
                tagModel.setDatasetId(currentDatasetId)
                tagModel.refresh()
                pageRoot.rebuildTagMaps()
                if (typeof ToastBus !== "undefined") ToastBus.success("已添加标签：" + name)
            } else {
                if (typeof ToastBus !== "undefined") ToastBus.error("添加失败：标签可能已存在")
            }
            tagNameField.clear()
            tagShortcutField.clear()
        }
        onRejected: { tagNameField.clear(); tagShortcutField.clear() }
    }

    // ================================================================
    // 导入数据集弹窗（复用原有导入逻辑）
    // ================================================================
    Dialog {
        id: importDialogRoot
        title: "导入数据集"
        modal: true
        anchors.centerIn: parent
        width: 560
        standardButtons: Dialog.NoButton

        property var scanResult: null
        property bool isScanning: false
        property string selectedImagePath: ""
        property string selectedLabelPath: ""
        property string importMode: "auto"

        background: Rectangle {
            color: Theme.bgCard
            border.color: Theme.borderColor
            border.width: 1
            radius: Theme.radiusLarge
        }

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingLarge

            Text {
                text: "导入新的数据集"
                font.pixelSize: Theme.fontSizeSubheading
                font.bold: true
                font.family: Theme.fontFamily
                color: Theme.textMain
            }

            // 导入模式切换
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingNormal

                Button {
                    id: autoModeBtn
                    text: "单目录自动探测"
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    font.bold: importDialogRoot.importMode === "auto"

                    background: Rectangle {
                        color: importDialogRoot.importMode === "auto" ? Theme.primary : (autoModeBtn.hovered ? Theme.bgHover : Theme.bgCard)
                        radius: Theme.radiusSmall
                        border.color: importDialogRoot.importMode === "auto" ? Theme.primary : Theme.borderColor
                        border.width: 1
                    }

                    contentItem: Text {
                        text: autoModeBtn.text
                        color: importDialogRoot.importMode === "auto" ? Theme.textMain : Theme.textMuted
                        font: autoModeBtn.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        importDialogRoot.importMode = "auto"
                        importDialogRoot.scanResult = null
                    }
                }

                Button {
                    id: sepModeBtn
                    text: "分别指定路径"
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    font.bold: importDialogRoot.importMode === "separate"

                    background: Rectangle {
                        color: importDialogRoot.importMode === "separate" ? Theme.primary : (sepModeBtn.hovered ? Theme.bgHover : Theme.bgCard)
                        radius: Theme.radiusSmall
                        border.color: importDialogRoot.importMode === "separate" ? Theme.primary : Theme.borderColor
                        border.width: 1
                    }

                    contentItem: Text {
                        text: sepModeBtn.text
                        color: importDialogRoot.importMode === "separate" ? Theme.textMain : Theme.textMuted
                        font: sepModeBtn.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        importDialogRoot.importMode = "separate"
                        importDialogRoot.scanResult = null
                    }
                }
            }

            // 自动探测模式
            ColumnLayout {
                visible: importDialogRoot.importMode === "auto"
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                Text {
                    text: "选择数据集根目录"
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingNormal

                    TextField {
                        id: importPathField
                        Layout.fillWidth: true
                        placeholderText: "选择数据集根目录..."
                        placeholderTextColor: Theme.textDisabled
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: importPathField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                            border.width: 1
                        }

                        onTextChanged: importDialogRoot.selectedImagePath = text
                    }

                    Button {
                        Layout.preferredHeight: 36
                        text: "浏览"
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily

                        background: Rectangle {
                            color: parent.hovered ? Theme.bgHover : Theme.bgCard
                            border.color: Theme.borderColor
                            border.width: 1
                            radius: Theme.radiusSmall
                        }

                        contentItem: Text {
                            text: parent.text
                            color: Theme.textMain
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: importFolderDialog.open()
                    }

                    Button {
                        Layout.preferredHeight: 36
                        text: "分析"
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                        font.family: Theme.fontFamily
                        enabled: importPathField.text.trim().length > 0 && !importDialogRoot.isScanning

                        background: Rectangle {
                            color: parent.enabled ? (parent.hovered ? Qt.lighter(Theme.primary, 1.1) : Theme.primary) : Theme.bgCard
                            radius: Theme.radiusSmall
                            border.color: parent.enabled ? Theme.primary : Theme.borderColor
                            border.width: 1
                        }

                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? Theme.textMain : Theme.textDisabled
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            importDialogRoot.isScanning = true
                            importDialogRoot.scanResult = null
                            importDialogRoot.selectedImagePath = importPathField.text.trim()
                            datasetService.scanFolderAsync(importDialogRoot.selectedImagePath)
                        }
                    }
                }
            }

            // 分别指定路径模式
            ColumnLayout {
                visible: importDialogRoot.importMode === "separate"
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                Text {
                    text: "分别指定图片和标签路径"
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingNormal

                    Text {
                        text: "图片:"
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily
                        color: Theme.textMuted
                        Layout.preferredWidth: 40
                    }

                    TextField {
                        id: sepImageField
                        Layout.fillWidth: true
                        placeholderText: "图片目录..."
                        placeholderTextColor: Theme.textDisabled
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: sepImageField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                            border.width: 1
                        }

                        onTextChanged: importDialogRoot.selectedImagePath = text
                    }

                    Button {
                        Layout.preferredHeight: 36
                        text: "浏览"
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily

                        background: Rectangle {
                            color: parent.hovered ? Theme.bgHover : Theme.bgCard
                            border.color: Theme.borderColor
                            border.width: 1
                            radius: Theme.radiusSmall
                        }

                        contentItem: Text {
                            text: parent.text
                            color: Theme.textMain
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: sepImageFolderDialog.open()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingNormal

                    Text {
                        text: "标签:"
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily
                        color: Theme.textMuted
                        Layout.preferredWidth: 40
                    }

                    TextField {
                        id: sepLabelField
                        Layout.fillWidth: true
                        placeholderText: "标签目录（可留空）..."
                        placeholderTextColor: Theme.textDisabled
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: sepLabelField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                            border.width: 1
                        }

                        onTextChanged: importDialogRoot.selectedLabelPath = text
                    }

                    Button {
                        Layout.preferredHeight: 36
                        text: "浏览"
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily

                        background: Rectangle {
                            color: parent.hovered ? Theme.bgHover : Theme.bgCard
                            border.color: Theme.borderColor
                            border.width: 1
                            radius: Theme.radiusSmall
                        }

                        contentItem: Text {
                            text: parent.text
                            color: Theme.textMain
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: sepLabelFolderDialog.open()
                    }
                }

                Button {
                    Layout.alignment: Qt.AlignRight
                    Layout.preferredHeight: 36
                    text: "分析匹配"
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: true
                    font.family: Theme.fontFamily
                    enabled: sepImageField.text.trim().length > 0 && !importDialogRoot.isScanning

                    background: Rectangle {
                        color: parent.enabled ? (parent.hovered ? Qt.lighter(Theme.primary, 1.1) : Theme.primary) : Theme.bgCard
                        radius: Theme.radiusSmall
                        border.color: parent.enabled ? Theme.primary : Theme.borderColor
                        border.width: 1
                    }

                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? Theme.textMain : Theme.textDisabled
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        importDialogRoot.isScanning = true
                        importDialogRoot.scanResult = null
                        importDialogRoot.selectedImagePath = sepImageField.text.trim()
                        importDialogRoot.selectedLabelPath = sepLabelField.text.trim()
                        datasetService.scanSeparateAsync(importDialogRoot.selectedImagePath, importDialogRoot.selectedLabelPath)
                    }
                }
            }

            // 扫描中状态
            BusyIndicator {
                visible: importDialogRoot.isScanning
                running: importDialogRoot.isScanning
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: 36
                Layout.preferredWidth: 36
            }

            Text {
                visible: importDialogRoot.isScanning
                text: "正在分析目录..."
                font.pixelSize: Theme.fontSizeSmall
                font.family: Theme.fontFamily
                color: Theme.textMuted
                Layout.alignment: Qt.AlignHCenter
            }

            // 扫描结果展示
            ColumnLayout {
                visible: importDialogRoot.scanResult !== null && !importDialogRoot.isScanning
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingLarge

                    // 格式标签
                    Rectangle {
                        visible: importDialogRoot.scanResult && importDialogRoot.scanResult.detectedFormat
                        Layout.preferredWidth: formatText.implicitWidth + 16
                        Layout.preferredHeight: 24
                        radius: Theme.radiusSmall
                        color: {
                            var fmt = importDialogRoot.scanResult ? importDialogRoot.scanResult.detectedFormat : ""
                            if (fmt === "yolo_txt") return Theme.primary
                            if (fmt === "coco_json") return Theme.primaryGlow
                            if (fmt === "labelme_json") return Theme.warning
                            if (fmt === "anomaly_unsupervised") return Theme.warning
                            return Theme.textMuted
                        }

                        Text {
                            id: formatText
                            anchors.centerIn: parent
                            text: {
                                var fmt = importDialogRoot.scanResult ? importDialogRoot.scanResult.detectedFormat : ""
                                if (fmt === "yolo_txt") return "YOLO TXT"
                                if (fmt === "coco_json") return "COCO JSON"
                                if (fmt === "labelme_json") return "LabelMe"
                                if (fmt === "anomaly_unsupervised") return "异常检测"
                                if (fmt === "image_only") return "纯图片"
                                return fmt || "未知"
                            }
                            font.pixelSize: Theme.fontSizeCaption
                            font.bold: true
                            font.family: Theme.fontFamily
                            color: Theme.textMain
                        }
                    }

                    Text {
                        text: "图片: " + (importDialogRoot.scanResult ? (importDialogRoot.scanResult.imageCount || 0) : 0)
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily
                        color: Theme.textMain
                    }

                    Text {
                        visible: importDialogRoot.scanResult && importDialogRoot.scanResult.labelCount !== undefined
                        text: "已标注: " + (importDialogRoot.scanResult ? (importDialogRoot.scanResult.labelCount || 0) : 0)
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily
                        color: Theme.success
                    }
                }

                // 数据集名称输入
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingNormal

                    Text {
                        text: "数据集名称"
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily
                        color: Theme.textMuted
                    }

                    TextField {
                        id: importNameField
                        Layout.fillWidth: true
                        placeholderText: "输入数据集名称"
                        placeholderTextColor: Theme.textDisabled
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily
                        text: importDialogRoot.scanResult ? extractFolderName(importDialogRoot.selectedImagePath) : ""

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: importNameField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                            border.width: 1
                        }
                    }
                }
            }

            // 底部按钮行
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingNormal

                Item { Layout.fillWidth: true }

                Button {
                    Layout.preferredHeight: 36
                    Layout.preferredWidth: 80
                    text: "取消"
                    font.pixelSize: Theme.fontSizeNormal
                    font.family: Theme.fontFamily

                    background: Rectangle {
                        color: parent.hovered ? Theme.bgHover : Theme.bgCard
                        border.color: Theme.borderColor
                        border.width: 1
                        radius: Theme.radiusSmall
                    }

                    contentItem: Text {
                        text: parent.text
                        color: Theme.textMain
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: importDialogRoot.reject()
                }

                Button {
                    Layout.preferredHeight: 36
                    Layout.preferredWidth: 100
                    text: "导入"
                    font.pixelSize: Theme.fontSizeNormal
                    font.bold: true
                    font.family: Theme.fontFamily
                    enabled: importDialogRoot.scanResult
                             && importDialogRoot.scanResult.isValid
                             && importNameField.text.trim().length > 0
                             && !importDialogRoot.isScanning

                    background: Rectangle {
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Theme.primary }
                            GradientStop { position: 1.0; color: Theme.primaryDark }
                        }
                        radius: Theme.radiusSmall
                    }

                    contentItem: Text {
                        text: parent.text
                        color: Theme.textMain
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        var dsId = ""
                        var format = importDialogRoot.scanResult ? importDialogRoot.scanResult.detectedFormat : ""
                        var name = importNameField.text.trim()

                        if (importDialogRoot.importMode === "separate") {
                            dsId = datasetService.importDatasetSeparate(
                                appController.currentProjectId,
                                name,
                                importDialogRoot.selectedImagePath,
                                importDialogRoot.selectedLabelPath
                            )
                        } else {
                            dsId = datasetService.importDatasetV2(
                                appController.currentProjectId,
                                name,
                                importDialogRoot.selectedImagePath,
                                format,
                                importDialogRoot.scanResult.labelDirOrPath ? importDialogRoot.scanResult.labelDirOrPath : "",
                                true
                            )
                        }

                        if (dsId && dsId.length > 0) {
                            datasetModel.refresh()
                            selectDataset(dsId)
                            importDialogRoot.scanResult = null
                            importPathField.clear()
                            sepImageField.clear()
                            sepLabelField.clear()
                            importNameField.clear()
                            importDialogRoot.close()
                        }
                    }
                }
            }
        }

        onRejected: {
            scanResult = null
            isScanning = false
            importPathField.clear()
            sepImageField.clear()
            sepLabelField.clear()
            importNameField.clear()
        }
    }

    // ================================================================
    // 文件夹选择对话框
    // ================================================================
    FolderDialog {
        id: importFolderDialog
        title: "选择数据集根目录"
        onAccepted: {
            importPathField.text = urlToPath(selectedFolder)
            importDialogRoot.selectedImagePath = importPathField.text
        }
    }

    FolderDialog {
        id: sepImageFolderDialog
        title: "选择图片目录"
        onAccepted: {
            sepImageField.text = urlToPath(selectedFolder)
            importDialogRoot.selectedImagePath = sepImageField.text
        }
    }

    FolderDialog {
        id: sepLabelFolderDialog
        title: "选择标签目录"
        onAccepted: {
            sepLabelField.text = urlToPath(selectedFolder)
            importDialogRoot.selectedLabelPath = sepLabelField.text
        }
    }

    // ================================================================
    // 工具函数
    // ================================================================

    // URL转本地路径（Windows兼容）
    function urlToPath(url) {
        var s = url.toString()
        if (s.startsWith("file:///")) {
            s = s.substring(7)
            if (s.length >= 3 && s.charAt(0) === "/" && s.charAt(2) === ":") {
                var driveLetter = s.charAt(1).toUpperCase()
                if (driveLetter >= 'A' && driveLetter <= 'Z') {
                    s = s.substring(1)
                }
            }
        } else if (s.startsWith("file://")) {
            s = s.substring(6)
        }
        return decodeURIComponent(s)
    }

    // 从路径提取文件夹名
    function extractFolderName(path) {
        if (!path) return ""
        var normalized = path.replace(/\\/g, "/")
        var parts = normalized.split("/")
        var name = parts[parts.length - 1]
        if (!name && parts.length > 1) name = parts[parts.length - 2]
        return name || "dataset"
    }
}
