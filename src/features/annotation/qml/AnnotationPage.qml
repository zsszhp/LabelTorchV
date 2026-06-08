// AnnotationPage.qml - 标注工作台（v2.0.0 参考UI像素级重写版）
// 布局：左侧栏(240px, 4个可折叠区) + 中央画布(浮动工具栏+画布+状态栏)
// 键盘快捷键由 AnnotCanvasItem C++ 层处理，QML 不再注册 Shortcut
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme
import LabelTorch.Components
import LabelTorch.Shell
import LabelTorch.Annotation 1.0

Item {
    id: root

    // === 属性 ===
    property int shapeMode: 0          // 0=HBB, 1=OBB, 2=Polygon
    property string annotationMode: "detect"  // detect / classify / anomaly
    property int selectedClassId: -1
    property var selectedMultiClassIds: []
    property bool isAnomalous: false
    property var sampleListData: []
    property var tagListData: []       // [{name, color}]
    property int currentSampleIndex: -1
    property int editLabelTargetIndex: -1  // 双击编辑标签的目标标注索引

    // === 生命周期 ===
    Component.onCompleted: {
        if (appController.projectOpen) {
            initAnnotationMode()
            refreshSampleList()
        }
    }

    onVisibleChanged: {
        if (visible && appController.projectOpen) {
            initAnnotationMode()
            refreshSampleList()
        }
    }

    // === 初始化标注模式 ===
    function initAnnotationMode() {
        var taskType = projectService.getTaskType(appController.currentProjectId)
        if (taskType === "detect") {
            root.annotationMode = "detect"
            root.shapeMode = 0
            annotationService.setShapeType(0)
            canvasItem.shapeMode = 0
        } else if (taskType === "obb") {
            root.annotationMode = "detect"
            root.shapeMode = 1
            annotationService.setShapeType(1)
            canvasItem.shapeMode = 1
        } else if (taskType === "classify") {
            root.annotationMode = "classify"
        } else if (taskType === "anomaly") {
            root.annotationMode = "anomaly"
        }
    }

    // === 刷新样本列表 ===
    function refreshSampleList() {
        if (!appController.projectOpen) {
            sampleListData = []
            return
        }
        var datasets = datasetService.listDatasets(appController.currentProjectId)
        var allSamples = []
        for (var d = 0; d < datasets.length; d++) {
            var ds = datasets[d]
            var samples = annotationService.listSamples(ds.id)
            for (var s = 0; s < samples.length; s++) {
                var sample = samples[s]
                sample.datasetName = ds.name
                allSamples.push(sample)
            }
        }
        sampleListData = allSamples
    }

    // === 加载样本 ===
    function loadSample(sampleData) {
        if (annotationMode === "classify") {
            var clsLabels = annotationService.loadClassificationLabels(sampleData.labelPath || "")
            if (clsLabels.labelType === "multi") {
                selectedMultiClassIds = clsLabels.classIds || []
            } else {
                selectedClassId = clsLabels.classId !== undefined ? clsLabels.classId : -1
                selectedMultiClassIds = []
            }
        } else if (annotationMode === "anomaly") {
            var anomalyLabels = annotationService.loadAnomalyLabels(sampleData.labelPath || "")
            isAnomalous = anomalyLabels.isAnomalous || false
        } else {
            annotationModel.loadFromLabel(sampleData.labelPath || "")
        }
        canvasItem.loadImage(sampleData.imagePath || "", sampleData.labelPath || "")
        // 更新当前样本索引
        for (var i = 0; i < sampleListData.length; i++) {
            if (sampleListData[i].imagePath === sampleData.imagePath) {
                currentSampleIndex = i
                break
            }
        }
    }

    // === 获取类别名 ===
    function getClassName(classIndex) {
        for (var i = 0; i < taxonomyModel.rowCount(); i++) {
            var idx = taxonomyModel.index(i, 0)
            if (taxonomyModel.data(idx, 0) === classIndex) {
                return taxonomyModel.data(idx, 1) || ("class_" + classIndex)
            }
        }
        return "class_" + classIndex
    }

    // === 保存当前标注 ===
    function saveCurrentAnnotations() {
        if (annotationMode === "classify") {
            if (classificationMultiCheck.checked ? selectedMultiClassIds.length > 0 : selectedClassId >= 0) {
                var labels = {}
                if (classificationMultiCheck.checked) {
                    labels.labelType = "multi"
                    labels.classIds = selectedMultiClassIds
                } else {
                    labels.labelType = "single"
                    labels.classId = selectedClassId
                }
                annotationService.saveClassificationLabels(
                    canvasController.currentLabelPath, "", "", labels
                )
            }
        } else if (annotationMode === "anomaly") {
            annotationService.saveAnomalyLabels(
                canvasController.currentLabelPath, "", "", isAnomalous
            )
        } else {
            if (canvasController.dirty) {
                canvasItem.commitUndoState()
                annotationService.saveAnnotations(
                    canvasController.currentLabelPath, "", "",
                    annotationModel.toVariantList()
                )
                canvasController.clearDirty()
            }
        }
    }

    // === 导航前后样本 ===
    function navigateToPrevious() {
        if (currentSampleIndex > 0) {
            loadSample(sampleListData[currentSampleIndex - 1])
        }
    }
    function navigateToNext() {
        if (currentSampleIndex < sampleListData.length - 1) {
            loadSample(sampleListData[currentSampleIndex + 1])
        }
    }

    // === 切换绘制模式 ===
    function setDrawTool(tool) {
        // tool: "select" / "rect" / "rotatedRect" / "polygon"
        if (tool === "select") {
            canvasController.drawMode = "select"
            canvasController.setPolygonDrawing(false)
        } else {
            canvasController.drawMode = "draw"
            canvasController.setPolygonDrawing(tool === "polygon")
            if (tool === "rect") {
                shapeMode = 0
                canvasItem.shapeMode = 0
                annotationService.setShapeType(0)
            } else if (tool === "rotatedRect") {
                shapeMode = 1
                canvasItem.shapeMode = 1
                annotationService.setShapeType(1)
            } else if (tool === "polygon") {
                shapeMode = 2
                canvasItem.shapeMode = 2
                annotationService.setShapeType(2)
            }
        }
    }

    // ================================================================
    // 主布局：左侧栏(240px) + 中央画布区 + 右侧面板(240px)
    // 对标参考设计：左面板4区域(hr分隔) + 可拖拽分割线 + 画布
    // ================================================================
    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        handle: Rectangle {
            implicitWidth: 4
            color: SplitHandle.pressed ? Theme.primaryGlow : (SplitHandle.hovered ? Theme.primaryGlow : Theme.borderColor)
            Behavior on color { ColorAnimation { duration: 150 } }
        }

        // === 左侧栏 (240px, padding:12px, gap:16px) ===
        Rectangle {
            id: sidebar
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

                ColumnLayout {
                    width: parent.width
                    spacing: 0

                    // ====== 区域1：图库与数据集 ======
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.margins: 12
                        spacing: 0

                        // 项目名（对标参考UI font-size:15px font-weight:600）
                        Text {
                            text: appController.projectOpen ? appController.currentProjectName : "未打开项目"
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            font.family: Theme.fontFamily
                            color: Theme.textMain
                            Layout.fillWidth: true
                            Layout.bottomMargin: 12
                            Layout.topMargin: 12
                        }

                        // 图库选择器（对标参考UI select + icon）
                        Text {
                            text: "图库"
                            font.pixelSize: 11
                            font.family: Theme.fontFamily
                            color: Theme.textMuted
                            Layout.bottomMargin: 5
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            Layout.bottomMargin: 14
                            color: Theme.bgCard
                            border.color: Theme.borderColor
                            border.width: 1
                            radius: 4

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 8

                                ComboBox {
                                    id: datasetCombo
                                    Layout.fillWidth: true
                                    model: datasetService.listDatasets(appController.currentProjectId || "")
                                    textRole: "name"
                                    valueRole: "id"
                                    font.pixelSize: 12

                                    background: Rectangle { color: "transparent" }
                                    contentItem: Text {
                                        text: datasetCombo.displayText
                                        font.pixelSize: 12
                                        font.family: Theme.fontFamily
                                        color: Theme.textMain
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    indicator: SvgIcon { 
                                        icon: "arrow-down"
                                        width: 12
                                        height: 12
                                        color: Theme.textMuted
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    onActivated: refreshSampleList()
                                }
                            }
                        }

                        // 数据集列表标题行（对标参考UI：无图标）
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.bottomMargin: 8
                            spacing: 4

                            Text {
                                text: "数据集(" + datasetService.listDatasets(appController.currentProjectId || "").length + ")"
                                font.pixelSize: 12
                                font.weight: Font.Normal
                                font.family: Theme.fontFamily
                                color: Theme.textMain
                            }
                        }

                        // 数据集卡片（对标参考UI：名称+进度条+计数）
                        Repeater {
                            model: datasetService.listDatasets(appController.currentProjectId || "")

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                color: Theme.bgCard
                                border.color: Theme.borderColor
                                border.width: 1
                                radius: 6

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    spacing: 8

                                    Text {
                                        text: modelData.name || "默认数据集"
                                        font.pixelSize: 12
                                        font.family: Theme.fontFamily
                                        color: Theme.textMain
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    // 进度条（对标参考UI 6px高，渐变填充）
                                    Rectangle {
                                        Layout.preferredWidth: 60
                                        Layout.preferredHeight: 6
                                        radius: 3
                                        color: Theme.bgMain

                                        Rectangle {
                                            width: parent.width
                                            height: parent.height
                                            radius: 3
                                            gradient: Gradient {
                                                GradientStop { position: 0.0; color: Theme.primary }
                                                GradientStop { position: 1.0; color: Theme.primaryGlow }
                                            }
                                        }
                                    }

                                    Text {
                                        text: (modelData.sampleCount || 0) + "/" + (modelData.sampleCount || 0)
                                        font.pixelSize: 11
                                        font.family: Theme.fontFamilyMono
                                        color: Theme.textMain
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        datasetCombo.currentIndex = index
                                        refreshSampleList()
                                    }
                                }
                            }
                        }
                    }

                    // 分隔线（对标参考UI <hr>）
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        height: 1
                        color: Theme.borderColor
                    }

                    // ====== 区域2：图像导航/切换 ======
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.margins: 12
                        spacing: 0

                        // 当前文件（对标参考UI：无图标，深色卡片居中文字）
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            Layout.bottomMargin: 10
                            color: Theme.bgCard
                            border.color: Theme.borderColor
                            border.width: 1
                            radius: 4

                            Text {
                                anchors.centerIn: parent
                                text: {
                                    if (currentSampleIndex >= 0 && currentSampleIndex < sampleListData.length) {
                                        var path = sampleListData[currentSampleIndex].imagePath || ""
                                        return "Image " + path.split('/').pop().split('\\').pop()
                                    }
                                    return "未选择图像"
                                }
                                font.pixelSize: 12
                                font.family: Theme.fontFamily
                                color: Theme.textMain
                                elide: Text.ElideRight
                                width: parent.width - 20
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        // 前后翻页（对标参考UI：< 1/32 >）
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Item { Layout.fillWidth: true }

                            Button {
                                Layout.preferredWidth: 26
                                Layout.preferredHeight: 26
                                text: "<"
                                font.pixelSize: 14
                                enabled: currentSampleIndex > 0

                                background: Rectangle {
                                    color: parent.enabled ? (parent.hovered ? Theme.bgHover : Theme.bgCard) : Theme.bgCard
                                    border.color: Theme.borderColor
                                    border.width: 1
                                    radius: 4
                                }

                                contentItem: Text {
                                    text: parent.text
                                    font.pixelSize: 14
                                    color: parent.enabled ? Theme.textMain : Theme.textDisabled
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: navigateToPrevious()
                            }

                            Text {
                                text: (currentSampleIndex >= 0 ? (currentSampleIndex + 1) : 0) + " / " + sampleListData.length
                                font.pixelSize: 12
                                font.family: Theme.fontFamilyMono
                                color: Theme.textMain
                                Layout.leftMargin: 12
                                Layout.rightMargin: 12
                            }

                            Button {
                                Layout.preferredWidth: 26
                                Layout.preferredHeight: 26
                                text: ">"
                                font.pixelSize: 14
                                enabled: currentSampleIndex < sampleListData.length - 1

                                background: Rectangle {
                                    color: parent.enabled ? (parent.hovered ? Theme.bgHover : Theme.bgCard) : Theme.bgCard
                                    border.color: Theme.borderColor
                                    border.width: 1
                                    radius: 4
                                }

                                contentItem: Text {
                                    text: parent.text
                                    font.pixelSize: 14
                                    color: parent.enabled ? Theme.textMain : Theme.textDisabled
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: navigateToNext()
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }

                    // 分隔线
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        height: 1
                        color: Theme.borderColor
                    }

                    // ====== 区域3：标签类别 ======
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.margins: 12
                        spacing: 0

                        // 标题行（对标参考UI：无图标）
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.bottomMargin: 8
                            spacing: 4

                            Text {
                                text: "标签类别"
                                font.pixelSize: 12
                                font.weight: Font.Normal
                                font.family: Theme.fontFamily
                                color: Theme.textMain
                            }
                        }

                        // 类别列表（对标参考UI：选中项蓝色高亮，未选中暗色卡片）
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: taxonomyModel

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 32
                                    radius: 4

                                    // 选中态：蓝色背景(primary)，未选中：暗色卡片
                                    color: {
                                        if (annotationMode === "detect" && selectedClassId === model.classIndex) return Theme.primary
                                        if (parent.hovered) return Theme.bgHover
                                        return Theme.bgCard
                                    }
                                    border.color: {
                                        if (annotationMode === "detect" && selectedClassId === model.classIndex) return "transparent"
                                        return Theme.borderColor
                                    }
                                    border.width: {
                                        if (annotationMode === "detect" && selectedClassId === model.classIndex) return 0
                                        return 1
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 10
                                        anchors.rightMargin: 10
                                        spacing: 8

                                        // 类别色块（12x12, radius:2）
                                        Rectangle {
                                            width: 12
                                            height: 12
                                            radius: 2
                                            color: Theme.classColor(model.classIndex)
                                        }

                                        Text {
                                            text: model.className || ("class_" + model.classIndex)
                                            font.pixelSize: 12
                                            font.weight: (annotationMode === "detect" && selectedClassId === model.classIndex) ? Font.DemiBold : Font.Normal
                                            font.family: Theme.fontFamily
                                            color: (annotationMode === "detect" && selectedClassId === model.classIndex) ? "#FFFFFF" : Theme.textMain
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (annotationMode === "classify") {
                                                if (classificationMultiCheck.checked) {
                                                    var idx = selectedMultiClassIds.indexOf(model.classIndex)
                                                    var newIds = selectedMultiClassIds.slice()
                                                    if (idx >= 0) newIds.splice(idx, 1)
                                                    else newIds.push(model.classIndex)
                                                    selectedMultiClassIds = newIds
                                                } else {
                                                    selectedClassId = model.classIndex
                                                }
                                            } else if (annotationMode === "detect") {
                                                selectedClassId = model.classIndex
                                                canvasItem.currentClassIndex = model.classIndex
                                                canvasItem.currentClassName = model.className
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // 分隔线
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        height: 1
                        color: Theme.borderColor
                    }

                    // ====== 区域4：Tag ======
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.margins: 12
                        spacing: 0

                        // 标题行
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.bottomMargin: 8
                            spacing: 4

                            Text {
                                text: "Tag"
                                font.pixelSize: 12
                                font.weight: Font.Normal
                                font.family: Theme.fontFamily
                                color: Theme.textMain
                            }
                        }

                        // 所属数据集提示
                        Text {
                            text: "所属数据集: " + (datasetCombo.currentText || "默认数据集")
                            font.pixelSize: 11
                            font.family: Theme.fontFamily
                            color: Theme.textMuted
                            Layout.bottomMargin: 10
                            Layout.leftMargin: 4
                        }

                        // Tag 按钮网格（2列，对标参考UI grid-template-columns:1fr 1fr）
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8

                            Repeater {
                                model: tagListData.length > 0 ? tagListData : [
                                    {name: "默认", color: Theme.primary, active: true},
                                    {name: "良品", color: "", active: false},
                                    {name: "漏检", color: "", active: false},
                                    {name: "误检", color: "", active: false},
                                    {name: "待定", color: "", active: false},
                                    {name: "重要", color: "", active: false}
                                ]

                                Button {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 30
                                    text: modelData.name
                                    font.pixelSize: 12
                                    font.weight: modelData.active || modelData.color === Theme.primary ? Font.DemiBold : Font.Normal

                                    background: Rectangle {
                                        color: modelData.active || modelData.color === Theme.primary ? Theme.primary : Theme.bgCard
                                        border.color: modelData.active || modelData.color === Theme.primary ? "transparent" : Theme.borderColor
                                        border.width: modelData.active || modelData.color === Theme.primary ? 0 : 1
                                        radius: 4
                                    }

                                    contentItem: Text {
                                        text: parent.text
                                        font.pixelSize: 12
                                        font.weight: parent.font.weight
                                        font.family: Theme.fontFamily
                                        color: modelData.active || modelData.color === Theme.primary ? "#FFFFFF" : Theme.textMain
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: {
                                        // 切换 Tag 选中状态
                                        var newTags = tagListData.slice()
                                        if (newTags.length === 0) {
                                            // 初始化 tagListData
                                            tagListData = [
                                                {name: "默认", color: Theme.primary, active: false},
                                                {name: "良品", color: "", active: false},
                                                {name: "漏检", color: "", active: false},
                                                {name: "误检", color: "", active: false},
                                                {name: "待定", color: "", active: false},
                                                {name: "重要", color: "", active: false}
                                            ]
                                        }
                                        // 切换当前 tag
                                        for (var i = 0; i < tagListData.length; i++) {
                                            if (tagListData[i].name === modelData.name) {
                                                tagListData[i].active = !tagListData[i].active
                                                if (tagListData[i].active) {
                                                    tagListData[i].color = Theme.primary
                                                } else {
                                                    tagListData[i].color = ""
                                                }
                                            }
                                        }
                                        tagListData = tagListData.slice() // 触发绑定更新
                                    }
                                }
                            }
                        }
                    }

                    // 底部弹性空间
                    Item { Layout.fillHeight: true }
                }
            }
        }

        // === 中央画布区 ===
        Rectangle {
            SplitView.fillWidth: true
            color: Theme.bgMain

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // === 顶部筛选栏 (38px) ===
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    color: Theme.bgMain

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


                // 画布区域
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    AnnotCanvasItem {
                        id: canvasItem
                        anchors.fill: parent
                        controller: canvasController
                        annotationModel: annotationModel
                        shapeMode: root.shapeMode
                        currentClassIndex: selectedClassId >= 0 ? selectedClassId : 0
                        currentClassName: selectedClassId >= 0 ? getClassName(selectedClassId) : "class_0"
                        interactionMode: (annotationMode === "detect" && canvasController.drawMode === "draw") ? "draw" : "select"

                        onAnnotationModified: canvasController.markDirty()

                        // C++ 层发出的导航信号
                        onNavigatePrevious: navigateToPrevious()
                        onNavigateNext: navigateToNext()
                        onSaveRequested: saveCurrentAnnotations()

                        // 双击标注弹出编辑标签对话框（参考 X-AnyLabeling）
                        onEditLabelRequested: function(annotationIndex) {
                            editLabelTargetIndex = annotationIndex
                            editLabelDialog.open()
                        }

                        onChangeClassRequested: function(direction) {
                            if (!appController.projectOpen || taxonomyModel.rowCount() === 0) return
                            var selectedRow = -1
                            for (var i = 0; i < annotationModel.rowCount(); i++) {
                                var idx = annotationModel.index(i, 0)
                                if (annotationModel.data(idx, Qt.UserRole + 12)) {
                                    selectedRow = i
                                    break
                                }
                            }
                            if (selectedRow >= 0) {
                                var idxSelected = annotationModel.index(selectedRow, 0)
                                var currentClass = annotationModel.data(idxSelected, Qt.UserRole + 2)
                                var totalClasses = taxonomyModel.rowCount()
                                var newClassIdx = (currentClass + direction + totalClasses) % totalClasses
                                
                                // 获取新类别的名字
                                var idxTax = taxonomyModel.index(newClassIdx, 0)
                                var newClassName = taxonomyModel.data(idxTax, 1) || ("class_" + newClassIdx)
                                
                                // 更新该标注的类别
                                annotationModel.setClassIndex(selectedRow, newClassIdx, newClassName)
                                
                                // 顺便更新当前画笔的默认类别
                                selectedClassId = newClassIdx
                                canvasItem.currentClassIndex = newClassIdx
                                canvasItem.currentClassName = newClassName
                            }
                        }
                    }

                    // === 右侧悬浮工具栏 ===
                    Rectangle {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: 12
                        width: 36
                        height: toolColumn.implicitHeight + 16
                        color: Theme.bgCard
                        radius: Theme.radiusSmall
                        border.color: Theme.borderColor
                        border.width: 1
                        visible: annotationMode === "detect"
                        z: 10

                        ColumnLayout {
                            id: toolColumn
                            anchors.centerIn: parent
                            spacing: 8

                            Repeater {
                                model: [
                                    { icon: "cursor", name: "select", isShape: false },
                                    { icon: "rect", name: "rect", isShape: true, mode: 0 },
                                    { icon: "rotate", name: "obb", isShape: true, mode: 1 },
                                    { icon: "polygon", name: "polygon", isShape: true, mode: 2 },
                                    { icon: "hand", name: "hand", isShape: false },
                                    { icon: "zoom-in", name: "zoom_in", isShape: false },
                                    { icon: "zoom-out", name: "zoom_out", isShape: false }
                                ]

                                Rectangle {
                                    Layout.preferredWidth: 28
                                    Layout.preferredHeight: 28
                                    radius: Theme.radiusSmall
                                    color: {
                                        if (modelData.isShape) {
                                            return (root.shapeMode === modelData.mode && canvasController.drawMode === "draw") ? Theme.bgSelected : (parent.hovered ? Theme.bgHover : "transparent")
                                        } else {
                                            if (modelData.name === "select") return canvasController.drawMode === "select" ? Theme.bgSelected : (parent.hovered ? Theme.bgHover : "transparent")
                                            return parent.hovered ? Theme.bgHover : "transparent"
                                        }
                                    }

                                    SvgIcon {
                                        anchors.centerIn: parent
                                        icon: modelData.icon
                                        width: 16
                                        height: 16
                                        color: {
                                            if (modelData.isShape) {
                                                return (root.shapeMode === modelData.mode && canvasController.drawMode === "draw") ? Theme.primaryGlow : Theme.textMain
                                            } else {
                                                if (modelData.name === "select") return canvasController.drawMode === "select" ? Theme.primaryGlow : Theme.textMain
                                                return Theme.textMain
                                            }
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        hoverEnabled: true
                                        onClicked: {
                                            if (modelData.isShape) {
                                                root.shapeMode = modelData.mode
                                                canvasController.drawMode = "draw"
                                            } else if (modelData.name === "select") {
                                                canvasController.drawMode = "select"
                                            } else if (modelData.name === "hand") {
                                                // 拖拽模式
                                                canvasController.drawMode = "select"
                                            } else if (modelData.name === "zoom_in") {
                                                canvasController.zoomIn()
                                            } else if (modelData.name === "zoom_out") {
                                                canvasController.zoomOut()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // 分类模式浮层
                    Rectangle {
                        id: classificationPanel
                        visible: annotationMode === "classify"
                        anchors.bottom: statusBar.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: Theme.spacingNormal
                        height: classificationLayout.implicitHeight + Theme.spacingLarge * 2
                        color: Theme.bgCard
                        radius: Theme.radiusNormal
                        z: 10
                        border.color: Theme.borderColor
                        border.width: 1

                        ColumnLayout {
                            id: classificationLayout
                            anchors.fill: parent
                            anchors.margins: Theme.spacingNormal
                            spacing: Theme.spacingNormal

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingNormal

                                Text {
                                    text: "分类模式"
                                    font.pixelSize: Theme.fontSizeSubheading
                                    font.weight: Font.DemiBold
                                    color: Theme.primary
                                }

                                Item { Layout.fillWidth: true }

                                CheckBox {
                                    id: classificationMultiCheck
                                    text: "多标签"
                                    font.pixelSize: Theme.fontSizeSmall
                                    checked: false

                                    contentItem: Text {
                                        text: classificationMultiCheck.text
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: classificationMultiCheck.checked ? Theme.primary : Theme.textMain
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: classificationMultiCheck.indicator.width + Theme.spacingSmall
                                    }

                                    indicator: Rectangle {
                                        x: classificationMultiCheck.leftPadding
                                        y: parent.height / 2 - height / 2
                                        width: 16
                                        height: 16
                                        radius: Theme.radiusSmall
                                        color: classificationMultiCheck.checked ? Theme.primary : Theme.bgInput
                                        border.color: classificationMultiCheck.checked ? Theme.primary : Theme.borderColor

                                        Text {
                                            anchors.centerIn: parent
                                            text: classificationMultiCheck.checked ? "\u2713" : ""
                                            color: Theme.bgMain
                                            font.pixelSize: Theme.fontSizeCaption
                                        }
                                    }

                                    onCheckedChanged: {
                                        if (!checked) selectedMultiClassIds = []
                                        else selectedClassId = -1
                                    }
                                }

                                Button {
                                    text: "保存分类"
                                    font.pixelSize: Theme.fontSizeSmall
                                    enabled: classificationMultiCheck.checked ? selectedMultiClassIds.length > 0 : selectedClassId >= 0

                                    background: Rectangle {
                                        color: parent.enabled ? Theme.primary : Theme.bgInput
                                        radius: Theme.radiusSmall
                                    }

                                    contentItem: Text {
                                        text: parent.text
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: parent.enabled ? Theme.bgMain : Theme.textDisabled
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: saveCurrentAnnotations()
                                }
                            }

                            GridView {
                                id: classGrid
                                Layout.fillWidth: true
                                implicitHeight: Math.min(cellHeight * Math.ceil(taxonomyModel.rowCount / Math.max(1, Math.floor((width || 0) / cellWidth) || 1)), 200)
                                cellWidth: 90
                                cellHeight: 36
                                clip: true
                                model: taxonomyModel

                                delegate: Item {
                                    width: classGrid.cellWidth
                                    height: classGrid.cellHeight

                                    Button {
                                        anchors.fill: parent
                                        anchors.margins: 2

                                        property bool isThisSelected: classificationMultiCheck.checked
                                            ? selectedMultiClassIds.indexOf(model.classIndex) >= 0
                                            : selectedClassId === model.classIndex

                                        background: Rectangle {
                                            color: parent.isThisSelected
                                                ? Theme.classColors[model.classIndex % Theme.classColors.length]
                                                : Theme.bgInput
                                            radius: Theme.radiusSmall
                                            border.color: parent.isThisSelected
                                                ? Theme.classColors[model.classIndex % Theme.classColors.length]
                                                : Theme.borderColor
                                            border.width: parent.isThisSelected ? 2 : 1
                                        }

                                        contentItem: Text {
                                            text: model.className || ("class_" + model.classIndex)
                                            font.pixelSize: Theme.fontSizeCaption
                                            color: parent.isThisSelected ? Theme.bgMain : Theme.textMain
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                            elide: Text.ElideRight
                                        }

                                        onClicked: {
                                            if (classificationMultiCheck.checked) {
                                                var idx = selectedMultiClassIds.indexOf(model.classIndex)
                                                var newIds = selectedMultiClassIds.slice()
                                                if (idx >= 0) newIds.splice(idx, 1)
                                                else newIds.push(model.classIndex)
                                                selectedMultiClassIds = newIds
                                            } else {
                                                selectedClassId = model.classIndex
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // 异常检测模式浮层
                    Rectangle {
                        id: anomalyPanel
                        visible: annotationMode === "anomaly"
                        anchors.bottom: statusBar.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: Theme.spacingNormal
                        height: anomalyLayout.implicitHeight + Theme.spacingLarge * 2
                        color: Theme.bgCard
                        radius: Theme.radiusNormal
                        z: 10
                        border.color: Theme.borderColor
                        border.width: 1

                        ColumnLayout {
                            id: anomalyLayout
                            anchors.fill: parent
                            anchors.margins: Theme.spacingNormal
                            spacing: Theme.spacingNormal

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingXLarge

                                Text {
                                    text: "异常检测模式"
                                    font.pixelSize: Theme.fontSizeSubheading
                                    font.weight: Font.DemiBold
                                    color: Theme.primary
                                }

                                Item { Layout.fillWidth: true }

                                Button {
                                    text: "正常"
                                    font.pixelSize: Theme.fontSizeNormal
                                    font.weight: Font.DemiBold
                                    highlighted: !isAnomalous
                                    Layout.preferredWidth: 120
                                    Layout.preferredHeight: 44

                                    background: Rectangle {
                                        color: !isAnomalous ? Theme.success : Theme.bgInput
                                        radius: Theme.radiusNormal
                                        border.color: !isAnomalous ? Theme.success : Theme.borderColor
                                        border.width: !isAnomalous ? 2 : 1
                                    }

                                    contentItem: Text {
                                        text: parent.text
                                        font.pixelSize: Theme.fontSizeNormal
                                        font.weight: Font.DemiBold
                                        color: !isAnomalous ? Theme.bgMain : Theme.textMain
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: isAnomalous = false
                                }

                                Button {
                                    text: "异常"
                                    font.pixelSize: Theme.fontSizeNormal
                                    font.weight: Font.DemiBold
                                    highlighted: isAnomalous
                                    Layout.preferredWidth: 120
                                    Layout.preferredHeight: 44

                                    background: Rectangle {
                                        color: isAnomalous ? Theme.danger : Theme.bgInput
                                        radius: Theme.radiusNormal
                                        border.color: isAnomalous ? Theme.danger : Theme.borderColor
                                        border.width: isAnomalous ? 2 : 1
                                    }

                                    contentItem: Text {
                                        text: parent.text
                                        font.pixelSize: Theme.fontSizeNormal
                                        font.weight: Font.DemiBold
                                        color: isAnomalous ? Theme.bgMain : Theme.textMain
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: isAnomalous = true
                                }

                                Item { Layout.fillWidth: true }

                                Button {
                                    text: "保存"
                                    font.pixelSize: Theme.fontSizeSmall
                                    Layout.preferredHeight: 36

                                    background: Rectangle {
                                        color: Theme.primary
                                        radius: Theme.radiusSmall
                                    }

                                    contentItem: Text {
                                        text: parent.text
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.bgMain
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: saveCurrentAnnotations()
                                }
                            }
                        }
                    }

                    // OBB 旋转浮层
                    Rectangle {
                        id: rotationPanel
                        visible: annotationMode === "detect" && shapeMode === 1
                        anchors.bottom: statusBar.top
                        anchors.right: parent.right
                        anchors.margins: Theme.spacingNormal
                        width: 200
                        height: 44
                        color: Theme.bgCard
                        radius: Theme.radiusSmall
                        z: 10
                        border.color: Theme.borderColor
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingNormal
                            spacing: Theme.spacingSmall

                            Text {
                                text: "旋转"
                                font.pixelSize: Theme.fontSizeCaption
                                color: Theme.textMain
                            }

                            Slider {
                                id: angleSlider
                                Layout.fillWidth: true
                                from: 0
                                to: 360
                                stepSize: 1

                                onValueChanged: {
                                    // IsSelectedRole = Qt.UserRole + 12
                                    for (var i = 0; i < annotationModel.rowCount(); i++) {
                                        var idx = annotationModel.index(i, 0)
                                        if (annotationModel.data(idx, Qt.UserRole + 12)) {
                                            var currentCx = annotationModel.data(idx, Qt.UserRole + 4)
                                            var currentCy = annotationModel.data(idx, Qt.UserRole + 5)
                                            var currentW = annotationModel.data(idx, Qt.UserRole + 6)
                                            var currentH = annotationModel.data(idx, Qt.UserRole + 7)
                                            annotationModel.updateOBBGeometry(i, currentCx, currentCy, currentW, currentH, angleSlider.value)
                                            canvasController.markDirty()
                                            break
                                        }
                                    }
                                }

                                background: Rectangle {
                                    x: angleSlider.leftPadding
                                    y: angleSlider.topPadding + angleSlider.availableHeight / 2 - height / 2
                                    width: angleSlider.availableWidth
                                    height: 4
                                    radius: 2
                                    color: Theme.bgInput

                                    Rectangle {
                                        width: angleSlider.visualPosition * parent.width
                                        height: parent.height
                                        color: Theme.primary
                                        radius: 2
                                    }
                                }

                                handle: Rectangle {
                                    x: angleSlider.leftPadding + angleSlider.visualPosition * (angleSlider.availableWidth - width)
                                    y: angleSlider.topPadding + angleSlider.availableHeight / 2 - height / 2
                                    width: 14
                                    height: 14
                                    radius: 7
                                    color: Theme.primary
                                }
                            }

                            Text {
                                text: Math.round(angleSlider.value) + "\u00B0"
                                font.pixelSize: Theme.fontSizeCaption
                                color: Theme.primary
                                font.family: Theme.fontFamilyMono
                                Layout.preferredWidth: 36
                            }
                        }
                    }
                }

                // 状态栏 (24px)
                Rectangle {
                    id: statusBar
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    color: Theme.bgCard
                    z: 10

                    Rectangle {
                        anchors.top: parent.top
                        width: parent.width
                        height: 1
                        color: Theme.dividerColor
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingNormal

                        // 保存状态
                        Text {
                            text: canvasController.dirty ? "未保存" : "已保存"
                            color: canvasController.dirty ? Theme.danger : Theme.success
                            font.pixelSize: Theme.fontSizeCaption
                        }

                        // 模式
                        Text {
                            text: annotationMode === "classify" ? "分类"
                                  : (annotationMode === "anomaly" ? "异常"
                                     : (shapeMode === 1 ? "旋转框" : (shapeMode === 2 ? "多边形" : "水平框")))
                            color: Theme.primary
                            font.pixelSize: Theme.fontSizeCaption
                        }

                        // 绘制/选择状态
                        Text {
                            text: canvasController.drawMode === "draw" ? "绘制" : "选择"
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeCaption
                            visible: annotationMode === "detect"
                        }

                        Item { Layout.fillWidth: true }

                        // 标注统计
                        Text {
                            text: annotationMode === "classify"
                                  ? (classificationMultiCheck.checked ? selectedMultiClassIds.length + " 个类别" : (selectedClassId >= 0 ? getClassName(selectedClassId) : "未分类"))
                                  : (annotationMode === "anomaly"
                                     ? (isAnomalous ? "异常" : "正常")
                                     : (annotationModel.count + " 个标注"))
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeCaption
                        }

                        // 快捷键提示
                        Text {
                            text: "R矩形 O旋转 P多边形 Esc选择 Del删除 Ctrl+Z/Y撤销重做 Ctrl+S保存"
                            color: Theme.textDisabled
                            font.pixelSize: 10
                            visible: annotationMode === "detect"
                        }
                    }
                }
            }
        }


    }

    // ================================================================
    // 添加标签弹窗
    // ================================================================
    ModalDialog {
        id: addLabelDialog
        title: "添加标签"
        dialogWidth: 500

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingNormal

            // 标签名
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingNormal

                Text {
                    text: "名称"
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textMain
                    Layout.preferredWidth: 60
                }

                TextField {
                    id: labelNameField
                    Layout.fillWidth: true
                    placeholderText: "输入标签名称"
                    placeholderTextColor: Theme.textMuted
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeSmall

                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: labelNameField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                    }
                }
            }

            // 颜色
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingNormal

                Text {
                    text: "颜色"
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textMain
                    Layout.preferredWidth: 60
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSmall

                    Repeater {
                        model: Theme.classColors

                        Rectangle {
                            width: 24
                            height: 24
                            radius: Theme.radiusSmall
                            color: modelData
                            border.color: labelColorField.text === modelData ? Theme.textMain : "transparent"
                            border.width: labelColorField.text === modelData ? 2 : 0

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: labelColorField.text = modelData
                            }
                        }
                    }

                    TextField {
                        id: labelColorField
                        Layout.fillWidth: true
                        placeholderText: "#RRGGBB"
                        placeholderTextColor: Theme.textMuted
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeCaption
                        font.family: Theme.fontFamilyMono

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: labelColorField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                        }
                    }
                }
            }

            // 快捷键
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingNormal

                Text {
                    text: "快捷键"
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textMain
                    Layout.preferredWidth: 60
                }

                TextField {
                    id: labelShortcutField
                    Layout.fillWidth: true
                    placeholderText: "1-9 数字键"
                    placeholderTextColor: Theme.textMuted
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeSmall
                    maximumLength: 1

                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: labelShortcutField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                    }
                }
            }

            // 模型索引
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingNormal

                Text {
                    text: "模型索引"
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textMain
                    Layout.preferredWidth: 60
                }

                SpinBox {
                    id: labelModelIndex
                    Layout.fillWidth: true
                    from: 0
                    to: 999
                    value: taxonomyModel.rowCount()

                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: labelModelIndex.activeFocus ? Theme.primaryGlow : Theme.borderColor
                    }

                    contentItem: TextInput {
                        text: labelModelIndex.textFromValue(labelModelIndex.value, labelModelIndex.locale)
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textMain
                        horizontalAlignment: Qt.AlignHCenter
                        verticalAlignment: Qt.AlignVCenter
                        readOnly: !labelModelIndex.editable
                        validator: labelModelIndex.validator
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                    }

                    up.indicator: Rectangle {
                        x: labelModelIndex.mirrored ? 0 : parent.width - width
                        height: parent.height
                        width: 24
                        color: labelModelIndex.up.pressed ? Theme.bgHover : Theme.bgInput
                        border.color: Theme.borderColor

                        Text {
                            text: "+"
                            font.pixelSize: Theme.fontSizeCaption
                            color: Theme.textMain
                            anchors.centerIn: parent
                        }
                    }

                    down.indicator: Rectangle {
                        x: labelModelIndex.mirrored ? parent.width - width : 0
                        height: parent.height
                        width: 24
                        color: labelModelIndex.down.pressed ? Theme.bgHover : Theme.bgInput
                        border.color: Theme.borderColor

                        Text {
                            text: "−"
                            font.pixelSize: Theme.fontSizeCaption
                            color: Theme.textMain
                            anchors.centerIn: parent
                        }
                    }
                }
            }

            // 下料ID（工业缺陷检测专用）
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingNormal

                Text {
                    text: "下料ID"
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textMain
                    Layout.preferredWidth: 60
                }

                TextField {
                    id: labelMaterialId
                    Layout.fillWidth: true
                    placeholderText: "可选，关联下料工序标识"
                    placeholderTextColor: Theme.textMuted
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeSmall

                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: labelMaterialId.activeFocus ? Theme.primaryGlow : Theme.borderColor
                    }
                }
            }
        }

        // 底部按钮
        footerContent: Row {
            spacing: Theme.spacingNormal

            Button {
                text: "取消"
                font.pixelSize: Theme.fontSizeSmall

                background: Rectangle {
                    color: parent.hovered ? Theme.bgHover : Theme.bgInput
                    radius: Theme.radiusSmall
                    border.color: Theme.borderColor
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textMain
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: addLabelDialog.close()
            }

            Button {
                text: "确定"
                font.pixelSize: Theme.fontSizeSmall
                enabled: labelNameField.text.trim().length > 0

                background: Rectangle {
                    color: parent.enabled ? Theme.primary : Theme.bgInput
                    radius: Theme.radiusSmall
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: Theme.fontSizeSmall
                    color: parent.enabled ? Theme.bgMain : Theme.textDisabled
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    // 通过 taxonomyModel 添加类别（仅名称，其他属性后续可扩展）
                    if (labelNameField.text.trim().length > 0) {
                        taxonomyModel.addClass(labelNameField.text.trim())
                    }
                    addLabelDialog.close()
                    // 清空表单
                    labelNameField.text = ""
                    labelColorField.text = ""
                    labelShortcutField.text = ""
                    labelMaterialId.text = ""
                }
            }
        }
    }

    // ================================================================
    // 添加 Tag 弹窗
    // ================================================================
    ModalDialog {
        id: addTagDialog
        title: "添加 Tag"
        dialogWidth: 400

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingNormal

            // Tag 名称
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingNormal

                Text {
                    text: "名称"
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textMain
                    Layout.preferredWidth: 50
                }

                TextField {
                    id: tagNameField
                    Layout.fillWidth: true
                    placeholderText: "输入 Tag 名称"
                    placeholderTextColor: Theme.textMuted
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeSmall

                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: tagNameField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                    }
                }
            }

            // Tag 颜色
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingNormal

                Text {
                    text: "颜色"
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textMain
                    Layout.preferredWidth: 50
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSmall

                    Repeater {
                        model: [Theme.primary, Theme.success, Theme.danger, Theme.warning, "#D500F9", "#FFD600", "#00E5FF", "#E879F9"]

                        Rectangle {
                            width: 24
                            height: 24
                            radius: Theme.radiusSmall
                            color: modelData
                            border.color: tagColorField.text === modelData ? Theme.textMain : "transparent"
                            border.width: tagColorField.text === modelData ? 2 : 0

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: tagColorField.text = modelData
                            }
                        }
                    }

                    TextField {
                        id: tagColorField
                        Layout.fillWidth: true
                        placeholderText: "#RRGGBB"
                        placeholderTextColor: Theme.textMuted
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeCaption
                        font.family: Theme.fontFamilyMono

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: tagColorField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                        }
                    }
                }
            }
        }

        // 底部按钮
        footerContent: Row {
            spacing: Theme.spacingNormal

            Button {
                text: "取消"
                font.pixelSize: Theme.fontSizeSmall

                background: Rectangle {
                    color: parent.hovered ? Theme.bgHover : Theme.bgInput
                    radius: Theme.radiusSmall
                    border.color: Theme.borderColor
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textMain
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: addTagDialog.close()
            }

            Button {
                text: "确定"
                font.pixelSize: Theme.fontSizeSmall
                enabled: tagNameField.text.trim().length > 0

                background: Rectangle {
                    color: parent.enabled ? Theme.primary : Theme.bgInput
                    radius: Theme.radiusSmall
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: Theme.fontSizeSmall
                    color: parent.enabled ? Theme.bgMain : Theme.textDisabled
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    var newTags = tagListData.slice()
                    newTags.push({
                        name: tagNameField.text.trim(),
                        color: tagColorField.text || Theme.primary
                    })
                    tagListData = newTags
                    addTagDialog.close()
                    tagNameField.text = ""
                    tagColorField.text = ""
                }
            }
        }
    }

    // ================================================================
    // 双击编辑标签对话框（参考 X-AnyLabeling）
    // ================================================================
    Dialog {
        id: editLabelDialog
        title: "编辑标签"
        modal: true
        anchors.centerIn: parent
        width: 320
        standardButtons: Dialog.NoButton

        background: Rectangle {
            color: Theme.bgCard
            border.color: Theme.borderColor
            border.width: 1
            radius: Theme.radiusLarge
        }

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingNormal

            // 当前标注信息
            Text {
                text: editLabelTargetIndex >= 0 && editLabelTargetIndex < annotationModel.rowCount()
                      ? "当前类别: " + (annotationModel.data(annotationModel.index(editLabelTargetIndex, 0), 258) || "未知")
                      : "未选中标注"
                font.pixelSize: Theme.fontSizeNormal
                font.family: Theme.fontFamily
                color: Theme.textMain
                Layout.fillWidth: true
            }

            // 类别选择列表
            Text {
                text: "选择新类别:"
                font.pixelSize: Theme.fontSizeSmall
                font.family: Theme.fontFamily
                color: Theme.textMuted
            }

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(taxonomyModel.rowCount * 36, 240)
                clip: true
                model: taxonomyModel
                spacing: 4

                delegate: Button {
                    width: ListView.view.width
                    height: 32

                    background: Rectangle {
                        color: parent.hovered ? Theme.bgHover : Theme.bgCard
                        border.color: Theme.borderColor
                        border.width: 1
                        radius: 4
                    }

                    contentItem: RowLayout {
                        spacing: Theme.spacingSmall

                        Rectangle {
                            width: 12
                            height: 12
                            radius: 2
                            color: Theme.classColor(model.classIndex)
                            Layout.leftMargin: 8
                        }

                        Text {
                            text: model.className || ("class_" + model.classIndex)
                            font.pixelSize: Theme.fontSizeSmall
                            font.family: Theme.fontFamily
                            color: Theme.textMain
                            Layout.fillWidth: true
                        }
                    }

                    onClicked: {
                        // 修改标注的类别
                        if (editLabelTargetIndex >= 0 && editLabelTargetIndex < annotationModel.rowCount()) {
                            canvasItem.commitUndoState()
                            annotationModel.setClassIndex(editLabelTargetIndex, model.classIndex, model.className)
                            canvasController.markDirty()
                        }
                        editLabelDialog.close()
                    }
                }
            }

            // 取消按钮
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                text: "取消"
                font.pixelSize: Theme.fontSizeSmall

                background: Rectangle {
                    color: parent.hovered ? Theme.bgHover : Theme.bgCard
                    border.color: Theme.borderColor
                    border.width: 1
                    radius: Theme.radiusSmall
                }

                contentItem: Text {
                    text: parent.text
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: Theme.textMain
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: editLabelDialog.close()
            }
        }
    }

    // ================================================================
    // 信号连接
    // ================================================================
    Connections {
        target: appController
        function onCurrentProjectIdChanged() {
            refreshSampleList()
        }
    }

    Connections {
        target: ApplicationWindow.window
        function onCurrentTaskTypeChanged() {
            initAnnotationMode()
        }
    }
}
