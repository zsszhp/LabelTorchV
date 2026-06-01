// AnnotationPage.qml - 标注工作台（v1.0.0 高性能画布版）
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme
import LabelTorch.Annotation 1.0
import QtQuick.Layouts

Item {
    id: root

    property int shapeMode: 0
    property string annotationMode: "detect"
    property int selectedClassId: -1
    property var selectedMultiClassIds: []
    property bool isAnomalous: false
    property var sampleListData: []

    Connections {
        target: ApplicationWindow.window
        function onCurrentTaskTypeChanged() {
            var taskType = ApplicationWindow.window.currentTaskType
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
    }

    Component.onCompleted: {
        if (appController.projectOpen) {
            var taskType = projectService.getTaskType(appController.currentProjectId)
            if (taskType === "detect") {
                root.annotationMode = "detect"
                root.shapeMode = 0
            } else if (taskType === "obb") {
                root.annotationMode = "detect"
                root.shapeMode = 1
            } else if (taskType === "classify") {
                root.annotationMode = "classify"
            } else if (taskType === "anomaly") {
                root.annotationMode = "anomaly"
            }
            refreshSampleList()
        }
    }

    onVisibleChanged: {
        if (visible && appController.projectOpen) {
            refreshSampleList()
        }
    }

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

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: Theme.bgCard

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    text: "样本列表 (" + sampleListData.length + ")"
                    font.pixelSize: 13
                    font.bold: true
                    color: Theme.textPrimary
                    leftPadding: 12
                    verticalAlignment: Text.AlignVCenter
                }

                TextField {
                    id: sampleFilter
                    Layout.fillWidth: true
                    Layout.margins: 4
                    placeholderText: "搜索样本..."
                    placeholderTextColor: Theme.textMuted
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    leftPadding: 8

                    background: Rectangle {
                        color: Theme.bgInput
                        radius: 4
                        border.color: sampleFilter.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                    }
                }

                ListView {
                    id: sampleList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: sampleListData
                    spacing: 1

                    delegate: ItemDelegate {
                        width: sampleList.width
                        height: 36

                        property var sampleData: modelData
                        property string fileName: sampleData.imagePath ? sampleData.imagePath.split('/').pop().split('\\').pop() : ""
                        property bool isCurrentSample: canvasController.currentImagePath === sampleData.imagePath

                        contentItem: Row {
                            spacing: 6
                            leftPadding: 8
                            Rectangle {
                                width: 4; height: parent.height - 8
                                radius: 2
                                color: isCurrentSample ? Theme.accentPrimary : "transparent"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: fileName
                                font.pixelSize: 11
                                color: isCurrentSample ? Theme.accentPrimary : Theme.textPrimary
                                elide: Text.ElideRight
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        background: Rectangle {
                            color: isCurrentSample ? Theme.bgInput : (parent.hovered ? Theme.bgInput : "transparent")
                        }

                        onClicked: loadSample(sampleData)
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.margins: 4
                    Layout.preferredHeight: 28
                    text: "刷新列表"
                    font.pixelSize: 11

                    background: Rectangle {
                        color: parent.hovered ? Theme.bgInput : Theme.bgPrimary
                        radius: 4
                        border.color: Theme.borderNormal
                    }

                    contentItem: Label {
                        text: parent.text
                        font.pixelSize: 11
                        color: Theme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: refreshSampleList()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgPrimary

            AnnotCanvasItem {
                id: canvasItem
                anchors.fill: parent
                controller: canvasController
                annotationModel: annotationModel
                shapeMode: root.shapeMode
                currentClassIndex: selectedClassId >= 0 ? selectedClassId : 0
                currentClassName: selectedClassId >= 0 ? getClassName(selectedClassId) : "class_0"
                interactionMode: (annotationMode === "detect" && canvasController.drawMode === "draw") ? "draw" : "select"

                onAnnotationModified: {
                    canvasController.markDirty()
                }
            }

            RowLayout {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 8
                spacing: 4
                z: 10

                RowLayout {
                    spacing: 0

                    Button {
                        text: "水平框"
                        font.pixelSize: 11
                        highlighted: annotationMode === "detect" && shapeMode === 0
                        flat: !highlighted
                        onClicked: {
                            annotationMode = "detect"
                            shapeMode = 0
                            canvasItem.shapeMode = 0
                            annotationService.setShapeType(0)
                        }

                        background: Rectangle {
                            color: parent.highlighted ? Theme.accentPrimary : Theme.bgInput
                            radius: 3
                        }

                        contentItem: Label {
                            text: parent.text
                            font.pixelSize: 11
                            color: parent.highlighted ? Theme.bgPrimary : Theme.textPrimary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Button {
                        text: "旋转框"
                        font.pixelSize: 11
                        highlighted: annotationMode === "detect" && shapeMode === 1
                        flat: !highlighted
                        onClicked: {
                            annotationMode = "detect"
                            shapeMode = 1
                            canvasItem.shapeMode = 1
                            annotationService.setShapeType(1)
                        }

                        background: Rectangle {
                            color: parent.highlighted ? Theme.accentPrimary : Theme.bgInput
                            radius: 3
                        }

                        contentItem: Label {
                            text: parent.text
                            font.pixelSize: 11
                            color: parent.highlighted ? Theme.bgPrimary : Theme.textPrimary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Button {
                        text: "分类"
                        font.pixelSize: 11
                        highlighted: annotationMode === "classify"
                        flat: !highlighted
                        onClicked: annotationMode = "classify"

                        background: Rectangle {
                            color: parent.highlighted ? Theme.accentPrimary : Theme.bgInput
                            radius: 3
                        }

                        contentItem: Label {
                            text: parent.text
                            font.pixelSize: 11
                            color: parent.highlighted ? Theme.bgPrimary : Theme.textPrimary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Button {
                        text: "异常"
                        font.pixelSize: 11
                        highlighted: annotationMode === "anomaly"
                        flat: !highlighted
                        onClicked: annotationMode = "anomaly"

                        background: Rectangle {
                            color: parent.highlighted ? Theme.accentPrimary : Theme.bgInput
                            radius: 3
                        }

                        contentItem: Label {
                            text: parent.text
                            font.pixelSize: 11
                            color: parent.highlighted ? Theme.bgPrimary : Theme.textPrimary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                Button {
                    text: canvasController.drawMode === "draw" ? "绘制中" : "绘制"
                    highlighted: canvasController.drawMode === "draw"
                    visible: annotationMode === "detect"
                    font.pixelSize: 11
                    onClicked: {
                        canvasController.drawMode = canvasController.drawMode === "draw" ? "select" : "draw"
                    }

                    background: Rectangle {
                        color: parent.highlighted ? Theme.accentPrimary : Theme.bgInput
                        radius: 4
                        border.color: parent.highlighted ? Theme.accentPrimary : Theme.borderNormal
                    }

                    contentItem: Label {
                        text: parent.text
                        font.pixelSize: 11
                        color: parent.highlighted ? Theme.bgPrimary : Theme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    text: "适应"
                    font.pixelSize: 11
                    onClicked: canvasItem.fitToView()

                    background: Rectangle {
                        color: parent.hovered ? Theme.bgInput : Theme.bgPrimary
                        radius: 4
                        border.color: Theme.borderNormal
                    }

                    contentItem: Label {
                        text: parent.text
                        font.pixelSize: 11
                        color: Theme.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    text: "撤销"
                    font.pixelSize: 11
                    enabled: canvasItem.canUndo()
                    visible: annotationMode === "detect"
                    onClicked: canvasItem.undo()

                    background: Rectangle {
                        color: parent.enabled ? (parent.hovered ? Theme.bgInput : Theme.bgPrimary) : Theme.bgPrimary
                        radius: 4
                        border.color: parent.enabled ? Theme.borderNormal : Theme.borderDisabled
                    }

                    contentItem: Label {
                        text: parent.text
                        font.pixelSize: 11
                        color: parent.enabled ? Theme.textPrimary : Theme.textDisabled
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    text: "重做"
                    font.pixelSize: 11
                    enabled: canvasItem.canRedo()
                    visible: annotationMode === "detect"
                    onClicked: canvasItem.redo()

                    background: Rectangle {
                        color: parent.enabled ? (parent.hovered ? Theme.bgInput : Theme.bgPrimary) : Theme.bgPrimary
                        radius: 4
                        border.color: parent.enabled ? Theme.borderNormal : Theme.borderDisabled
                    }

                    contentItem: Label {
                        text: parent.text
                        font.pixelSize: 11
                        color: parent.enabled ? Theme.textPrimary : Theme.textDisabled
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    text: "保存"
                    font.pixelSize: 11
                    highlighted: true
                    visible: annotationMode === "detect"
                    enabled: canvasController.dirty
                    onClicked: {
                        canvasItem.commitUndoState()
                        annotationService.saveAnnotations(
                            canvasController.currentLabelPath,
                            "", "",
                            annotationModel.toVariantList()
                        )
                        canvasController.clearDirty()
                    }

                    background: Rectangle {
                        color: parent.enabled ? Theme.accentPrimary : Theme.bgPrimary
                        radius: 4
                    }

                    contentItem: Label {
                        text: parent.text
                        font.pixelSize: 11
                        color: parent.enabled ? Theme.bgPrimary : Theme.textDisabled
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Label {
                    text: Math.round(canvasController.zoom * 100) + "%"
                    color: Theme.textMuted
                    font.pixelSize: 11
                }
            }

            Rectangle {
                id: classificationPanel
                visible: annotationMode === "classify"
                anchors.bottom: statusBar.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 8
                height: classificationLayout.implicitHeight + 16
                color: Theme.bgCard
                radius: 6
                z: 10

                ColumnLayout {
                    id: classificationLayout
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: "分类模式"
                            font.pixelSize: 13
                            font.bold: true
                            color: Theme.accentPrimary
                        }

                        Item { Layout.fillWidth: true }

                        CheckBox {
                            id: classificationMultiCheck
                            text: "多标签"
                            font.pixelSize: 12
                            checked: false

                            contentItem: Label {
                                text: classificationMultiCheck.text
                                font.pixelSize: 12
                                color: classificationMultiCheck.checked ? Theme.accentPrimary : Theme.textPrimary
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: classificationMultiCheck.indicator.width + 6
                            }

                            indicator: Rectangle {
                                x: classificationMultiCheck.leftPadding
                                y: parent.height / 2 - height / 2
                                width: 16
                                height: 16
                                radius: 3
                                color: classificationMultiCheck.checked ? Theme.accentPrimary : Theme.bgInput
                                border.color: classificationMultiCheck.checked ? Theme.accentPrimary : Theme.borderNormal

                                Label {
                                    anchors.centerIn: parent
                                    text: classificationMultiCheck.checked ? "\u2713" : ""
                                    color: Theme.bgPrimary
                                    font.pixelSize: 11
                                }
                            }

                            onCheckedChanged: {
                                if (!checked) selectedMultiClassIds = []
                                else selectedClassId = -1
                            }
                        }

                        Button {
                            text: "保存分类"
                            highlighted: true
                            font.pixelSize: 12
                            enabled: classificationMultiCheck.checked ? selectedMultiClassIds.length > 0 : selectedClassId >= 0

                            background: Rectangle {
                                color: parent.enabled ? Theme.accentPrimary : Theme.bgPrimary
                                radius: 4
                            }

                            contentItem: Label {
                                text: parent.text
                                font.pixelSize: 12
                                color: parent.enabled ? Theme.bgPrimary : Theme.textDisabled
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
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
                                    radius: 4
                                    border.color: parent.isThisSelected
                                        ? Theme.classColors[model.classIndex % Theme.classColors.length]
                                        : Theme.borderNormal
                                    border.width: parent.isThisSelected ? 2 : 1
                                }

                                contentItem: Label {
                                    text: model.className || ("class_" + model.classIndex)
                                    font.pixelSize: 11
                                    color: parent.isThisSelected ? Theme.bgPrimary : Theme.textPrimary
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

            Rectangle {
                id: anomalyPanel
                visible: annotationMode === "anomaly"
                anchors.bottom: statusBar.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 8
                height: anomalyLayout.implicitHeight + 16
                color: Theme.bgCard
                radius: 6
                z: 10

                ColumnLayout {
                    id: anomalyLayout
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Label {
                            text: "异常检测模式"
                            font.pixelSize: 13
                            font.bold: true
                            color: Theme.accentPrimary
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: "正常"
                            font.pixelSize: 14
                            font.bold: true
                            highlighted: !isAnomalous
                            Layout.preferredWidth: 120
                            Layout.preferredHeight: 44

                            background: Rectangle {
                                color: !isAnomalous ? Theme.accentSuccess : Theme.bgInput
                                radius: 6
                                border.color: !isAnomalous ? Theme.accentSuccess : Theme.borderNormal
                                border.width: !isAnomalous ? 2 : 1
                            }

                            contentItem: Label {
                                text: parent.text
                                font.pixelSize: 14
                                font.bold: true
                                color: !isAnomalous ? Theme.bgPrimary : Theme.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: isAnomalous = false
                        }

                        Button {
                            text: "异常"
                            font.pixelSize: 14
                            font.bold: true
                            highlighted: isAnomalous
                            Layout.preferredWidth: 120
                            Layout.preferredHeight: 44

                            background: Rectangle {
                                color: isAnomalous ? Theme.accentError : Theme.bgInput
                                radius: 6
                                border.color: isAnomalous ? Theme.accentError : Theme.borderNormal
                                border.width: isAnomalous ? 2 : 1
                            }

                            contentItem: Label {
                                text: parent.text
                                font.pixelSize: 14
                                font.bold: true
                                color: isAnomalous ? Theme.bgPrimary : Theme.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: isAnomalous = true
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: "保存"
                            highlighted: true
                            Layout.preferredHeight: 36

                            background: Rectangle {
                                color: Theme.accentPrimary
                                radius: 4
                            }

                            contentItem: Label {
                                text: parent.text
                                font.pixelSize: 12
                                color: Theme.bgPrimary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                annotationService.saveAnomalyLabels(
                                    canvasController.currentLabelPath, "", "", isAnomalous
                                )
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: rotationPanel
                visible: annotationMode === "detect" && shapeMode === 1
                anchors.bottom: statusBar.top
                anchors.right: parent.right
                anchors.margins: 8
                width: 200
                height: 44
                color: Theme.bgCard
                radius: 4
                z: 10

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    Label {
                        text: "旋转"
                        font.pixelSize: 11
                        color: Theme.textPrimary
                    }

                    Slider {
                        id: angleSlider
                        Layout.fillWidth: true
                        from: 0
                        to: 360
                        stepSize: 1

                        onValueChanged: {
                            for (var i = 0; i < annotationModel.rowCount(); i++) {
                                var idx = annotationModel.index(i, 0)
                                if (annotationModel.data(idx, Qt.UserRole + 8)) {
                                    var currentCx = annotationModel.data(idx, Qt.UserRole + 3)
                                    var currentCy = annotationModel.data(idx, Qt.UserRole + 4)
                                    var currentW = annotationModel.data(idx, Qt.UserRole + 5)
                                    var currentH = annotationModel.data(idx, Qt.UserRole + 6)
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
                                color: Theme.accentPrimary
                                radius: 2
                            }
                        }

                        handle: Rectangle {
                            x: angleSlider.leftPadding + angleSlider.visualPosition * (angleSlider.availableWidth - width)
                            y: angleSlider.topPadding + angleSlider.availableHeight / 2 - height / 2
                            width: 14
                            height: 14
                            radius: 7
                            color: Theme.accentPrimary
                        }
                    }

                    Label {
                        text: Math.round(angleSlider.value) + "\u00B0"
                        font.pixelSize: 11
                        color: Theme.accentPrimary
                        Layout.preferredWidth: 36
                    }
                }
            }

            Rectangle {
                id: statusBar
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 24
                color: Theme.bgCard
                z: 10

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8

                    Label {
                        text: canvasController.dirty ? "未保存" : "已保存"
                        color: canvasController.dirty ? Theme.accentError : Theme.accentSuccess
                        font.pixelSize: 11
                    }

                    Label {
                        text: annotationMode === "classify" ? "分类" : (annotationMode === "anomaly" ? "异常" : (shapeMode === 1 ? "旋转框" : "水平框"))
                        color: Theme.accentPrimary
                        font.pixelSize: 11
                    }

                    Label {
                        text: canvasController.drawMode === "draw" ? "绘制" : "选择"
                        color: Theme.textMuted
                        font.pixelSize: 11
                        visible: annotationMode === "detect"
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: annotationMode === "classify"
                            ? (classificationMultiCheck.checked ? selectedMultiClassIds.length + " 个类别" : (selectedClassId >= 0 ? getClassName(selectedClassId) : "未分类"))
                            : (annotationMode === "anomaly"
                               ? (isAnomalous ? "异常" : "正常")
                               : (annotationModel.count + " 个标注"))
                        color: Theme.textMuted
                        font.pixelSize: 11
                    }

                    Label {
                        text: "W绘制 | Space平移 | Del删除 | Ctrl+Z撤销"
                        color: Theme.textDisabled
                        font.pixelSize: 10
                        visible: annotationMode === "detect"
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            color: Theme.bgCard

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    text: "类别"
                    font.pixelSize: 13
                    font.bold: true
                    color: Theme.textPrimary
                    leftPadding: 12
                    verticalAlignment: Text.AlignVCenter
                }

                ListView {
                    id: classListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: taxonomyModel
                    spacing: 2

                    delegate: ItemDelegate {
                        width: classListView.width
                        height: 32

                        property bool isDrawingClass: annotationMode === "detect" && selectedClassId === model.classIndex

                        contentItem: Row {
                            spacing: 8
                            leftPadding: 8
                            Rectangle {
                                width: 16; height: 16; radius: 2
                                color: Theme.classColors[model.classIndex % Theme.classColors.length]
                                anchors.verticalCenter: parent.verticalCenter

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 2
                                    border.color: Theme.bgPrimary
                                    border.width: isDrawingClass ? 2 : 0
                                    visible: isDrawingClass
                                }
                            }
                            Label {
                                text: model.className
                                font.pixelSize: 12
                                color: isDrawingClass ? Theme.accentPrimary : Theme.textPrimary
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        background: Rectangle {
                            color: isDrawingClass ? Theme.bgInput : (parent.hovered ? Theme.bgInput : "transparent")
                        }

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

                ItemDelegate {
                    Layout.fillWidth: true
                    text: "删除选中"
                    font.pixelSize: 12
                    visible: annotationMode !== "classify" && annotationMode !== "anomaly" && annotationModel.count > 0

                    contentItem: Label {
                        text: parent.text
                        font.pixelSize: 12
                        color: Theme.accentError
                        horizontalAlignment: Text.AlignHCenter
                    }

                    background: Rectangle { color: parent.hovered ? Theme.bgInput : "transparent" }

                    onClicked: canvasItem.deleteSelected()
                }
            }
        }
    }

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
    }

    function getClassName(classIndex) {
        for (var i = 0; i < taxonomyModel.rowCount(); i++) {
            var idx = taxonomyModel.index(i, 0)
            if (taxonomyModel.data(idx, 0) === classIndex) {
                return taxonomyModel.data(idx, 1) || ("class_" + classIndex)
            }
        }
        return "class_" + classIndex
    }

    Connections {
        target: appController
        function onCurrentProjectIdChanged() {
            refreshSampleList()
        }
    }

    Shortcut {
        sequence: "W"
        onActivated: {
            if (annotationMode === "detect") {
                canvasController.drawMode = "draw"
            }
        }
    }
    Shortcut {
        sequence: "Escape"
        onActivated: canvasController.drawMode = "select"
    }
    Shortcut {
        sequence: "Delete"
        onActivated: {
            if (annotationMode === "detect") canvasItem.deleteSelected()
        }
    }
    Shortcut {
        sequence: "Ctrl+Z"
        onActivated: {
            if (annotationMode === "detect") canvasItem.undo()
        }
    }
    Shortcut {
        sequence: "Ctrl+Shift+Z"
        onActivated: {
            if (annotationMode === "detect") canvasItem.redo()
        }
    }
    Shortcut {
        sequence: "Ctrl+S"
        onActivated: {
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
    }
    Shortcut {
        sequence: "F"
        onActivated: canvasItem.fitToView()
    }
    Shortcut {
        sequence: "Ctrl+A"
        onActivated: canvasItem.selectAll()
    }
}
