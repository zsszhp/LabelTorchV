// AnnotationPage.qml - 标注工作台
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    // Current shape mode: 0 = HBB, 1 = OBB
    property int shapeMode: 0

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
                        var annAngle = annotationModel.data(idx, 263) // AngleRole
                        var classIdx = annotationModel.data(idx, 256)  // ClassIndexRole
                        var className = annotationModel.data(idx, 257) // ClassNameRole
                        var selected = annotationModel.data(idx, 264)  // IsSelectedRole

                        if (cx === undefined || cy === undefined) continue

                        var color = colors[classIdx % colors.length]

                        // Center in canvas coordinates
                        var canvasCx = canvasController.imageToCanvasX(cx)
                        var canvasCy = canvasController.imageToCanvasY(cy)
                        var canvasHalfW = (canvasController.imageToCanvasX(cx + w / 2) - canvasController.imageToCanvasX(cx - w / 2)) / 2
                        var canvasHalfH = (canvasController.imageToCanvasY(cy + h / 2) - canvasController.imageToCanvasY(cy - h / 2)) / 2

                        if (annAngle !== undefined && annAngle !== 0 && shapeMode === 1) {
                            // OBB mode: draw rotated rectangle
                            var rad = annAngle * Math.PI / 180.0
                            ctx.save()
                            ctx.translate(canvasCx, canvasCy)
                            ctx.rotate(rad)

                            // Fill
                            ctx.globalAlpha = 0.15
                            ctx.fillStyle = color
                            ctx.fillRect(-canvasHalfW, -canvasHalfH, canvasHalfW * 2, canvasHalfH * 2)

                            // Stroke
                            ctx.globalAlpha = selected ? 1.0 : 0.8
                            ctx.strokeStyle = color
                            ctx.lineWidth = selected ? 3 : 2
                            ctx.strokeRect(-canvasHalfW, -canvasHalfH, canvasHalfW * 2, canvasHalfH * 2)

                            // Rotation indicator line
                            ctx.globalAlpha = 0.5
                            ctx.strokeStyle = "#cdd6f4"
                            ctx.lineWidth = 1
                            ctx.beginPath()
                            ctx.moveTo(0, 0)
                            ctx.lineTo(canvasHalfW, 0)
                            ctx.stroke()

                            ctx.restore()
                        } else {
                            // HBB mode: draw axis-aligned rectangle
                            var x1 = canvasController.imageToCanvasX(cx - w / 2)
                            var y1 = canvasController.imageToCanvasY(cy - h / 2)
                            var x2 = canvasController.imageToCanvasX(cx + w / 2)
                            var y2 = canvasController.imageToCanvasY(cy + h / 2)

                            // Fill
                            ctx.globalAlpha = 0.15
                            ctx.fillStyle = color
                            ctx.fillRect(x1, y1, x2 - x1, y2 - y1)

                            // Stroke
                            ctx.globalAlpha = selected ? 1.0 : 0.8
                            ctx.strokeStyle = color
                            ctx.lineWidth = selected ? 3 : 2
                            ctx.strokeRect(x1, y1, x2 - x1, y2 - y1)
                        }

                        // Label (always drawn at the top of the annotation)
                        var labelX, labelY
                        if (annAngle !== undefined && annAngle !== 0 && shapeMode === 1) {
                            // For OBB, place label at the rotated top-left corner area
                            labelX = canvasCx - canvasHalfW * Math.cos(annAngle * Math.PI / 180) - 4
                            labelY = canvasCy - canvasHalfW * Math.sin(annAngle * Math.PI / 180) - 4
                        } else {
                            labelX = canvasController.imageToCanvasX(cx - w / 2)
                            labelY = canvasController.imageToCanvasY(cy - h / 2) - 18
                        }

                        ctx.globalAlpha = 1.0
                        ctx.fillStyle = color
                        ctx.font = "bold 11px sans-serif"
                        var label = (className || ("class_" + classIdx))
                        var textW = ctx.measureText(label).width + 8
                        ctx.fillRect(labelX, labelY, textW, 18)
                        ctx.fillStyle = "#1e1e2e"
                        ctx.fillText(label, labelX + 4, labelY + 13)
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
                                if (shapeMode === 1) {
                                    annotationModel.addOBBAnnotation(0, "class_0", cx, cy, w, h, 0.0)
                                } else {
                                    annotationModel.addAnnotation(0, "class_0", cx, cy, w, h)
                                }
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

                // HBB/OBB mode switch
                RowLayout {
                    spacing: 0

                    Button {
                        text: "HBB"
                        font.pixelSize: 11
                        highlighted: shapeMode === 0
                        flat: !highlighted
                        onClicked: {
                            shapeMode = 0
                            annotationService.setShapeType(0)
                        }

                        background: Rectangle {
                            color: parent.highlighted ? "#89b4fa" : "#313244"
                            radius: 3

                            Rectangle {
                                anchors.right: parent.right
                                width: obbBtn.visible ? 0 : parent.radius
                                color: parent.color
                            }
                        }

                        contentItem: Label {
                            text: parent.text
                            font.pixelSize: 11
                            color: parent.highlighted ? "#1e1e2e" : "#cdd6f4"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Button {
                        id: obbBtn
                        text: "OBB"
                        font.pixelSize: 11
                        highlighted: shapeMode === 1
                        flat: !highlighted
                        onClicked: {
                            shapeMode = 1
                            annotationService.setShapeType(1)
                        }

                        background: Rectangle {
                            color: parent.highlighted ? "#89b4fa" : "#313244"
                            radius: 3

                            Rectangle {
                                anchors.left: parent.left
                                width: hbbBtn.visible ? 0 : parent.radius
                                color: parent.color
                            }
                        }

                        contentItem: Label {
                            text: parent.text
                            font.pixelSize: 11
                            color: parent.highlighted ? "#1e1e2e" : "#cdd6f4"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

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

            // OBB rotation control (only visible in OBB mode when an annotation is selected)
            Rectangle {
                id: rotationPanel
                visible: shapeMode === 1 && hasSelectedAnnotation()
                anchors.bottom: statusBar.top
                anchors.right: parent.right
                anchors.margins: 8
                width: 200
                height: 44
                color: "#181825"
                radius: 4

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    Label {
                        text: "旋转"
                        font.pixelSize: 11
                        color: "#cdd6f4"
                    }

                    Slider {
                        id: angleSlider
                        Layout.fillWidth: true
                        from: 0
                        to: 360
                        stepSize: 1

                        onValueChanged: {
                            // Update the angle of the selected annotation
                            for (var i = 0; i < annotationModel.rowCount(); i++) {
                                var idx = annotationModel.index(i, 0)
                                if (annotationModel.data(idx, 264)) { // IsSelectedRole
                                    var currentCx = annotationModel.data(idx, 259)
                                    var currentCy = annotationModel.data(idx, 260)
                                    var currentW = annotationModel.data(idx, 261)
                                    var currentH = annotationModel.data(idx, 262)
                                    annotationModel.updateOBBGeometry(i, currentCx, currentCy, currentW, currentH, angleSlider.value)
                                    canvasController.markDirty()
                                    annotationCanvas.requestPaint()
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
                            color: "#313244"

                            Rectangle {
                                width: angleSlider.visualPosition * parent.width
                                height: parent.height
                                color: "#89b4fa"
                                radius: 2
                            }
                        }

                        handle: Rectangle {
                            x: angleSlider.leftPadding + angleSlider.visualPosition * (angleSlider.availableWidth - width)
                            y: angleSlider.topPadding + angleSlider.availableHeight / 2 - height / 2
                            width: 14
                            height: 14
                            radius: 7
                            color: angleSlider.pressed ? "#b4befe" : "#89b4fa"
                        }
                    }

                    Label {
                        text: Math.round(angleSlider.value) + "°"
                        font.pixelSize: 11
                        color: "#89b4fa"
                        Layout.preferredWidth: 36
                    }
                }
            }

            // Status bar
            Rectangle {
                id: statusBar
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

                    Label {
                        text: shapeMode === 1 ? "OBB" : "HBB"
                        color: "#89b4fa"
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

    function hasSelectedAnnotation() {
        for (var i = 0; i < annotationModel.rowCount(); i++) {
            var idx = annotationModel.index(i, 0)
            if (annotationModel.data(idx, 264)) { // IsSelectedRole
                return true
            }
        }
        return false
    }

    function selectAnnotationAt(imgX, imgY) {
        var found = false
        for (var i = annotationModel.rowCount() - 1; i >= 0; i--) {
            var idx = annotationModel.index(i, 0)
            var cx = annotationModel.data(idx, 259)
            var cy = annotationModel.data(idx, 260)
            var w = annotationModel.data(idx, 261)
            var h = annotationModel.data(idx, 262)
            var annAngle = annotationModel.data(idx, 263) // AngleRole

            if (shapeMode === 1 && annAngle !== undefined && annAngle !== 0) {
                // OBB hit test: transform point to local coordinates
                var dx = imgX - cx
                var dy = imgY - cy
                var rad = -annAngle * Math.PI / 180.0
                var localX = dx * Math.cos(rad) - dy * Math.sin(rad)
                var localY = dx * Math.sin(rad) + dy * Math.cos(rad)
                var hw = w / 2
                var hh = h / 2

                if (localX >= -hw && localX <= hw && localY >= -hh && localY <= hh) {
                    annotationModel.setSelected(i, !annotationModel.data(idx, 264))

                    // Update angle slider to this annotation's angle
                    angleSlider.value = annAngle

                    found = true
                    break
                }
            } else {
                // HBB hit test
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
