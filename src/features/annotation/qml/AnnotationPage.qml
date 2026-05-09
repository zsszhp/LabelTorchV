// AnnotationPage.qml - 标注工作台
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // 左侧样本列表
        Rectangle {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            color: "#181825"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    text: "样本列表"
                    font.pixelSize: 13
                    font.bold: true
                    color: "#cdd6f4"
                    leftPadding: 12
                    verticalAlignment: Text.AlignVCenter
                }

                ListView {
                    id: sampleList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: annotationService.listSamples(appController.currentProjectId ? "" : "")
                    spacing: 2

                    delegate: ItemDelegate {
                        width: sampleList.width
                        height: 36
                        text: modelData.imagePath ? modelData.imagePath.split('/').pop().split('\\').pop() : ""
                        font.pixelSize: 12

                        contentItem: Label {
                            text: parent.text
                            font.pixelSize: 12
                            color: "#cdd6f4"
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            color: parent.hovered ? "#313244" : "transparent"
                        }

                        onClicked: {
                            // Load this sample's annotations
                            annotationModel.loadFromLabel(modelData.labelPath || "")
                            canvasController.loadImage(modelData.imagePath || "", modelData.labelPath || "")
                        }
                    }
                }
            }
        }

        // 中央画布
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#11111b"

            Canvas {
                id: annotationCanvas
                anchors.fill: parent

                onPaint: {
                    var ctx = getContext('2d')
                    ctx.clearRect(0, 0, width, height)

                    // Draw image background (placeholder)
                    ctx.fillStyle = "#1e1e2e"
                    ctx.fillRect(0, 0, width, height)

                    if (!canvasController.currentImagePath) {
                        ctx.fillStyle = "#6c7086"
                        ctx.font = "16px sans-serif"
                        ctx.textAlign = "center"
                        ctx.fillText("选择一个样本开始标注", width / 2, height / 2)
                        return
                    }

                    // Draw image area
                    var imgW = canvasController.imageToCanvasX(1.0) - canvasController.imageToCanvasX(0)
                    var imgH = canvasController.imageToCanvasY(1.0) - canvasController.imageToCanvasY(0)
                    var imgX = canvasController.imageToCanvasX(0)
                    var imgY = canvasController.imageToCanvasY(0)

                    // Image border
                    ctx.strokeStyle = "#45475a"
                    ctx.lineWidth = 1
                    ctx.strokeRect(imgX, imgY, imgW, imgH)

                    // Draw annotations
                    var colors = ["#f38ba8", "#a6e3a1", "#89b4fa", "#f9e2af", "#fab387", "#94e2d5", "#cba6f7", "#f5c2e7", "#89dceb", "#b4befe"]

                    for (var i = 0; i < annotationModel.rowCount(); i++) {
                        var idx = annotationModel.index(i, 0)
                        var cx = annotationModel.data(idx, 259)  // CxRole
                        var cy = annotationModel.data(idx, 260)  // CyRole
                        var w = annotationModel.data(idx, 261)   // WRole
                        var h = annotationModel.data(idx, 262)   // HRole
                        var classIdx = annotationModel.data(idx, 256)  // ClassIndexRole
                        var className = annotationModel.data(idx, 257) // ClassNameRole
                        var selected = annotationModel.data(idx, 264)  // IsSelectedRole

                        if (cx === undefined || cy === undefined) continue

                        var x1 = canvasController.imageToCanvasX(cx - w / 2)
                        var y1 = canvasController.imageToCanvasY(cy - h / 2)
                        var x2 = canvasController.imageToCanvasX(cx + w / 2)
                        var y2 = canvasController.imageToCanvasY(cy + h / 2)

                        var color = colors[classIdx % colors.length]

                        // Fill
                        ctx.globalAlpha = 0.15
                        ctx.fillStyle = color
                        ctx.fillRect(x1, y1, x2 - x1, y2 - y1)

                        // Stroke
                        ctx.globalAlpha = selected ? 1.0 : 0.8
                        ctx.strokeStyle = color
                        ctx.lineWidth = selected ? 3 : 2
                        ctx.strokeRect(x1, y1, x2 - x1, y2 - y1)

                        // Label
                        ctx.globalAlpha = 1.0
                        ctx.fillStyle = color
                        ctx.font = "bold 11px sans-serif"
                        var label = (className || ("class_" + classIdx))
                        var textW = ctx.measureText(label).width + 8
                        ctx.fillRect(x1, y1 - 18, textW, 18)
                        ctx.fillStyle = "#1e1e2e"
                        ctx.fillText(label, x1 + 4, y1 - 5)
                    }

                    // Draw current drawing rect
                    if (drawingRect.visible) {
                        ctx.strokeStyle = "#89b4fa"
                        ctx.lineWidth = 2
                        ctx.setLineDash([4, 4])
                        ctx.strokeRect(drawingRect.x, drawingRect.y, drawingRect.width, drawingRect.height)
                        ctx.setLineDash([])
                    }
                }

                // Mouse handling
                MouseArea {
                    id: canvasMouseArea
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    hoverEnabled: true

                    property real startImgX: 0
                    property real startImgY: 0
                    property bool isDrawing: false

                    onWheel: function(wheel) {
                        var factor = wheel.angleDelta.y > 0 ? 1.1 : 0.9
                        var newZoom = canvasController.zoom * factor
                        if (newZoom < 0.1) newZoom = 0.1
                        if (newZoom > 20) newZoom = 20

                        // Zoom toward mouse position
                        var mx = wheel.x
                        var my = wheel.y
                        canvasController.panX = mx - (mx - canvasController.panX) * (newZoom / canvasController.zoom)
                        canvasController.panY = my - (my - canvasController.panY) * (newZoom / canvasController.zoom)
                        canvasController.zoom = newZoom
                        annotationCanvas.requestPaint()
                    }

                    onPressed: function(mouse) {
                        if (canvasController.drawMode === "draw" && mouse.button === Qt.LeftButton) {
                            isDrawing = true
                            startImgX = mouse.x
                            startImgY = mouse.y
                            drawingRect.x = mouse.x
                            drawingRect.y = mouse.y
                            drawingRect.width = 0
                            drawingRect.height = 0
                            drawingRect.visible = true
                        } else if (canvasController.drawMode === "select" && mouse.button === Qt.LeftButton) {
                            // Check hit on existing annotations
                            var imgX = canvasController.canvasToImageX(mouse.x)
                            var imgY = canvasController.canvasToImageY(mouse.y)
                            selectAnnotationAt(imgX, imgY)
                        }
                    }

                    onPositionChanged: function(mouse) {
                        if (isDrawing) {
                            drawingRect.x = Math.min(startImgX, mouse.x)
                            drawingRect.y = Math.min(startImgY, mouse.y)
                            drawingRect.width = Math.abs(mouse.x - startImgX)
                            drawingRect.height = Math.abs(mouse.y - startImgY)
                            annotationCanvas.requestPaint()
                        }
                    }

                    onReleased: function(mouse) {
                        if (isDrawing && drawingRect.width > 5 && drawingRect.height > 5) {
                            // Convert canvas rect to normalized image coordinates
                            var imgX1 = canvasController.canvasToImageX(drawingRect.x)
                            var imgY1 = canvasController.canvasToImageY(drawingRect.y)
                            var imgX2 = canvasController.canvasToImageX(drawingRect.x + drawingRect.width)
                            var imgY2 = canvasController.canvasToImageY(drawingRect.y + drawingRect.height)

                            var cx = (imgX1 + imgX2) / 2
                            var cy = (imgY1 + imgY2) / 2
                            var w = Math.abs(imgX2 - imgX1)
                            var h = Math.abs(imgY2 - imgY1)

                            if (w > 0.001 && h > 0.001 && cx >= 0 && cy >= 0 && cx <= 1 && cy <= 1) {
                                annotationModel.addAnnotation(0, "class_0", cx, cy, w, h)
                                canvasController.markDirty()
                            }
                        }
                        isDrawing = false
                        drawingRect.visible = false
                        annotationCanvas.requestPaint()
                    }
                }

                // Drawing rect indicator
                QtObject {
                    id: drawingRect
                    property real x: 0
                    property real y: 0
                    property real width: 0
                    property real height: 0
                    property bool visible: false
                }
            }

            // Overlay toolbar
            RowLayout {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 8
                spacing: 4

                Button {
                    text: canvasController.drawMode === "draw" ? "绘制中" : "选择"
                    highlighted: canvasController.drawMode === "draw"
                    onClicked: canvasController.drawMode = canvasController.drawMode === "draw" ? "select" : "draw"
                }

                Button {
                    text: "适应"
                    onClicked: {
                        canvasController.fitToView(annotationCanvas.width, annotationCanvas.height)
                        annotationCanvas.requestPaint()
                    }
                }

                Button {
                    text: "保存"
                    enabled: canvasController.dirty
                    highlighted: true
                    onClicked: {
                        annotationService.saveAnnotations(
                            canvasController.currentLabelPath,
                            "", "",
                            annotationModel.toVariantList()
                        )
                        canvasController.clearDirty()
                    }
                }

                Label {
                    text: Math.round(canvasController.zoom * 100) + "%"
                    color: "#6c7086"
                    font.pixelSize: 11
                }
            }

            // Status bar
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 24
                color: "#11111b"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8

                    Label {
                        text: canvasController.dirty ? "未保存" : "已保存"
                        color: canvasController.dirty ? "#f38ba8" : "#a6e3a1"
                        font.pixelSize: 11
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: annotationModel.count + " 个标注"
                        color: "#6c7086"
                        font.pixelSize: 11
                    }
                }
            }
        }

        // 右侧类别面板
        Rectangle {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            color: "#181825"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    text: "类别"
                    font.pixelSize: 13
                    font.bold: true
                    color: "#cdd6f4"
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

                        contentItem: Row {
                            spacing: 8
                            leftPadding: 8
                            Rectangle {
                                width: 16; height: 16; radius: 2
                                color: ["#f38ba8", "#a6e3a1", "#89b4fa", "#f9e2af", "#fab387", "#94e2d5", "#cba6f7", "#f5c2e7", "#89dceb", "#b4befe"][model.classIndex % 10]
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Label {
                                text: model.className
                                font.pixelSize: 12
                                color: "#cdd6f4"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        background: Rectangle { color: parent.hovered ? "#313244" : "transparent" }

                        onClicked: {
                            // Set current class for drawing
                        }
                    }
                }

                ItemDelegate {
                    Layout.fillWidth: true
                    text: "删除选中"
                    font.pixelSize: 12
                    visible: annotationModel.count > 0

                    contentItem: Label {
                        text: parent.text
                        font.pixelSize: 12
                        color: "#f38ba8"
                        horizontalAlignment: Text.AlignHCenter
                    }

                    background: Rectangle { color: parent.hovered ? "#313244" : "transparent" }

                    onClicked: {
                        // Remove selected annotations
                        for (var i = annotationModel.rowCount() - 1; i >= 0; i--) {
                            var idx = annotationModel.index(i, 0)
                            if (annotationModel.data(idx, 264)) { // IsSelectedRole
                                annotationModel.removeAnnotation(i)
                                canvasController.markDirty()
                            }
                        }
                        annotationCanvas.requestPaint()
                    }
                }
            }
        }
    }

    function selectAnnotationAt(imgX, imgY) {
        var found = false
        for (var i = annotationModel.rowCount() - 1; i >= 0; i--) {
            var idx = annotationModel.index(i, 0)
            var cx = annotationModel.data(idx, 259)
            var cy = annotationModel.data(idx, 260)
            var w = annotationModel.data(idx, 261)
            var h = annotationModel.data(idx, 262)

            var left = cx - w / 2
            var right = cx + w / 2
            var top = cy - h / 2
            var bottom = cy + h / 2

            if (imgX >= left && imgX <= right && imgY >= top && imgY <= bottom) {
                annotationModel.setSelected(i, !annotationModel.data(idx, 264))
                found = true
                break
            }
        }
        if (!found) {
            // Deselect all
            for (var j = 0; j < annotationModel.rowCount(); j++) {
                var jdx = annotationModel.index(j, 0)
                if (annotationModel.data(jdx, 264)) {
                    annotationModel.setSelected(j, false)
                }
            }
        }
        annotationCanvas.requestPaint()
    }

    Connections {
        target: canvasController
        function onCanvasUpdateRequested() {
            annotationCanvas.requestPaint()
        }
    }

    // Keyboard shortcuts
    Shortcut {
        sequence: "W"
        onActivated: canvasController.drawMode = "draw"
    }
    Shortcut {
        sequence: "Escape"
        onActivated: canvasController.drawMode = "select"
    }
    Shortcut {
        sequence: "Delete"
        onActivated: {
            for (var i = annotationModel.rowCount() - 1; i >= 0; i--) {
                var idx = annotationModel.index(i, 0)
                if (annotationModel.data(idx, 264)) {
                    annotationModel.removeAnnotation(i)
                    canvasController.markDirty()
                }
            }
            annotationCanvas.requestPaint()
        }
    }
    Shortcut {
        sequence: "Ctrl+S"
        onActivated: {
            if (canvasController.dirty) {
                annotationService.saveAnnotations(
                    canvasController.currentLabelPath,
                    "", "",
                    annotationModel.toVariantList()
                )
                canvasController.clearDirty()
            }
        }
    }
}
