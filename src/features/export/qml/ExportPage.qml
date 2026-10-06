// ExportPage.qml - V5 导出页：模型导出与产物验证
// 像素级对标参考UI：左侧模型列表sidebar(240px) + 中心左右分栏(参数面板280px + 展示区)
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LabelTorch.Theme
import LabelTorch.Components

Item {
    id: root
    anchors.fill: parent

    property string currentProjectId: appController.currentProjectId
    property string selectedVersionId: ""
    property var exportHistory: []
    property string exportStatus: "idle"
    property string exportActionMessage: ""
    property string exportActionTone: "neutral"
    property string currentTaskType: currentProjectId !== "" ? projectService.getTaskType(currentProjectId) : "detect"
    property bool isAnomalyProject: currentTaskType === "anomaly"
    property string selectedArtifactId: ""
    property var selectedArtifactDetails: null
    // 空对象兜底：产物详情异步返回前为 null，读取统一走安全别名
    readonly property var artifactSafe: selectedArtifactDetails || {}
    property var parsedValidationDetails: {
        if (!artifactSafe.validationResult)
            return ({})
        try {
            return JSON.parse(artifactSafe.validationResult)
        } catch (error) {
            return ({ "rawText": artifactSafe.validationResult })
        }
    }

    function formatShape(shapeData) {
        if (!shapeData || shapeData.length === 0)
            return "未提供"
        return "[" + shapeData.map(function(item) {
            return item === null || item === undefined ? "?" : item
        }).join(", ") + "]"
    }

    function validationStatusText(details) {
        if (!details || Object.keys(details).length === 0)
            return "待验证"
        if (details.valid === true)
            return "验证通过"
        if (details.verified === false)
            return "验证失败"
        if (details.error)
            return "验证失败"
        return "已记录结果"
    }

    function validationStatusColor(details) {
        if (!details || Object.keys(details).length === 0)
            return Theme.textMuted
        if (details.valid === true)
            return Theme.success
        if (details.verified === false || details.error)
            return Theme.danger
        return Theme.warning
    }

    // 切换项目时重置状态
    onCurrentProjectIdChanged: {
        if (currentProjectId !== "") {
            currentTaskType = projectService.getTaskType(currentProjectId)
            modelVersionModel.setProjectId(currentProjectId)
        }
        selectedVersionId = ""
        exportHistory = []
    }

    // 页面可见时刷新数据
    onVisibleChanged: {
        if (visible && currentProjectId !== "") {
            modelVersionModel.setProjectId(currentProjectId)
            refreshExports()
        }
    }

    // 刷新导出历史列表
    function refreshExports() {
        if (selectedVersionId !== "") {
            exportHistory = exportService.listExports(selectedVersionId)
            if (selectedArtifactId !== "") {
                selectedArtifactDetails = exportService.getExportStatus(selectedArtifactId)
            }
        } else {
            exportHistory = []
            selectedArtifactId = ""
            selectedArtifactDetails = ({})
        }
    }

    // === 表单内联校验状态 ===
    property var fieldErrors: ({})

    function setFieldError(fieldName, message) {
        var errs = Object.assign({}, fieldErrors)
        if (message)
            errs[fieldName] = message
        else
            delete errs[fieldName]
        fieldErrors = errs
    }

    function clearFieldErrors() {
        fieldErrors = {}
    }

    function fieldError(fieldName) {
        return fieldErrors[fieldName] || ""
    }

    function validateExportStart() {
        clearFieldErrors()
        if (!root.selectedVersionId) {
            setFieldError("version", "请先选择一个模型版本")
            return {"ok": false, "message": "请先选择一个模型版本"}
        }
        if (formatCombo.currentText === "onnx" && opsetStepper.value < 11) {
            setFieldError("opset", "ONNX opset 版本不能低于 11")
            return {"ok": false, "message": "ONNX opset 版本不能低于 11"}
        }
        if (formatCombo.currentText === "onnx" && outputPathField.text.trim() === "") {
            setFieldError("outputPath", "请填写导出输出路径")
            return {"ok": false, "message": "请填写导出输出路径"}
        }
        if (formatCombo.currentText === "engine" && !(environmentInfo && environmentInfo.tensorrt_available)) {
            setFieldError("format", "导出 TensorRT 需要当前环境支持 TensorRT")
            return {"ok": false, "message": "导出 TensorRT 需要当前环境支持 TensorRT"}
        }
        if (formatCombo.currentText === "tflite" && currentTaskType !== "detect") {
            setFieldError("format", "TFLite 导出仅支持检测任务")
            return {"ok": false, "message": "TFLite 导出仅支持检测任务"}
        }
        return {"ok": true, "message": ""}
    }

    function startExportWithValidation() {
        var validation = validateExportStart()
        if (!validation.ok) {
            exportActionMessage = validation.message
            exportActionTone = "warning"
            ToastBus.error(validation.message)
            return
        }
        var format = formatCombo.currentText
        var optionsJson = "{}"
        if (format === "onnx") {
            optionsJson = JSON.stringify({
                "opset": opsetStepper.value,
                "simplify": simplifySwitch.checked,
                "dynamic": dynamicSwitch.checked
            })
        }
        var artifactId = exportService.exportModel(root.selectedVersionId, format, optionsJson)
        if (artifactId !== "") {
            exportStatus = "running"
            exportActionMessage = "导出任务已启动"
            exportActionTone = "info"
            ToastBus.success("导出任务已启动（" + format + "）")
            refreshExports()
        } else {
            exportActionMessage = "导出任务启动失败"
            exportActionTone = "danger"
            ToastBus.error("导出任务启动失败")
        }
    }

    // 监听导出状态变更信号
    Connections {
        target: exportService
        function onExportStatusChanged(artifactId, status) {
            refreshExports()
            exportStatus = status
            if (artifactId === root.selectedArtifactId) {
                root.selectedArtifactDetails = exportService.getExportStatus(artifactId)
            }
        }
    }

    Connections {
        target: projectService
        function onTaskTypeChanged(projectId, taskType) {
            if (projectId === root.currentProjectId) {
                root.currentTaskType = taskType
            }
        }
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        handle: Rectangle {
            implicitWidth: 4
            color: SplitHandle.pressed ? Theme.primaryGlow : (SplitHandle.hovered ? Theme.primaryGlow : Theme.borderColor)
            Behavior on color { ColorAnimation { duration: Theme.animDurationFast } }
        }

        // 左侧模型列表 Sidebar (240px, padding:0)
        Rectangle {
            id: sidebar
            SplitView.preferredWidth: 240
            SplitView.minimumWidth: Theme.sidebarMinWidth
            SplitView.maximumWidth: 400
            color: Theme.bgSide

            // 右侧分割线
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Theme.borderColor
            }

            ScrollView {
                anchors.fill: parent
                anchors.rightMargin: 1
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width - 1
                    spacing: 0

                // 区块标题 "模型列表" + 模型中心/对比入口
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 16
                    Layout.leftMargin: 16
                    Layout.bottomMargin: 8
                    spacing: Theme.spacingNormal

                    Text {
                        text: "模型列表"
                        font.pixelSize: Theme.fontSizeNormal
                        font.weight: Font.DemiBold
                        font.family: Theme.fontFamily
                        color: Theme.textMain
                    }

                    Item { Layout.fillWidth: true }

                    // 模型中心入口
                    Text {
                        text: "模型中心"
                        color: modelHubMouse.containsMouse ? Theme.primaryGlow : Theme.textMuted
                        font.pixelSize: Theme.fontSizeCaption
                        font.family: Theme.fontFamily
                        MouseArea {
                            id: modelHubMouse
                            anchors.fill: parent
                            anchors.margins: -4
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: appController.currentPage = "model"
                        }
                    }

                    // 指标对比入口
                    Text {
                        text: "指标对比"
                        color: compareMouse.containsMouse ? Theme.primaryGlow : Theme.textMuted
                        font.pixelSize: Theme.fontSizeCaption
                        font.family: Theme.fontFamily
                        MouseArea {
                            id: compareMouse
                            anchors.fill: parent
                            anchors.margins: -4
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: appController.currentPage = "compare"
                        }
                    }
                }

                // 模型版本列表
                ListView {
                    id: modelVersionList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 0
                    Layout.rightMargin: 0
                    clip: true
                    spacing: 0

                    model: modelVersionModel
                    delegate: Rectangle {
                        id: listDelegate
                        width: modelVersionList.width
                        height: 52
                        // 选中态：左边框3px primaryGlow + 半透明背景
                        color: {
                            if (root.selectedVersionId === model.versionId) return Qt.alpha(Theme.primaryGlow, 0.05)
                            if (delegateMouse.containsMouse) return Theme.bgHover
                            return "transparent"
                        }

                        // 选中态左边框高亮
                        Rectangle {
                            visible: root.selectedVersionId === model.versionId
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 3
                            color: Theme.primaryGlow
                        }

                        // 内容：模型名(bold) + 状态行
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            anchors.topMargin: 8
                            anchors.bottomMargin: 8
                            spacing: 2

                            Text {
                                text: model.bestWeightPath ? model.bestWeightPath.split("/").pop().split("\\").pop() : "版本 " + model.versionId.substring(0, 8)
                                font.pixelSize: Theme.fontSizeSmall
                                font.weight: Font.DemiBold
                                font.family: Theme.fontFamily
                                color: root.selectedVersionId === model.versionId ? Theme.primaryGlow : Theme.textMain
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: {
                                    var metrics = model.metricsJson ? JSON.parse(model.metricsJson) : {}
                                    if (root.isAnomalyProject) {
                                        if (metrics.auroc !== undefined) return "AUROC: " + (metrics.auroc * 100).toFixed(1) + "%"
                                    } else if (metrics.mAP50 !== undefined) {
                                        return "mAP50: " + (metrics.mAP50 * 100).toFixed(1) + "%"
                                    }
                                    return "未评估"
                                }
                                font.pixelSize: Theme.fontSizeCaption
                                font.family: Theme.fontFamily
                                color: Theme.textMuted
                                Layout.fillWidth: true
                            }
                        }

                        MouseArea {
                            id: delegateMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.selectedVersionId = model.versionId
                                // 同步列表 currentIndex：「训练任务」等详情按 currentIndex 取行数据
                                modelVersionList.currentIndex = index
                                refreshExports()
                            }
                        }
                    }
                }
            }
        }
        } // 关闭 sidebar Rectangle

        // ============================================================
        // 中心内容区（左右分栏）
        // ============================================================
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 4
                color: SplitHandle.pressed ? Theme.primaryGlow : (SplitHandle.hovered ? Theme.primaryGlow : Theme.borderColor)
                Behavior on color { ColorAnimation { duration: Theme.animDurationFast } }
            }

            // 左侧参数面板 (280px, bgSide, border-right 1px borderColor)
            Rectangle {
                id: paramPanel
                SplitView.preferredWidth: 280
                SplitView.minimumWidth: 200
                SplitView.maximumWidth: 400
                color: Theme.bgSide

                    // 右侧分割线
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
                        padding: 16
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        ColumnLayout {
                            width: parent.width
                            spacing: Theme.spacingLarge

                            // ---- Card 1: 导出模型 ----
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingNormal

                                // section-title "导出模型" (border-left 2px primaryGlow)
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 20
                                    color: "transparent"

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: 2
                                        color: Theme.primaryGlow
                                    }

                                    Text {
                                        anchors.left: parent.left
                                        anchors.leftMargin: 10
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: "导出模型"
                                        font.pixelSize: Theme.fontSizeNormal
                                        font.weight: Font.DemiBold
                                        font.family: Theme.fontFamily
                                        color: Theme.textMain
                                    }
                                }

                                // 设备
                                ParamRow {
                                    label: "设备"
                                    labelWidth: 100

                                    LtComboBox {
                                        id: deviceCombo
                                        // 显示中文设备名，导出时映射回后端设备串
                                        model: ["自动", "CPU", "GPU 0"]
                                        property var deviceValues: ["auto", "cpu", "0"]
                                        property string deviceValue: deviceValues[currentIndex] || "auto"
                                    }
                                }

                                // 测试权重
                                ParamRow {
                                    label: "测试权重"
                                    labelWidth: 100

                                    LtComboBox {
                                        id: weightCombo
                                        model: ["最佳权重", "最末权重"]
                                    }
                                }

                                // 导出格式
                                ParamRow {
                                    label: "导出格式"
                                    labelWidth: 100

                                    LtComboBox {
                                        id: formatCombo
                                        model: root.isAnomalyProject ? ["pt", "onnx"] : ["pt", "onnx"]
                                        currentIndex: 1
                                    }
                                }

                                // 导出路径: input + "选择"按钮（FormField 承载内联校验）
                                FormField {
                                    label: "导出路径"
                                    errorText: root.fieldError("outputPath")
                                    Layout.fillWidth: true

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: Theme.spacingSmall

                                        TextField {
                                            id: outputPathField
                                            Layout.fillWidth: true
                                            placeholderText: "自动生成"
                                            color: Theme.textMain
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamilyMono
                                            background: Rectangle {
                                                color: Theme.bgInput
                                                border.color: {
                                                    if (root.fieldError("outputPath") !== "") return Theme.fieldErrorBorder
                                                    return outputPathField.activeFocus ? Theme.primary : Theme.borderColor
                                                }
                                                border.width: 1
                                                radius: Theme.radiusSmall
                                            }
                                        }

                                        // "选择"按钮
                                        Rectangle {
                                            width: 48
                                            height: 32
                                            radius: Theme.radiusSmall
                                            color: browseMouse.containsMouse ? Theme.bgHover : Theme.bgCard
                                            border.color: Theme.borderColor
                                            border.width: 1

                                            Text {
                                                anchors.centerIn: parent
                                                text: "选择"
                                                font.pixelSize: Theme.fontSizeCaption
                                                font.family: Theme.fontFamily
                                                color: Theme.textMuted
                                            }

                                            MouseArea {
                                                id: browseMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: outputFolderDialog.open()
                                            }
                                        }
                                    }
                                }

                                // ONNX 附加配置（仅 onnx 格式显示）
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    visible: formatCombo.currentText === "onnx"
                                    spacing: Theme.spacingSmall

                                    // 分割线
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 1
                                        color: Theme.borderColor
                                    }

                                    // Opset版本
                                    ParamRow {
                                        label: "Opset 版本"
                                        labelWidth: 100

                                        Stepper {
                                            id: opsetStepper
                                            value: 12
                                            minValue: 9
                                            maxValue: 17
                                            stepSize: 1
                                        }
                                    }

                                    // opset 内联错误文案
                                    Text {
                                        visible: root.fieldError("opset") !== ""
                                        text: root.fieldError("opset")
                                        color: Theme.fieldErrorText
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                    }

                                    // 简化模型
                                    ParamRow {
                                        label: "简化模型"
                                        labelWidth: 100

                                        ToggleSwitch {
                                            id: simplifySwitch
                                            checked: true
                                        }
                                    }

                                    // 动态轴
                                    ParamRow {
                                        label: "动态轴"
                                        labelWidth: 100

                                        ToggleSwitch {
                                            id: dynamicSwitch
                                            checked: false
                                        }
                                    }
                                }

                                // 导出模型按钮（真 Button：无障碍可达，UIA/键盘可操作）
                                Button {
                                    id: exportBtnRect
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 36
                                    Layout.topMargin: Theme.spacingSmall
                                    // 禁用条件：未选择版本 或 正在导出
                                    readonly property bool btnDisabled: !root.selectedVersionId || exportStatus === "running"
                                    enabled: !btnDisabled
                                    opacity: exportStatus === "running" ? 0.6 : 1.0
                                    // 无障碍/UIA 名称
                                    text: exportStatus === "running" ? "导出中..." : "导出模型"

                                    background: Rectangle {
                                        radius: Theme.radiusNormal
                                        color: {
                                            if (!exportBtnRect.enabled) return Theme.bgCard
                                            if (exportBtnRect.pressed) return Qt.darker(Theme.primary, 1.3)
                                            if (exportBtnRect.hovered) return Qt.lighter(Theme.primary, 1.1)
                                            return Theme.primary
                                        }
                                    }

                                    contentItem: RowLayout {
                                        spacing: Theme.spacingNormal

                                        BusyIndicator {
                                            Layout.preferredWidth: 16
                                            Layout.preferredHeight: 16
                                            running: exportStatus === "running"
                                            visible: running
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: exportBtnRect.text
                                            font.pixelSize: Theme.fontSizeNormal
                                            font.weight: Font.DemiBold
                                            font.family: Theme.fontFamily
                                            color: exportBtnRect.enabled ? "#FFFFFF" : Theme.textDisabled
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }

                                    onClicked: {
                                        if (exportBtnRect.btnDisabled) return
                                        root.startExportWithValidation()
                                    }
                                }

                                // 导出状态反馈标签
                                StatusTag {
                                    visible: root.exportActionMessage !== ""
                                    text: root.exportActionMessage
                                    tone: root.exportActionTone
                                    Layout.fillWidth: true
                                    Layout.topMargin: Theme.spacingSmall
                                }
                            }

                            // ---- Card 2: 导出报告 ----
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingNormal

                                // section-title "导出报告" (border-left 2px primaryGlow)
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 20
                                    color: "transparent"

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: 2
                                        color: Theme.primaryGlow
                                    }

                                    Text {
                                        anchors.left: parent.left
                                        anchors.leftMargin: 10
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: "导出报告"
                                        font.pixelSize: Theme.fontSizeNormal
                                        font.weight: Font.DemiBold
                                        font.family: Theme.fontFamily
                                        color: Theme.textMain
                                    }
                                }

                                // 报告类型 ComboBox
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Text {
                                        text: "报告类型"
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamily
                                        color: Theme.textMuted
                                    }

                                    LtComboBox {
                                        id: reportTypeCombo
                                        Layout.fillWidth: true
                                        model: ["训练报告", "评估报告", "对比报告"]
                                    }
                                }

                                // 导出报告按钮 (btn-secondary)
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 36
                                    Layout.topMargin: Theme.spacingSmall
                                    radius: Theme.radiusNormal
                                    color: reportBtnMouse.containsMouse ? Theme.bgHover : Theme.bgCard
                                    border.color: Theme.borderColor
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: "导出报告"
                                        font.pixelSize: Theme.fontSizeNormal
                                        font.family: Theme.fontFamily
                                        color: Theme.textMuted
                                    }

                                    MouseArea {
                                        id: reportBtnMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (!root.selectedVersionId) {
                                                exportActionMessage = "请先选择模型版本"
                                                exportActionTone = "warning"
                                                return
                                            }
                                            // 收集报告数据
                                            var reportData = {
                                                "modelVersion": root.selectedVersionId,
                                                "exportFormat": formatCombo.currentText,
                                                "validationResult": root.artifactSafe.validationResult || "未验证"
                                            }
                                            var reportJson = JSON.stringify(reportData)
                                            var reportPath = exportService.exportReport(
                                                root.currentProjectId,
                                                root.selectedVersionId,
                                                reportTypeCombo.currentText,
                                                reportJson
                                            )
                                            if (reportPath !== "") {
                                                exportActionMessage = "报告已保存到: " + reportPath
                                                exportActionTone = "success"
                                            } else {
                                                exportActionMessage = "报告导出失败"
                                                exportActionTone = "danger"
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

                // ========================================================
                // 右侧展示区 (flex-grow:1, bgMain)
                // ========================================================
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.bgMain

                    ScrollView {
                        id: detailScroll
                        anchors.fill: parent
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                        // 内容宽=视口宽：不设时 contentItem 按子项 implicit 求解，
                        // 与列宽绑定成环会把内容撑到视口两倍（右侧内容被推出屏外）
                        contentWidth: availableWidth

                        ColumnLayout {
                            id: detailColumn
                            // 只写 anchors.margins 而无 anchors 时边距全部失效，内容顶到边缘
                            x: Theme.spacingLarge
                            y: Theme.spacingLarge
                            width: Math.max(detailScroll.availableWidth - Theme.spacingLarge * 2, 320)
                            spacing: Theme.spacingNormal

                            // 未选择模型时的空状态提示
                            Item {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 220
                                visible: root.selectedVersionId === ""

                                EmptyState {
                                    anchors.centerIn: parent
                                    width: parent.width - Theme.spacingXLarge * 2
                                    icon: "export"
                                    title: "请从左侧选择模型版本"
                                    description: "选择模型后可查看导出历史与版本信息"
                                    actionText: "前往模型中心"
                                    onActionClicked: appController.currentPage = "model"
                                }
                            }

                            // ---- 版本信息卡片 ----
                            CollapsibleSection {
                                title: "版本信息"
                                Layout.fillWidth: true
                                expanded: true
                                visible: root.selectedVersionId !== ""

                                ColumnLayout {
                                    width: parent.width
                                    spacing: Theme.spacingSmall

                                    // 版本号
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.spacingNormal

                                        Text {
                                            text: "版本号"
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamily
                                            color: Theme.textMuted
                                            Layout.preferredWidth: 80
                                        }

                                        Text {
                                            text: root.selectedVersionId ? root.selectedVersionId.substring(0, 8) : "N/A"
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamilyMono
                                            font.weight: Font.Bold
                                            color: Theme.primaryGlow
                                        }
                                    }

                                    // 训练任务ID
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.spacingNormal

                                        Text {
                                            text: "训练任务"
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamily
                                            color: Theme.textMuted
                                            Layout.preferredWidth: 80
                                        }

                                        Text {
                                            text: {
                                                if (!root.selectedVersionId) return "N/A"
                                                var idx = modelVersionModel.index(modelVersionList.currentIndex, 0)
                                                var runId = modelVersionModel.data(idx, Qt.UserRole + 1)
                                                return runId ? runId.substring(0, 8) + "..." : "N/A"
                                            }
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamilyMono
                                            color: Theme.textMain
                                        }
                                    }

                                    // 数据冻结版ID
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Theme.spacingNormal

                                        Text {
                                            text: "数据冻结版"
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamily
                                            color: Theme.textMuted
                                            Layout.preferredWidth: 80
                                        }

                                        Text {
                                            text: "N/A"
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamilyMono
                                            color: Theme.textMain
                                        }
                                    }
                                }
                            }

                            // ---- 导出历史卡片 ----
                            CollapsibleSection {
                                title: "导出历史"
                                Layout.fillWidth: true
                                expanded: true
                                visible: root.selectedVersionId !== ""

                                ColumnLayout {
                                    width: parent.width
                                    spacing: Theme.spacingSmall

                                    // 导出历史列表
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: exportHistory.length > 0 ? Math.min(exportHistoryList.contentHeight + 40, 300) : 60
                                        color: Theme.bgInput
                                        radius: Theme.radiusSmall

                                        // 空状态
                                        Text {
                                            visible: exportHistory.length === 0
                                            anchors.centerIn: parent
                                            text: "暂无导出记录"
                                            font.pixelSize: Theme.fontSizeSmall
                                            font.family: Theme.fontFamily
                                            color: Theme.textMuted
                                        }

                                        // 有数据时显示列表
                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: Theme.spacingSmall
                                            visible: exportHistory.length > 0
                                            spacing: 2

                                            Text {
                                                text: "共 " + exportHistory.length + " 条导出记录"
                                                font.pixelSize: Theme.fontSizeCaption
                                                font.family: Theme.fontFamily
                                                color: Theme.textMuted
                                            }

                                            ListView {
                                                id: exportHistoryList
                                                Layout.fillWidth: true
                                                Layout.fillHeight: true
                                                clip: true
                                                model: exportHistory
                                                spacing: 2

                                                delegate: Rectangle {
                                                    width: exportHistoryList.width
                                                    height: 36
                                                    radius: Theme.radiusSmall
                                                    color: root.selectedArtifactId === modelData.id ? Qt.alpha(Theme.primaryGlow, 0.08) : Theme.bgCard
                                                    border.color: root.selectedArtifactId === modelData.id ? Theme.primaryGlow : "transparent"
                                                    border.width: 1

                                                    RowLayout {
                                                        anchors.fill: parent
                                                        anchors.leftMargin: Theme.spacingSmall
                                                        anchors.rightMargin: Theme.spacingSmall
                                                        spacing: Theme.spacingSmall

                                                        // 状态圆点
                                                        Rectangle {
                                                            width: 8
                                                            height: 8
                                                            radius: 4
                                                            Layout.alignment: Qt.AlignVCenter
                                                            color: {
                                                                switch (modelData.status) {
                                                                    case "succeeded": return Theme.success
                                                                    case "failed": return Theme.danger
                                                                    case "running": return Theme.warning
                                                                    default: return Theme.textMuted
                                                                }
                                                            }
                                                        }

                                                        // 格式标签
                                                        Text {
                                                            text: modelData.format ? modelData.format.toUpperCase() : "?"
                                                            font.pixelSize: Theme.fontSizeCaption
                                                            font.family: Theme.fontFamilyMono
                                                            font.weight: Font.Bold
                                                            color: Theme.primaryGlow
                                                        }

                                                        // 产物ID
                                                        Text {
                                                            text: modelData.id ? modelData.id.substring(0, 8) + "..." : ""
                                                            font.pixelSize: Theme.fontSizeCaption
                                                            font.family: Theme.fontFamilyMono
                                                            color: Theme.textMuted
                                                            Layout.fillWidth: true
                                                            elide: Text.ElideRight
                                                        }

                                                        // 状态文字
                                                        Text {
                                                            text: {
                                                                switch (modelData.status) {
                                                                    case "succeeded": return "成功"
                                                                    case "failed": return "失败"
                                                                    case "running": return "导出中"
                                                                    case "verifying": return "验证中"
                                                                    case "pending": return "等待中"
                                                                    default: return modelData.status || "未知"
                                                                }
                                                            }
                                                            font.pixelSize: Theme.fontSizeCaption
                                                            font.family: Theme.fontFamily
                                                            color: {
                                                                switch (modelData.status) {
                                                                    case "succeeded": return Theme.success
                                                                    case "failed": return Theme.danger
                                                                    case "running": return Theme.warning
                                                                    default: return Theme.textMuted
                                                                }
                                                            }
                                                        }

                                                        // 验证按钮（仅 succeeded 状态显示）
                                                        Rectangle {
                                                            visible: modelData.status === "succeeded"
                                                            width: 48
                                                            height: 22
                                                            radius: Theme.radiusSmall
                                                            color: verifyMouse.containsMouse ? Theme.bgHover : "transparent"
                                                            border.color: Theme.borderColor
                                                            border.width: 1

                                                            Text {
                                                                anchors.centerIn: parent
                                                                text: "验证"
                                                                font.pixelSize: Theme.fontSizeCaption - 1
                                                                font.family: Theme.fontFamily
                                                                color: Theme.textMuted
                                                            }

                                                            MouseArea {
                                                                id: verifyMouse
                                                                anchors.fill: parent
                                                                hoverEnabled: true
                                                                cursorShape: Qt.PointingHandCursor
                                                                onClicked: {
                                                                    if (modelData.id) {
                                                                        exportService.verifyExport(modelData.id)
                                                                    }
                                                                }
                                                            }
                                                        }

                                                        MouseArea {
                                                            anchors.fill: parent
                                                            acceptedButtons: Qt.LeftButton
                                                            onClicked: {
                                                                root.selectedArtifactId = modelData.id || ""
                                                                root.selectedArtifactDetails = root.selectedArtifactId ? exportService.getExportStatus(root.selectedArtifactId) : ({})
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            CollapsibleSection {
                                title: "验证详情"
                                Layout.fillWidth: true
                                expanded: true
                                visible: root.selectedVersionId !== ""

                                ColumnLayout {
                                    width: parent.width
                                    spacing: Theme.spacingSmall

                                    Rectangle {
                                        visible: root.selectedArtifactId !== ""
                                        Layout.fillWidth: true
                                        // 无高度约束时布局按 implicit(0) 求高 → 卡片塌缩不可见；
                                        // 由内容列的 implicitHeight 驱动（与下方输入/输出卡片同法）
                                        implicitHeight: verifyCol.implicitHeight + Theme.spacingSmall * 2
                                        radius: Theme.radiusSmall
                                        color: Theme.bgInput
                                        border.color: Theme.borderColor
                                        border.width: 1

                                        ColumnLayout {
                                            id: verifyCol
                                            anchors.fill: parent
                                            anchors.margins: Theme.spacingSmall
                                            spacing: 6

                                            RowLayout {
                                                Layout.fillWidth: true

                                                StatusTag {
                                                    text: root.validationStatusText(root.parsedValidationDetails)
                                                    tone: {
                                                        var details = root.parsedValidationDetails
                                                        if (details.valid === true) return "success"
                                                        if (details.verified === false || details.error) return "danger"
                                                        if (Object.keys(details).length === 0) return "neutral"
                                                        return "warning"
                                                    }
                                                }

                                                Item {
                                                    Layout.fillWidth: true
                                                }

                                                Text {
                                                    text: artifactSafe.format ? artifactSafe.format.toUpperCase() : "N/A"
                                                    font.pixelSize: Theme.fontSizeCaption
                                                    font.family: Theme.fontFamilyMono
                                                    color: Theme.textMuted
                                                }
                                            }

                                            Text {
                                                visible: !!artifactSafe.outputPath
                                                text: "产物路径：" + artifactSafe.outputPath
                                                wrapMode: Text.WrapAnywhere
                                                font.pixelSize: Theme.fontSizeCaption
                                                font.family: Theme.fontFamilyMono
                                                color: Theme.textMuted
                                                Layout.fillWidth: true
                                            }

                                            Text {
                                                visible: !!root.parsedValidationDetails.provider
                                                text: "验证引擎：" + root.parsedValidationDetails.provider
                                                font.pixelSize: Theme.fontSizeCaption
                                                font.family: Theme.fontFamily
                                                color: Theme.textMuted
                                            }

                                            Text {
                                                visible: !!root.parsedValidationDetails.note
                                                text: "说明：" + root.parsedValidationDetails.note
                                                wrapMode: Text.WrapAnywhere
                                                font.pixelSize: Theme.fontSizeCaption
                                                font.family: Theme.fontFamily
                                                color: Theme.textMuted
                                                Layout.fillWidth: true
                                            }

                                            Text {
                                                visible: !!root.parsedValidationDetails.error
                                                text: "错误：" + root.parsedValidationDetails.error
                                                wrapMode: Text.WrapAnywhere
                                                font.pixelSize: Theme.fontSizeCaption
                                                font.family: Theme.fontFamilyMono
                                                color: Theme.danger
                                                Layout.fillWidth: true
                                            }

                                            Repeater {
                                                model: root.parsedValidationDetails.inputs || []

                                                delegate: Rectangle {
                                                    required property var modelData
                                                    Layout.fillWidth: true
                                                    radius: Theme.radiusSmall
                                                    color: Qt.alpha(Theme.primaryGlow, 0.04)
                                                    border.color: Qt.alpha(Theme.primaryGlow, 0.18)
                                                    border.width: 1
                                                    implicitHeight: inputColumn.implicitHeight + Theme.spacingSmall * 2

                                                    ColumnLayout {
                                                        id: inputColumn
                                                        anchors.fill: parent
                                                        anchors.margins: Theme.spacingSmall
                                                        spacing: 2

                                                        Text {
                                                            text: "输入：" + (modelData.name || "未命名")
                                                            font.pixelSize: Theme.fontSizeCaption
                                                            font.weight: Font.DemiBold
                                                            font.family: Theme.fontFamily
                                                            color: Theme.textMain
                                                        }

                                                        Text {
                                                            text: "Shape: " + root.formatShape(modelData.shape)
                                                            font.pixelSize: Theme.fontSizeCaption
                                                            font.family: Theme.fontFamilyMono
                                                            color: Theme.textMuted
                                                        }

                                                        Text {
                                                            visible: !!modelData.type
                                                            text: "Type: " + modelData.type
                                                            font.pixelSize: Theme.fontSizeCaption
                                                            font.family: Theme.fontFamilyMono
                                                            color: Theme.textMuted
                                                        }
                                                    }
                                                }
                                            }

                                            Repeater {
                                                model: root.parsedValidationDetails.outputs || []

                                                delegate: Rectangle {
                                                    required property var modelData
                                                    Layout.fillWidth: true
                                                    radius: Theme.radiusSmall
                                                    color: Qt.alpha(Theme.success, 0.05)
                                                    border.color: Qt.alpha(Theme.success, 0.18)
                                                    border.width: 1
                                                    implicitHeight: outputColumn.implicitHeight + Theme.spacingSmall * 2

                                                    ColumnLayout {
                                                        id: outputColumn
                                                        anchors.fill: parent
                                                        anchors.margins: Theme.spacingSmall
                                                        spacing: 2

                                                        Text {
                                                            text: "输出：" + (modelData.name || "未命名")
                                                            font.pixelSize: Theme.fontSizeCaption
                                                            font.weight: Font.DemiBold
                                                            font.family: Theme.fontFamily
                                                            color: Theme.textMain
                                                        }

                                                        Text {
                                                            text: "Shape: " + root.formatShape(modelData.shape)
                                                            font.pixelSize: Theme.fontSizeCaption
                                                            font.family: Theme.fontFamilyMono
                                                            color: Theme.textMuted
                                                        }

                                                        Text {
                                                            visible: !!modelData.type
                                                            text: "Type: " + modelData.type
                                                            font.pixelSize: Theme.fontSizeCaption
                                                            font.family: Theme.fontFamilyMono
                                                            color: Theme.textMuted
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        visible: root.selectedArtifactId === ""
                                        text: "请选择一条导出记录查看验证详情"
                                        wrapMode: Text.WrapAnywhere
                                        font.pixelSize: Theme.fontSizeSmall
                                        font.family: Theme.fontFamily
                                        color: Theme.textMuted
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        visible: root.selectedArtifactId !== "" && !!root.parsedValidationDetails.rawText
                                        // rawText 可能为 undefined，直接赋值会报 Unable to assign [undefined] to QString
                                        text: root.parsedValidationDetails.rawText || ""
                                        wrapMode: Text.WrapAnywhere
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamilyMono
                                        color: Theme.textMuted
                                        Layout.fillWidth: true
                                    }
                                }
                            }

                            // 底部留白
                            Item { Layout.preferredHeight: Theme.spacingLarge }
                        }
                    }
                }
            }
    }

    // 初始化时加载数据
    Component.onCompleted: {
        if (currentProjectId !== "") {
            modelVersionModel.setProjectId(currentProjectId)
        }
    }

    // 导出目录选择
    FolderDialog {
        id: outputFolderDialog
        onAccepted: {
            var s = selectedFolder.toString()
            if (s.startsWith("file:///")) {
                s = s.substring(7)
                if (s.length >= 3 && s.charAt(0) === "/" && s.charAt(2) === ":")
                    s = s.substring(1)
            } else if (s.startsWith("file://")) {
                s = s.substring(6)
            }
            outputPathField.text = decodeURIComponent(s)
        }
    }
}
