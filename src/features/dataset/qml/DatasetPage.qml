// DatasetPage.qml - V6 数据集页（像素级复刻参考UI设计）
// 左侧sidebar(240px) + resizer-v(4px) + 中心缩略图网格
// 布局：图库选择器 → 数据集卡片列表 → 图像属性 → Tag标签
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
            currentDatasetId = ""
            currentDatasetName = ""
            selectedSample = null
        }
    }

    // === 监听扫描完成信号 ===
    Connections {
        target: datasetService
        function onScanFolderFinished(result) { importDialogRoot.isScanning = false; importDialogRoot.scanResult = result }
        function onScanSeparateFinished(result) { importDialogRoot.isScanning = false; importDialogRoot.scanResult = result }
    }

    // ================================================================
    // 主布局：sidebar + resizer + center
    // ================================================================
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ============================================================
        // 左侧边栏 (240px, padding:12px, gap:16px)
        // ============================================================
        Rectangle {
            id: sidebarRect
            Layout.preferredWidth: Theme.sidebarWidth
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

            ScrollView {
                anchors.fill: parent
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: Theme.sidebarWidth - 1
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

                    // === Tag 可折叠区块 ===
                    CollapsibleSection {
                        Layout.fillWidth: true
                        title: "Tag"
                        expanded: true

                        ColumnLayout {
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

                            // 2列grid按钮: 默认(primary填充), 良品/漏检/误检/待定/重要(bgCard+border)
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                rowSpacing: Theme.spacingTiny
                                columnSpacing: Theme.spacingTiny

                                Repeater {
                                    model: ["默认", "良品", "漏检", "误检", "待定", "重要"]

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 26
                                        radius: Theme.radiusSmall
                                        // 默认tag用primary填充，其他用bgCard+border
                                        color: selectedTag === modelData
                                               ? Theme.primary
                                               : (modelData === "默认" ? Theme.bgCard : Theme.bgCard)
                                        border.color: selectedTag === modelData
                                                      ? Theme.primary
                                                      : Theme.borderColor
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData
                                            font.pixelSize: Theme.fontSizeCaption
                                            font.family: Theme.fontFamily
                                            color: selectedTag === modelData ? Theme.textMain : Theme.textMuted
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                selectedTag = selectedTag === modelData ? "" : modelData
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
        // resizer-v (4px宽，hover变primaryGlow色，可拖拽)
        // ============================================================
        Splitter {
            id: sidebarResizer
            Layout.preferredWidth: 4
            Layout.fillHeight: true
            vertical: true
            targetItem: sidebarRect
            minSize: Theme.sidebarMinWidth  // 120
            maxSize: 600
        }

        // ============================================================
        // 中心缩略图画廊网格
        // ============================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgMain

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 空状态提示：未打开项目
                Text {
                    visible: !appController.projectOpen
                    text: "请先打开项目"
                    font.pixelSize: Theme.fontSizeSubheading
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Theme.spacingXLarge
                }

                // 空状态提示：未选择数据集
                Text {
                    visible: appController.projectOpen && currentDatasetId === ""
                    text: "请从左侧选择一个数据集"
                    font.pixelSize: Theme.fontSizeSubheading
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Theme.spacingXLarge
                }

                // 空状态提示：数据集无图片
                Text {
                    visible: appController.projectOpen && currentDatasetId !== "" && sampleListModel.count === 0
                    text: "该数据集暂无图片"
                    font.pixelSize: Theme.fontSizeSubheading
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
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
                    // auto-fill minmax(90px, 1fr) → cellWidth=90+gap, cellHeight=90+gap+label
                    cellWidth: 90 + 10  // 90px thumb + 10px gap
                    cellHeight: 90 + 10 + 14  // 90px thumb + 10px gap + 14px label
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
                                onClicked: {
                                    selectedSample = {
                                        sampleId: model.sampleId,
                                        fileName: model.fileName,
                                        imagePath: model.imagePath,
                                        width: model.imgWidth,
                                        height: model.imgHeight,
                                        labelCount: model.labelCount
                                    }
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
                                    source: model.imagePath
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

    // ================================================================
    // 选择数据集：加载样本列表与统计信息
    // ================================================================
    function selectDataset(dsId) {
        currentDatasetId = dsId
        selectedSample = null
        sampleListModel.clear()

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
        for (var j = 0; j < samples.length; j++) {
            var s = samples[j]
            var imgPath = s.image_path || s.imagePath || ""
            var fileName = imgPath.split("/").pop().split("\\").pop()
            sampleListModel.append({
                "sampleId": s.id || "",
                "fileName": fileName,
                "imagePath": "file:///" + imgPath.replace(/\\/g, "/"),
                "imgWidth": s.width || 0,
                "imgHeight": s.height || 0,
                "labelCount": s.label_count || 0
            })
        }
    }

    // ================================================================
    // 添加Tag弹窗
    // ================================================================
    Dialog {
        id: addTagDialog
        title: "添加Tag"
        modal: true
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
                text: "添加新的图像Tag标签"
                font.pixelSize: Theme.fontSizeSubheading
                font.bold: true
                font.family: Theme.fontFamily
                color: Theme.textMain
            }

            // Tag名称输入
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                Text {
                    text: "Tag名称"
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }

                TextField {
                    id: tagNameField
                    Layout.fillWidth: true
                    placeholderText: "输入Tag名称"
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
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                Text {
                    text: "快捷键"
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }

                TextField {
                    id: tagShortcutField
                    Layout.fillWidth: true
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
                    text: "确认添加"
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

        onAccepted: { tagNameField.clear(); tagShortcutField.clear() }
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
                    text: "确认导入"
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
