// ImportPage.qml - V3 数据导入页面
// 支持分别指定图片/标签路径、自动格式探测、步骤卡片式交互
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme
import QtQuick.Dialogs

Item {
    id: root

    // 扫描结果状态
    property var scanResult: null
    property bool isScanning: false
    property string selectedImagePath: ""
    property string selectedLabelPath: ""
    // 导入模式: "auto" = 单目录自动探测, "separate" = 分别指定图片/标签路径
    property string importMode: "auto"

    // 未打开项目时的空状态提示
    ColumnLayout {
        anchors.centerIn: parent
        visible: appController.currentProjectId === ""
        spacing: 16

        Label {
            text: "📁 请先打开一个项目"
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeTitle
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            text: "在左侧项目中心创建或打开项目后，即可导入数据集"
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeNormal
            Layout.alignment: Qt.AlignHCenter
        }

        Button {
            text: "前往项目中心"
            font.family: Theme.fontFamily
            Layout.alignment: Qt.AlignHCenter
            background: Rectangle {
                color: parent.hovered ? Theme.accentPrimary : Theme.bgTertiary
                radius: Theme.radiusSmall
                border.color: Theme.accentPrimary
                border.width: 1
                implicitWidth: 140
                implicitHeight: 36
            }
            contentItem: Label {
                text: parent.text
                color: Theme.accentPrimary
                font.pixelSize: Theme.fontSizeNormal
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: appController.currentPageIndex = 0
        }
    }

    // 按当前项目过滤数据集列表
    Connections {
        target: appController
        function onCurrentProjectIdChanged() {
            datasetModel.setProjectId(appController.currentProjectId)
        }
    }

    // 异步扫描完成信号处理
    Connections {
        target: datasetService
        function onScanFolderFinished(result) {
            root.isScanning = false
            root.scanResult = result
            if (result && result.isValid) {
                datasetNameField.text = extractFolderName(root.selectedImagePath)
            }
        }
        function onScanSeparateFinished(result) {
            root.isScanning = false
            if (result && result.isValid) {
                root.scanResult = result
            } else if (result && result.error && result.error.length > 0) {
                root.scanResult = result
            } else {
                // 无标签或纯图片模式，尝试 scanFolderAsync
                datasetService.scanFolderAsync(root.selectedImagePath)
                return
            }
            if (root.scanResult && root.scanResult.isValid) {
                datasetNameField.text = extractFolderName(root.selectedImagePath)
            }
        }
    }

    Component.onCompleted: {
        datasetModel.setProjectId(appController.currentProjectId)
    }

    // 页面激活时刷新数据集列表（解决从其他页面切回时数据不显示的问题）
    onVisibleChanged: {
        if (visible && appController.currentProjectId !== "") {
            datasetModel.setProjectId(appController.currentProjectId)
        }
    }

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        visible: appController.currentProjectId !== ""

        ColumnLayout {
            width: Math.max(root.width, 600)
            anchors.margins: Theme.spacingXLarge
            spacing: Theme.spacingLarge

            // 标题栏
            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: "数据导入"
                    font.pixelSize: Theme.fontSizeDisplay
                    font.bold: true
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: "刷新"
                    font.family: Theme.fontFamily
                    onClicked: datasetModel.refresh()
                }
            }

            // 未打开项目提示
            Label {
                visible: !appController.projectOpen
                text: "请先打开一个项目再导入数据"
                font.pixelSize: Theme.fontSizeNormal
                font.family: Theme.fontFamily
                color: Theme.accentError
                Layout.fillWidth: true
            }

            // === 导入模式切换 ===
            Rectangle {
                visible: appController.projectOpen
                Layout.fillWidth: true
                implicitHeight: modeRow.implicitHeight + 24
                color: Theme.bgCard
                radius: Theme.radiusLarge

                RowLayout {
                    id: modeRow
                    anchors.fill: parent
                    anchors.margins: Theme.spacingNormal
                    spacing: Theme.spacingNormal

                    Label {
                        text: "导入模式："
                        font.pixelSize: Theme.fontSizeNormal
                        font.bold: true
                        font.family: Theme.fontFamily
                        color: Theme.textPrimary
                    }

                    Button {
                        id: autoBtn
                        text: "单目录自动探测"
                        font.family: Theme.fontFamily
                        font.bold: root.importMode === "auto"
                        palette.buttonText: root.importMode === "auto" ? "#FFFFFF" : (autoBtn.hovered ? Theme.textPrimary : Theme.textSecondary)
                        background: Rectangle {
                            color: {
                                if (root.importMode === "auto") return Theme.accentPrimary
                                return autoBtn.hovered ? Theme.bgHover : Theme.bgTertiary
                            }
                            radius: Theme.radiusSmall
                            border.color: root.importMode === "auto" ? Theme.accentPrimary : Theme.border
                            border.width: 1
                        }
                        onClicked: {
                            root.importMode = "auto"
                            root.scanResult = null
                        }
                    }

                    Button {
                        id: sepBtn
                        text: "分别指定图片和标签路径"
                        font.family: Theme.fontFamily
                        font.bold: root.importMode === "separate"
                        palette.buttonText: root.importMode === "separate" ? "#FFFFFF" : (sepBtn.hovered ? Theme.textPrimary : Theme.textSecondary)
                        background: Rectangle {
                            color: {
                                if (root.importMode === "separate") return Theme.accentPrimary
                                return sepBtn.hovered ? Theme.bgHover : Theme.bgTertiary
                            }
                            radius: Theme.radiusSmall
                            border.color: root.importMode === "separate" ? Theme.accentPrimary : Theme.border
                            border.width: 1
                        }
                        onClicked: {
                            root.importMode = "separate"
                            root.scanResult = null
                        }
                    }
                }
            }

            // === 步骤卡片区域 ===
            Rectangle {
                visible: appController.projectOpen
                Layout.fillWidth: true
                Layout.preferredHeight: stepContent.implicitHeight + 32
                color: Theme.bgCard
                radius: Theme.radiusLarge

                ColumnLayout {
                    id: stepContent
                    anchors.fill: parent
                    anchors.margins: Theme.spacingLarge
                    spacing: Theme.spacingLarge

                    // ====== 模式1: 单目录自动探测 ======
                    ColumnLayout {
                        visible: root.importMode === "auto"
                        Layout.fillWidth: true
                        spacing: Theme.spacingNormal

                        Label {
                            text: "步骤1：选择数据集根目录"
                            font.pixelSize: Theme.fontSizeSubheading
                            font.bold: true
                            font.family: Theme.fontFamily
                            color: Theme.textPrimary
                        }

                        // 拖拽区域
                        Rectangle {
                            id: dropArea
                            Layout.fillWidth: true
                            Layout.preferredHeight: 120
                            color: dropAreaDrag.containsDrag ? Theme.bgTertiary : Theme.bgInput
                            radius: Theme.radiusNormal
                            border.color: dropAreaDrag.containsDrag ? Theme.accentPrimary : Theme.borderNormal
                            border.width: 1

                            DropArea {
                                id: dropAreaDrag
                                anchors.fill: parent
                                onEntered: function(drag) {
                                    if (drag.hasUrls) drag.accepted = true
                                }
                                onDropped: function(drop) {
                                    if (drop.hasUrls) {
                                        folderPathField.text = urlToPath(drop.urls[0])
                                        root.selectedImagePath = folderPathField.text
                                        startScanAuto()
                                    }
                                }
                            }

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: Theme.spacingSmall

                                Label {
                                    text: "📁"
                                    font.pixelSize: 32
                                    Layout.alignment: Qt.AlignHCenter
                                }

                                Label {
                                    text: "点击或拖拽文件夹到此处"
                                    font.pixelSize: Theme.fontSizeNormal
                                    font.family: Theme.fontFamily
                                    color: Theme.textSecondary
                                    Layout.alignment: Qt.AlignHCenter
                                }

                                Label {
                                    text: "支持 YOLO TXT / COCO JSON / LabelMe JSON / Anomalib 异常检测格式"
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.family: Theme.fontFamily
                                    color: Theme.textMuted
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: folderDialog.open()
                            }
                        }

                        // 路径输入行
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingNormal

                            TextField {
                                id: folderPathField
                                Layout.fillWidth: true
                                placeholderText: "选择数据集根目录..."
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeNormal
                                background: Rectangle {
                                    color: Theme.bgInput
                                    radius: Theme.radiusSmall
                                    border.color: folderPathField.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                                    border.width: 1
                                }
                                onTextChanged: root.selectedImagePath = text
                            }

                            Button {
                                text: "浏览"
                                font.family: Theme.fontFamily
                                onClicked: folderDialog.open()
                            }

                            Button {
                                id: analyzeAutoBtn
                                text: "分析"
                                enabled: folderPathField.text.trim().length > 0 && !root.isScanning
                                font.family: Theme.fontFamily
                                font.bold: true
                                palette.buttonText: enabled ? "#FFFFFF" : Theme.textDisabled
                                background: Rectangle {
                                    color: analyzeAutoBtn.enabled ? (analyzeAutoBtn.hovered ? Qt.lighter(Theme.accentPrimary, 1.1) : Theme.accentPrimary) : Theme.bgTertiary
                                    radius: Theme.radiusSmall
                                    border.color: analyzeAutoBtn.enabled ? Theme.accentPrimary : Theme.border
                                    border.width: 1
                                }
                                onClicked: startScanAuto()
                            }
                        }
                    }

                    // ====== 模式2: 分别指定图片和标签路径 ======
                    ColumnLayout {
                        visible: root.importMode === "separate"
                        Layout.fillWidth: true
                        spacing: Theme.spacingNormal

                        Label {
                            text: "步骤1：分别指定图片和标签路径"
                            font.pixelSize: Theme.fontSizeSubheading
                            font.bold: true
                            font.family: Theme.fontFamily
                            color: Theme.textPrimary
                        }

                        Label {
                            text: "图片和标签可以在不同目录，系统会按文件名自动匹配。标签路径可留空（用于异常检测或待标注数据）。"
                            font.pixelSize: Theme.fontSizeCaption
                            font.family: Theme.fontFamily
                            color: Theme.textMuted
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }

                        // 图片路径
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingNormal

                            Label {
                                text: "图片目录："
                                font.pixelSize: Theme.fontSizeNormal
                                font.family: Theme.fontFamily
                                color: Theme.textPrimary
                                Layout.preferredWidth: 80
                            }

                            TextField {
                                id: imagePathField
                                Layout.fillWidth: true
                                placeholderText: "选择图片目录（jpg/png/bmp/pbm等）..."
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeNormal
                                background: Rectangle {
                                    color: Theme.bgInput
                                    radius: Theme.radiusSmall
                                    border.color: imagePathField.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                                    border.width: 1
                                }
                                onTextChanged: root.selectedImagePath = text
                            }

                            Button {
                                text: "浏览"
                                font.family: Theme.fontFamily
                                onClicked: imageFolderDialog.open()
                            }
                        }

                        // 标签路径
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingNormal

                            Label {
                                text: "标签目录："
                                font.pixelSize: Theme.fontSizeNormal
                                font.family: Theme.fontFamily
                                color: Theme.textSecondary
                                Layout.preferredWidth: 80
                            }

                            TextField {
                                id: labelPathField
                                Layout.fillWidth: true
                                placeholderText: "选择标签目录（txt/json），可留空..."
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeNormal
                                background: Rectangle {
                                    color: Theme.bgInput
                                    radius: Theme.radiusSmall
                                    border.color: labelPathField.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                                    border.width: 1
                                }
                                onTextChanged: root.selectedLabelPath = text
                            }

                            Button {
                                text: "浏览"
                                font.family: Theme.fontFamily
                                onClicked: labelFolderDialog.open()
                            }

                            Button {
                                text: "清空"
                                font.family: Theme.fontFamily
                                onClicked: {
                                    labelPathField.clear()
                                    root.selectedLabelPath = ""
                                }
                            }
                        }

                        // 分析按钮
                        RowLayout {
                            Layout.fillWidth: true

                            Item { Layout.fillWidth: true }

                            Button {
                                id: analyzeBtn
                                text: "分析匹配"
                                enabled: imagePathField.text.trim().length > 0 && !root.isScanning
                                font.family: Theme.fontFamily
                                font.bold: true
                                palette.buttonText: enabled ? "#FFFFFF" : Theme.textDisabled
                                background: Rectangle {
                                    color: analyzeBtn.enabled ? (analyzeBtn.hovered ? Qt.lighter(Theme.accentPrimary, 1.1) : Theme.accentPrimary) : Theme.bgTertiary
                                    radius: Theme.radiusSmall
                                    border.color: analyzeBtn.enabled ? Theme.accentPrimary : Theme.border
                                    border.width: 1
                                }
                                onClicked: startScanSeparate()
                            }
                        }
                    }

                    // 加载动画
                    BusyIndicator {
                        visible: root.isScanning
                        running: root.isScanning
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredHeight: 40
                        Layout.preferredWidth: 40
                    }

                    Label {
                        visible: root.isScanning
                        text: "正在分析目录文件..."
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily
                        color: Theme.textSecondary
                        Layout.alignment: Qt.AlignHCenter
                    }

                    // === 步骤2：扫描结果预览 ===
                    ColumnLayout {
                        visible: root.scanResult !== null && !root.isScanning
                        Layout.fillWidth: true
                        spacing: Theme.spacingNormal

                        Label {
                            text: "步骤2：扫描结果预览"
                            font.pixelSize: Theme.fontSizeSubheading
                            font.bold: true
                            font.family: Theme.fontFamily
                            color: Theme.textPrimary
                        }

                        // 格式徽章 + 匹配统计卡片
                        Rectangle {
                            Layout.fillWidth: true
                            color: Theme.bgInput
                            radius: Theme.radiusNormal
                            implicitHeight: statsRow.implicitHeight + 24

                            RowLayout {
                                id: statsRow
                                anchors.fill: parent
                                anchors.margins: Theme.spacingNormal
                                spacing: Theme.spacingXLarge

                                // 格式徽章
                                Rectangle {
                                    Layout.alignment: Qt.AlignVCenter
                                    Layout.preferredWidth: formatBadgeText.implicitWidth + 16
                                    Layout.preferredHeight: 28
                                    implicitWidth: formatBadgeText.implicitWidth + 16
                                    implicitHeight: 28
                                    radius: Theme.radiusSmall
                                    color: {
                                        var fmt = root.scanResult ? root.scanResult.detectedFormat : ""
                                        if (fmt === "yolo_txt") return Theme.accentPrimary
                                        if (fmt === "coco_json") return Theme.accentSecondary
                                        if (fmt === "labelme_json") return "#E67E22"
                                        if (fmt === "anomaly_unsupervised") return Theme.accentWarning
                                        if (fmt === "image_only") return Theme.textMuted
                                        return Theme.textMuted
                                    }

                                    Label {
                                        id: formatBadgeText
                                        anchors.centerIn: parent
                                        text: {
                                            var fmt = root.scanResult ? root.scanResult.detectedFormat : ""
                                            if (fmt === "yolo_txt") return "YOLO 目标检测 (TXT)"
                                            if (fmt === "coco_json") return "COCO 目标检测 (JSON)"
                                            if (fmt === "labelme_json") return "LabelMe 标注 (JSON)"
                                            if (fmt === "anomaly_unsupervised") return "Anomalib 异常检测"
                                            if (fmt === "image_only") return "纯图片（无标签）"
                                            return "未知格式"
                                        }
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.bold: true
                                        font.family: Theme.fontFamily
                                        color: "#FFFFFF"
                                    }
                                }

                                // 图片总数
                                ColumnLayout {
                                    spacing: 2
                                    Label {
                                        text: "图片总数"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: Theme.textMuted
                                    }
                                    Label {
                                        text: root.scanResult ? root.scanResult.imageCount : "0"
                                        font.pixelSize: Theme.fontSizeLarge
                                        font.bold: true
                                        font.family: Theme.fontFamily
                                        color: Theme.textPrimary
                                    }
                                }

                                // 已标注
                                ColumnLayout {
                                    visible: root.scanResult && root.scanResult.detectedFormat !== "anomaly_unsupervised" && root.scanResult.detectedFormat !== "image_only"
                                    spacing: 2
                                    Label {
                                        text: "已标注"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: Theme.textMuted
                                    }
                                    Label {
                                        text: root.scanResult ? (root.scanResult.labelCount || 0) : "0"
                                        font.pixelSize: Theme.fontSizeLarge
                                        font.bold: true
                                        font.family: Theme.fontFamily
                                        color: Theme.accentSuccess
                                    }
                                }

                                // 未标注
                                ColumnLayout {
                                    visible: root.scanResult && root.scanResult.detectedFormat !== "anomaly_unsupervised" && root.scanResult.detectedFormat !== "image_only"
                                    spacing: 2
                                    Label {
                                        text: "未标注"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: Theme.textMuted
                                    }
                                    Label {
                                        text: root.scanResult ? (root.scanResult.unmatchedImagesCount || 0) : "0"
                                        font.pixelSize: Theme.fontSizeLarge
                                        font.bold: true
                                        font.family: Theme.fontFamily
                                        color: root.scanResult && root.scanResult.unmatchedImagesCount > 0 ? Theme.accentWarning : Theme.textPrimary
                                    }
                                }

                                // 异常检测统计
                                ColumnLayout {
                                    visible: root.scanResult && root.scanResult.detectedFormat === "anomaly_unsupervised"
                                    spacing: 2
                                    Label {
                                        text: "正常训练集"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: Theme.textMuted
                                    }
                                    Label {
                                        text: root.scanResult && root.scanResult.layoutStats ? root.scanResult.layoutStats.trainGood : "0"
                                        font.pixelSize: Theme.fontSizeLarge
                                        font.bold: true
                                        font.family: Theme.fontFamily
                                        color: Theme.accentSuccess
                                    }
                                }

                                ColumnLayout {
                                    visible: root.scanResult && root.scanResult.detectedFormat === "anomaly_unsupervised" && root.scanResult.layoutStats && root.scanResult.layoutStats.testDefective > 0
                                    spacing: 2
                                    Label {
                                        text: "异常测试集"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: Theme.textMuted
                                    }
                                    Label {
                                        text: root.scanResult && root.scanResult.layoutStats ? root.scanResult.layoutStats.testDefective : "0"
                                        font.pixelSize: Theme.fontSizeLarge
                                        font.bold: true
                                        font.family: Theme.fontFamily
                                        color: Theme.accentError
                                    }
                                }
                            }
                        }

                        // 类别列表（目标检测格式）
                        Rectangle {
                            visible: root.scanResult && root.scanResult.detectedFormat !== "anomaly_unsupervised" && root.scanResult.detectedFormat !== "image_only" && root.scanResult.classIds && root.scanResult.classIds.length > 0
                            Layout.fillWidth: true
                            implicitHeight: classFlow.implicitHeight + 24
                            color: Theme.bgInput
                            radius: Theme.radiusNormal

                            ColumnLayout {
                                id: classFlow
                                anchors.fill: parent
                                anchors.margins: Theme.spacingNormal
                                spacing: Theme.spacingSmall

                                Label {
                                    text: "类别探测 (" + (root.scanResult && root.scanResult.classIds ? root.scanResult.classIds.length : 0) + "类)"
                                    font.pixelSize: Theme.fontSizeNormal
                                    font.bold: true
                                    font.family: Theme.fontFamily
                                    color: Theme.textPrimary
                                }

                                Flow {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSmall

                                    Repeater {
                                        model: root.scanResult && root.scanResult.classIds ? root.scanResult.classIds : []

                                        Rectangle {
                                            width: classTagText.implicitWidth + 16
                                            height: 24
                                            radius: Theme.radiusSmall
                                            color: Theme.classColor(index)

                                            Label {
                                                id: classTagText
                                                anchors.centerIn: parent
                                                text: {
                                                    var classes = root.scanResult ? root.scanResult.classes : {}
                                                    var name = classes[modelData] || ("class_" + modelData)
                                                    return modelData + ": " + name
                                                }
                                                font.pixelSize: Theme.fontSizeCaption
                                                font.family: Theme.fontFamily
                                                color: "#FFFFFF"
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // 错误提示
                        Rectangle {
                            visible: root.scanResult && root.scanResult.error && root.scanResult.error.length > 0
                            Layout.fillWidth: true
                            height: 48
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: Theme.accentError
                            border.width: 1

                            Label {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingNormal
                                text: root.scanResult ? root.scanResult.error : ""
                                font.pixelSize: Theme.fontSizeNormal
                                font.family: Theme.fontFamily
                                color: Theme.accentError
                                wrapMode: Text.WordWrap
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        // 数据集名称 + 确认导入
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingNormal

                            Label {
                                text: "数据集名称"
                                font.pixelSize: Theme.fontSizeNormal
                                font.family: Theme.fontFamily
                                color: Theme.textPrimary
                            }

                            TextField {
                                id: datasetNameField
                                Layout.fillWidth: true
                                placeholderText: "输入数据集名称"
                                color: Theme.textPrimary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeNormal
                                text: root.scanResult ? extractFolderName(root.selectedImagePath) : ""
                                background: Rectangle {
                                    color: Theme.bgInput
                                    radius: Theme.radiusSmall
                                    border.color: datasetNameField.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                                    border.width: 1
                                }
                            }

                            Button {
                                id: confirmImportBtn
                                text: "确认导入"
                                enabled: root.scanResult
                                         && root.scanResult.isValid
                                         && datasetNameField.text.trim().length > 0
                                         && !root.isScanning
                                font.family: Theme.fontFamily
                                font.bold: true
                                palette.buttonText: enabled ? "#FFFFFF" : Theme.textDisabled
                                background: Rectangle {
                                    color: confirmImportBtn.enabled ? (confirmImportBtn.hovered ? Qt.lighter(Theme.accentPrimary, 1.1) : Theme.accentPrimary) : Theme.bgTertiary
                                    radius: Theme.radiusSmall
                                    border.color: confirmImportBtn.enabled ? Theme.accentPrimary : Theme.border
                                    border.width: 1
                                }
                                onClicked: {
                                    var dsId = ""
                                    var format = root.scanResult ? root.scanResult.detectedFormat : ""
                                    var count = root.scanResult ? (root.scanResult.imageCount || 0) : 0
                                    var name = datasetNameField.text.trim()

                                    if (root.importMode === "separate") {
                                        dsId = datasetService.importDatasetSeparate(
                                            appController.currentProjectId,
                                            name,
                                            root.selectedImagePath,
                                            root.selectedLabelPath
                                        )
                                    } else {
                                        dsId = datasetService.importDatasetV2(
                                            appController.currentProjectId,
                                            name,
                                            root.selectedImagePath,
                                            format,
                                            root.scanResult.labelDirOrPath ? root.scanResult.labelDirOrPath : "",
                                            true
                                        )
                                    }
                                    if (dsId && dsId.length > 0) {
                                        datasetModel.refresh()
                                        
                                        var formatStr = "未知格式"
                                        if (format === "yolo_txt") formatStr = "YOLO TXT"
                                        else if (format === "coco_json") formatStr = "COCO JSON"
                                        else if (format === "labelme_json") formatStr = "LabelMe JSON"
                                        else if (format === "anomaly_unsupervised") formatStr = "无监督异常检测"
                                        else if (format === "image_only") formatStr = "纯图片"

                                        importSuccessMsg.text = "数据集: " + name + "\n格式: " + formatStr + "\n样本数量: " + count + " 张图片"
                                        importSuccessDialog.open()

                                        root.scanResult = null
                                        folderPathField.clear()
                                        imagePathField.clear()
                                        labelPathField.clear()
                                        datasetNameField.clear()
                                        root.selectedImagePath = ""
                                        root.selectedLabelPath = ""
                                    } else {
                                        // 导入失败，显示错误提示
                                        importErrorLabel.visible = true
                                        importErrorLabel.text = "导入失败，请检查路径和格式是否正确"
                                    }
                                }
                            }

                            Label {
                                id: importErrorLabel
                                visible: false
                                color: Theme.accentError
                                font.pixelSize: Theme.fontSizeCaption
                                font.family: Theme.fontFamily
                            }
                        }
                    }
                }
            }

            // 已导入数据集列表
            Label {
                visible: appController.projectOpen
                text: "已导入数据集"
                font.pixelSize: Theme.fontSizeLarge
                font.bold: true
                font.family: Theme.fontFamily
                color: Theme.textPrimary
            }

            ColumnLayout {
                visible: appController.projectOpen
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                Repeater {
                    model: datasetModel
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        height: 72
                        color: index % 2 === 0 ? Theme.bgCard : Theme.bgInput
                        radius: Theme.radiusNormal

                        property bool hovered: mouseArea.containsMouse

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingNormal
                            spacing: Theme.spacingNormal

                            Column {
                                Layout.fillWidth: true
                                Label {
                                    text: model.name
                                    font.bold: true
                                    font.pixelSize: Theme.fontSizeNormal
                                    font.family: Theme.fontFamily
                                    color: Theme.textPrimary
                                }
                                Label {
                                    text: model.imageRoot
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.family: Theme.fontFamily
                                    color: Theme.textMuted
                                    elide: Text.ElideMiddle
                                    width: parent.width
                                }
                            }

                            // 格式徽章
                            Rectangle {
                                visible: model.format && model.format.length > 0
                                width: formatText.implicitWidth + 12
                                height: 20
                                radius: Theme.radiusSmall
                                color: {
                                    if (model.format === "yolo_txt") return Theme.accentPrimary
                                    if (model.format === "coco_json") return Theme.accentSecondary
                                    if (model.format === "labelme_json") return "#E67E22"
                                    if (model.format === "anomaly_unsupervised") return Theme.accentWarning
                                    if (model.format === "image_only") return Theme.textMuted
                                    return Theme.textMuted
                                }

                                Label {
                                    id: formatText
                                    anchors.centerIn: parent
                                    text: {
                                        if (model.format === "yolo_txt") return "YOLO TXT"
                                        if (model.format === "coco_json") return "COCO JSON"
                                        if (model.format === "labelme_json") return "LabelMe"
                                        if (model.format === "anomaly_unsupervised") return "异常检测"
                                        if (model.format === "image_only") return "纯图片"
                                        return model.format || ""
                                    }
                                    font.pixelSize: 10
                                    font.family: Theme.fontFamily
                                    color: "#FFFFFF"
                                }
                            }

                            Rectangle {
                                width: 12
                                height: 12
                                radius: 6
                                color: model.importStatus === "completed" ? Theme.accentSuccess :
                                       model.importStatus === "failed" ? Theme.accentError : Theme.accentWarning
                            }

                            Label {
                                text: model.sampleCount + " 张"
                                font.pixelSize: Theme.fontSizeCaption
                                font.family: Theme.fontFamily
                                color: Theme.textSecondary
                            }

                            Label {
                                text: {
                                    if (model.importStatus === "completed") return "已完成"
                                    if (model.importStatus === "failed") return "失败"
                                    if (model.importStatus === "scanning") return "扫描中"
                                    if (model.importStatus === "importing") return "导入中"
                                    return model.importStatus
                                }
                                font.pixelSize: Theme.fontSizeCaption
                                font.family: Theme.fontFamily
                                color: Theme.textMuted
                            }

                            Button {
                                text: "删除"
                                font.family: Theme.fontFamily
                                onClicked: {
                                    datasetService.deleteDataset(model.datasetId)
                                    datasetModel.refresh()
                                }
                            }
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }
            }
        }
    }

    // URL 转本地路径工具函数（兼容中文路径和特殊字符）
    // Windows: file:///C:/Users/... → C:/Users/...
    // Linux: file:///home/... → /home/...
    function urlToPath(url) {
        var s = url.toString()
        if (s.startsWith("file:///")) {
            s = s.substring(7)
            // Windows 路径: /C:/... → C:/...
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

    // 文件夹选择对话框（单目录模式）
    FolderDialog {
        id: folderDialog
        onAccepted: {
            folderPathField.text = urlToPath(selectedFolder)
            root.selectedImagePath = folderPathField.text
            startScanAuto()
        }
    }

    // 图片目录选择对话框（分别路径模式）
    FolderDialog {
        id: imageFolderDialog
        onAccepted: {
            imagePathField.text = urlToPath(selectedFolder)
            root.selectedImagePath = imagePathField.text
        }
    }

    // 标签目录选择对话框（分别路径模式）
    FolderDialog {
        id: labelFolderDialog
        onAccepted: {
            labelPathField.text = urlToPath(selectedFolder)
            root.selectedLabelPath = labelPathField.text
        }
    }

    // 单目录自动探测扫描
    function startScanAuto() {
        if (!folderPathField.text.trim()) return
        root.isScanning = true
        root.scanResult = null
        root.selectedImagePath = folderPathField.text.trim()
        datasetService.scanFolderAsync(root.selectedImagePath)
    }

    // 分别路径扫描
    function startScanSeparate() {
        if (!imagePathField.text.trim()) return
        root.isScanning = true
        root.scanResult = null
        root.selectedImagePath = imagePathField.text.trim()
        root.selectedLabelPath = labelPathField.text.trim()
        datasetService.scanSeparateAsync(root.selectedImagePath, root.selectedLabelPath)
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

    // 导入成功提示对话框
    Dialog {
        id: importSuccessDialog
        title: "导入成功"
        modal: true
        anchors.centerIn: parent
        width: 320
        standardButtons: Dialog.Ok

        background: Rectangle {
            color: Theme.bgSecondary
            border.color: Theme.border
            border.width: 1
            radius: Theme.radiusLarge
        }

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingLarge

            Label {
                text: "🎉 数据集导入成功"
                font.bold: true
                color: Theme.accentSuccess
                font.pixelSize: Theme.fontSizeSubheading
                font.family: Theme.fontFamily
            }

            Label {
                id: importSuccessMsg
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                font.family: Theme.fontFamily
            }
        }
    }
}
