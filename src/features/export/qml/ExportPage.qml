// ExportPage.qml - 导出中心
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme
import QtQuick.Layouts

Item {
    id: root

    property string currentProjectId: appController.currentProjectId
    property string selectedVersionId: ""
    property var exportHistory: []

    onCurrentProjectIdChanged: {
        modelVersionModel.setProjectId(currentProjectId)
        selectedVersionId = ""
        exportHistory = []
        refreshExports()
    }

    function refreshExports() {
        if (selectedVersionId !== "") {
            exportHistory = exportService.listExports(selectedVersionId)
        } else {
            exportHistory = []
        }
    }

    // 监听导出状态变更，自动刷新列表并更新状态提示
    Connections {
        target: exportService
        function onExportStatusChanged(artifactId, status) {
            refreshExports()
            if (status === "verifying") {
                statusLabel.text = "导出完成，正在验证产物:" + artifactId.substring(0, 8) + "..."
                statusLabel.color = Theme.accentWarning
                validationResultBox.visible = false
            } else if (status === "succeeded") {
                statusLabel.text = "导出并验证完成:" + artifactId.substring(0, 8) + "..."
                statusLabel.color = Theme.accentSuccess
                // 获取验证结果并显示
                var details = exportService.getExportStatus(artifactId)
                if (details.validationResult && details.validationResult.length > 0) {
                    try {
                        var vr = JSON.parse(details.validationResult)
                        validationResultBox.isVerified = vr.verified !== false
                        validationResultBox.resultText = details.validationResult
                        validationResultBox.visible = true
                    } catch (e) {
                        validationResultBox.resultText = details.validationResult
                        validationResultBox.isVerified = true
                        validationResultBox.visible = true
                    }
                }
            } else if (status === "failed") {
                statusLabel.text = "导出或验证失败:" + artifactId.substring(0, 8) + "..."
                statusLabel.color = Theme.accentError
                var details2 = exportService.getExportStatus(artifactId)
                if (details2.validationResult && details2.validationResult.length > 0) {
                    try {
                        var vr2 = JSON.parse(details2.validationResult)
                        validationResultBox.isVerified = false
                        validationResultBox.resultText = vr2.error || details2.validationResult
                        validationResultBox.visible = true
                    } catch (e2) {
                        validationResultBox.resultText = details2.validationResult
                        validationResultBox.isVerified = false
                        validationResultBox.visible = true
                    }
                }
            } else if (status === "running") {
                statusLabel.text = "正在导出:" + artifactId.substring(0, 8) + "..."
                statusLabel.color = Theme.accentWarning
                validationResultBox.visible = false
            }
        }
    }

    // 未打开项目时的空状态提示
    ColumnLayout {
        anchors.centerIn: parent
        visible: currentProjectId === ""
        spacing: 16

        Label {
            text: "📦 请先打开一个项目"
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeTitle
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            text: "完成训练后，在此导出模型"
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

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12
        visible: currentProjectId !== ""

        // 左侧面板：导出配置
        Rectangle {
            Layout.preferredWidth: 400
            Layout.fillHeight: true
            color: Theme.bgCard
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // 区域标题
                Label {
                    text: "导出模型"
                    color: Theme.accentPrimary
                    font.pixelSize: 16
                    font.bold: true
                }

                    // 模型版本选择器
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "模型版本:"
                        color: Theme.textPrimary
                        font.pixelSize: 13
                        Layout.preferredWidth: 100
                    }

                    ComboBox {
                        id: versionCombo
                        Layout.fillWidth: true
                        model: modelVersionModel
                        textRole: "versionId"
                        valueRole: "versionId"
                        displayText: currentIndex >= 0 ?
                            modelVersionModel.data(modelVersionModel.index(currentIndex, 0), Qt.UserRole + 1) ?
                            modelVersionModel.data(modelVersionModel.index(currentIndex, 0), Qt.UserRole + 1).substring(0, 8) + "..." :
                            "选择版本" : "选择版本"

                        contentItem: Label {
                            text: versionCombo.displayText
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: 4
                            border.color: versionCombo.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                            border.width: 1
                        }

                        popup: Popup {
                            y: versionCombo.height
                            width: versionCombo.width
                            implicitHeight: Math.min(contentItem.implicitHeight, 300)
                            padding: 1

                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: versionCombo.popup.visible ? versionCombo.delegateModel : null
                                currentIndex: versionCombo.highlightedIndex
                            }

                            background: Rectangle {
                                color: Theme.bgPrimary
                                border.color: Theme.borderNormal
                                radius: 4
                            }
                        }

                        delegate: ItemDelegate {
                            width: versionCombo.width
                            contentItem: Label {
                                text: model.versionId.substring(0, 8) + "..." + (model.bestWeightPath ? " (" + model.bestWeightPath + ")" : "")
                                color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                font.pixelSize: 12
                                font.family: "monospace"
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            highlighted: versionCombo.highlightedIndex === index
                            background: Rectangle {
                                color: highlighted ? Theme.bgInput : Theme.bgPrimary
                            }
                        }

                        onActivated: {
                            selectedVersionId = currentValue
                            refreshExports()
                        }
                    }
                }

                    // 格式选择器
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "导出格式:"
                        color: Theme.textPrimary
                        font.pixelSize: 13
                        Layout.preferredWidth: 100
                    }

                    ComboBox {
                        id: formatCombo
                        Layout.fillWidth: true
                        model: ["pt", "onnx", "tflite", "engine"]
                        currentIndex: 1

                        contentItem: Label {
                            text: formatCombo.displayText
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: 4
                            border.color: formatCombo.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                            border.width: 1
                        }

                        popup: Popup {
                            y: formatCombo.height
                            width: formatCombo.width
                            implicitHeight: Math.min(contentItem.implicitHeight, 200)
                            padding: 1

                            contentItem: ListView {
                                clip: true
                                implicitHeight: contentHeight
                                model: formatCombo.popup.visible ? formatCombo.delegateModel : null
                                currentIndex: formatCombo.highlightedIndex
                            }

                            background: Rectangle {
                                color: Theme.bgPrimary
                                border.color: Theme.borderNormal
                                radius: 4
                            }
                        }

                        delegate: ItemDelegate {
                            width: formatCombo.width
                            contentItem: Label {
                                text: modelData.toUpperCase()
                                color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                font.pixelSize: 13
                                font.bold: true
                                verticalAlignment: Text.AlignVCenter
                            }
                            highlighted: formatCombo.highlightedIndex === index
                            background: Rectangle {
                                color: highlighted ? Theme.bgInput : Theme.bgPrimary
                            }
                        }
                    }
                }

                // ONNX 配置面板（仅格式为 onnx 时可见）
                OnnxConfigPanel {
                    id: onnxConfigPanel
                    Layout.fillWidth: true
                    visible: formatCombo.currentText === "onnx"
                }

                // 非 ONNX 格式的提示信息
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    visible: formatCombo.currentText !== "onnx"
                    color: Theme.bgInput
                    radius: 6

                    Label {
                        anchors.centerIn: parent
                        text: {
                            switch (formatCombo.currentText) {
                            case "pt": return "PyTorch .pt 格式 - 无额外选项"
                            case "tflite": return "TensorFlow Lite 格式 - 无额外选项"
                            case "engine": return "TensorRT Engine 格式 - 无额外选项"
                            default: return ""
                            }
                        }
                        color: Theme.textMuted
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Item { Layout.fillHeight: true }

                // 导出按钮
                Button {
                    id: exportBtn
                    text: "开始导出"
                    highlighted: true
                    enabled: versionCombo.currentIndex >= 0 && selectedVersionId !== ""
                    Layout.fillWidth: true

                    background: Rectangle {
                        color: parent.enabled ? (parent.pressed ? Qt.darker(Theme.accentSuccess, 1.2) : Theme.accentSuccess) : Theme.borderNormal
                        radius: 6
                        implicitHeight: 40
                    }

                    contentItem: Label {
                        text: parent.text
                        color: parent.enabled ? Theme.bgPrimary : Theme.textMuted
                        font.pixelSize: 14
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        if (selectedVersionId === "") return
                        var format = formatCombo.currentText
                        var optionsJson = "{}"
                        if (format === "onnx") {
                            optionsJson = onnxConfigPanel.getConfigJson()
                        }
                        var artifactId = exportService.exportModel(selectedVersionId, format, optionsJson)
                        if (artifactId !== "") {
                            statusLabel.text = "导出已启动:" + artifactId.substring(0, 8) + "..."
                            statusLabel.color = Theme.accentSuccess
                            refreshExports()
                        } else {
                            statusLabel.text = "导出启动失败"
                            statusLabel.color = Theme.accentError
                        }
                    }
                }

                // 状态标签 - 显示导出/验证进度
                Label {
                    id: statusLabel
                    Layout.fillWidth: true
                    text: ""
                    color: Theme.accentSuccess
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                // 验证结果详情
                Rectangle {
                    id: validationResultBox
                    Layout.fillWidth: true
                    visible: false
                    height: validationResultContent.implicitHeight + 16
                    color: Theme.bgInput
                    radius: Theme.radiusSmall

                    property string resultText: ""
                    property bool isVerified: false

                    Column {
                        id: validationResultContent
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        Label {
                            text: validationResultBox.isVerified ? "✅ 验证通过" : "❌ 验证失败"
                            color: validationResultBox.isVerified ? Theme.accentSuccess : Theme.accentError
                            font.pixelSize: Theme.fontSizeNormal
                            font.bold: true
                        }

                        Label {
                            text: validationResultBox.resultText
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeSmall
                            font.family: Theme.fontFamilyMono
                            wrapMode: Text.WordWrap
                            width: parent.width
                        }
                    }
                }
            }
        }

        // 右侧面板：导出历史
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgCard
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // 标题栏
                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: "导出历史"
                        color: Theme.accentPrimary
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: exportHistory.length + " 个产物"
                        color: Theme.textMuted
                        font.pixelSize: 12
                    }

                    Button {
                        text: "刷新"
                        flat: true
                        palette.buttonText: Theme.accentPrimary
                        font.pixelSize: 12
                        onClicked: refreshExports()
                    }
                }

                // 版本信息标签
                Label {
                    Layout.fillWidth: true
                    visible: selectedVersionId !== ""
                    text: "当前版本的导出记录:" + selectedVersionId.substring(0, 8) + "..."
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    font.family: "monospace"
                }

                // 导出列表
                ListView {
                    id: exportList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: exportHistory
                    spacing: 4

                    Label {
                        anchors.centerIn: parent
                        visible: exportList.count === 0
                        text: selectedVersionId === "" ?
                            "选择一个模型版本查看导出记录" :
                            "该版本暂无导出记录"
                        color: Theme.textMuted
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                    }

                    delegate: Rectangle {
                        width: exportList.width
                        height: 72
                        radius: 6
                        color: delegateMouseArea.containsMouse ? Theme.bgInput : Theme.bgSecondary

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 12

                            // 状态指示圆点
                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                color: {
                                    switch (modelData.status) {
                                    case "pending": return Theme.accentPrimary
                                    case "running": return Theme.accentWarning
                                    case "verifying": return Theme.accentWarning
                                    case "succeeded": return Theme.accentSuccess
                                    case "failed": return Theme.accentError
                                    default: return Theme.textMuted
                                    }
                                }
                            }

                            // 产物信息
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Label {
                                        text: modelData.id.substring(0, 8) + "..."
                                        color: Theme.accentPrimary
                                        font.pixelSize: 13
                                        font.family: "monospace"
                                    }

                                    // 格式标签
                                    Rectangle {
                                        Layout.preferredHeight: 20
                                        Layout.preferredWidth: formatBadgeText.implicitWidth + 12
                                        radius: 4
                                        color: Theme.accentPrimary
                                        border.color: Theme.accentPrimary
                                        border.width: 1

                                        Label {
                                            id: formatBadgeText
                                            anchors.centerIn: parent
                                            text: modelData.format ? modelData.format.toUpperCase() : ""
                                            color: Theme.accentPrimary
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    // 状态标签
                                    Rectangle {
                                        Layout.preferredHeight: 20
                                        Layout.preferredWidth: statusBadgeText.implicitWidth + 12
                                        radius: 4
                                        color: {
                                            switch (modelData.status) {
                                            case "pending": return Theme.accentPrimary
                                            case "running": return Theme.accentWarning
                                            case "verifying": return Theme.accentWarning
                                            case "succeeded": return Theme.accentSuccess
                                            case "failed": return Theme.accentError
                                            default: return Theme.textMuted
                                            }
                                        }
                                        border.color: {
                                            switch (modelData.status) {
                                            case "pending": return Theme.accentPrimary
                                            case "running": return Theme.accentWarning
                                            case "verifying": return Theme.accentWarning
                                            case "succeeded": return Theme.accentSuccess
                                            case "failed": return Theme.accentError
                                            default: return Theme.borderNormal
                                            }
                                        }
                                        border.width: 1

                                        Label {
                                            id: statusBadgeText
                                            anchors.centerIn: parent
                                            text: modelData.status || "pending"
                                            color: {
                                                switch (modelData.status) {
                                                case "pending": return Theme.accentPrimary
                                                case "running": return Theme.accentWarning
                                                case "verifying": return Theme.accentWarning
                                                case "succeeded": return Theme.accentSuccess
                                                case "failed": return Theme.accentError
                                                default: return Theme.textMuted
                                                }
                                            }
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    Item { Layout.fillWidth: true }
                                }

                                // 输出路径
                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.outputPath || "N/A"
                                    color: Theme.textSecondary
                                    font.pixelSize: 11
                                    font.family: "monospace"
                                    elide: Text.ElideMiddle
                                }

                                // 验证结果摘要
                                Label {
                                    Layout.fillWidth: true
                                    visible: modelData.validationResult && modelData.validationResult.length > 0
                                    text: {
                                        if (!modelData.validationResult || modelData.validationResult.length === 0) return ""
                                        try {
                                            var vr = JSON.parse(modelData.validationResult)
                                            if (vr.verified === false && vr.error) return "验证错误: " + vr.error
                                            if (vr.verified !== undefined) return "验证结果: " + (vr.verified ? "通过" : "未通过")
                                            return "验证信息: " + modelData.validationResult.substring(0, 60)
                                        } catch (e) {
                                            return "验证信息: " + modelData.validationResult.substring(0, 60)
                                        }
                                    }
                                    color: modelData.status === "succeeded" ? Theme.accentSuccess : Theme.accentError
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }

                                // 时间戳
                                Label {
                                    text: modelData.createdAt || "N/A"
                                    color: Theme.textMuted
                                    font.pixelSize: 10
                                }
                            }

                            // 验证/重试按钮
                            Button {
                                text: {
                                    if (modelData.status === "failed") return "重试验证"
                                    if (modelData.status === "succeeded") return "重新验证"
                                    return "验证"
                                }
                                visible: modelData.status === "succeeded" || modelData.status === "failed"
                                flat: true
                                Layout.preferredWidth: 72

                                background: Rectangle {
                                    color: parent.pressed ? Qt.darker(Theme.accentSuccess, 1.2) : Theme.accentSuccess
                                    radius: 4
                                    border.color: modelData.status === "failed" ? Theme.accentError : Theme.accentSuccess
                                    border.width: 1
                                    implicitHeight: 28
                                }

                                contentItem: Label {
                                    text: parent.text
                                    color: modelData.status === "failed" ? Theme.accentError : Theme.accentSuccess
                                    font.pixelSize: 11
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: {
                                    if (exportService.verifyExport(modelData.id)) {
                                        statusLabel.text = "验证已启动:" + modelData.id.substring(0, 8) + "..."
                                        statusLabel.color = Theme.accentWarning
                                        validationResultBox.visible = false
                                        refreshExports()
                                    } else {
                                        statusLabel.text = "验证启动失败"
                                        statusLabel.color = Theme.accentError
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: delegateMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }
                }
            }
        }
    }
}
