// ProjectPage.qml - V6 项目中心（像素级复刻参考UI）
// 左侧边栏(240px) + 可拖拽分割线(4px) + 中心内容区
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LabelTorch.Theme
import LabelTorch.Components
import LabelTorch.Shell
import QtQuick.Effects

Item {
    id: pageRoot

    // 路径校验结果存储
    QtObject {
        id: pathValidationResult
        property var errors: []
        property var warnings: []
        property bool valid: true
    }

    // 侧边栏宽度（可拖拽调整）
    property real sidebarW: Theme.sidebarWidth
    property string projectActionMessage: ""
    property string projectActionTone: "neutral"

    // 根路径是否存在（换机/改盘符后项目可能打不开）
    function pathExists(p) {
        if (typeof projectService === "undefined" || !p) return true
        return projectService.pathExists(p)
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // 页面显示时强制刷新项目列表，避免空列表/模型未同步
        Component.onCompleted: projectModel.refresh()
        onVisibleChanged: if (visible) projectModel.refresh()

        handle: Rectangle {
            implicitWidth: 4
            color: SplitHandle.pressed ? Theme.primaryGlow : (SplitHandle.hovered ? Theme.primaryGlow : Theme.borderColor)
            Behavior on color { ColorAnimation { duration: Theme.animDurationFast } }
        }

        // === 左侧边栏 ===
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
                anchors.rightMargin: 1
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: parent.width - Theme.spacingNormal * 2
                    x: Theme.spacingNormal
                    y: Theme.spacingNormal
                    spacing: Theme.spacingNormal

                    // 区块标题：项目管理
                    SectionTitle {
                        Layout.fillWidth: true
                        text: "项目管理"
                    }

                    // 新建项目
                    LtButton {
                        id: newProjectBtn
                        Layout.fillWidth: true
                        text: "新建项目"
                        variant: "primary"
                        iconName: "plus"
                        onClicked: newProjectDialog.open()
                    }

                    // 打开/导入项目
                    LtButton {
                        id: openProjectBtn
                        Layout.fillWidth: true
                        text: "打开项目"
                        variant: "secondary"
                        iconName: "folder"
                        onClicked: importFolderDialog.open()
                    }

                    // 载入示例项目（演示模式：一键获得带数据的完整项目）
                    LtButton {
                        id: demoProjectBtn
                        Layout.fillWidth: true
                        text: "载入示例项目"
                        variant: "ghost"
                        iconName: "images"
                        visible: demoBootstrap.demoProjectMissing()
                        onClicked: {
                            var pid = demoBootstrap.ensureDemoProject()
                            if (pid) {
                                projectModel.refresh()
                                projectService.openProject(pid)
                                var info = projectService.getCurrentProject()
                                appController.openProject(pid, info.name || "示例项目")
                                var taxes = taxonomyService.listTaxonomies(pid)
                                if (taxes.length > 0) {
                                    taxonomyModel.taxonomyId = taxes[0].id
                                }
                                projectActionMessage = "已载入示例项目（含12张合成缺陷图与标签）"
                                projectActionTone = "success"
                            } else {
                                projectActionMessage = "示例项目创建失败，请查看日志"
                                projectActionTone = "danger"
                            }
                        }
                    }

                    // 分割线
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.dividerColor
                    }

                    // 类别体系管理（可折叠区块）
                    CollapsibleSection {
                        id: taxonomySection
                        Layout.fillWidth: true
                        title: "类别体系"
                        expanded: appController.projectOpen

                        ColumnLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            spacing: Theme.spacingSmall

                            // 未打开项目提示
                            Text {
                                visible: !appController.projectOpen
                                text: "请先打开项目"
                                font.pixelSize: Theme.fontSizeSmall
                                font.family: Theme.fontFamily
                                color: Theme.textMuted
                            }

                            // 完整类别体系管理入口
                            RowLayout {
                                visible: appController.projectOpen
                                Layout.fillWidth: true
                                spacing: Theme.spacingSmall

                                Text {
                                    text: "完整管理类别体系"
                                    color: taxonomyHubMouse.containsMouse ? Theme.primaryGlow : Theme.textMuted
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.family: Theme.fontFamily
                                    MouseArea {
                                        id: taxonomyHubMouse
                                        anchors.fill: parent
                                        anchors.margins: -4
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: appController.currentPage = "taxonomy"
                                    }
                                }

                                Item { Layout.fillWidth: true }
                            }

                            // 添加类别输入行
                            RowLayout {
                                visible: appController.projectOpen
                                Layout.fillWidth: true
                                spacing: Theme.spacingSmall

                                TextField {
                                    id: newClassField
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 28
                                    placeholderText: "类别名称"
                                    placeholderTextColor: Theme.textDisabled
                                    color: Theme.textMain
                                    font.pixelSize: Theme.fontSizeSmall
                                    font.family: Theme.fontFamily

                                    background: Rectangle {
                                        color: Theme.bgInput
                                        radius: Theme.radiusSmall
                                        border.color: newClassField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                                        border.width: 1
                                    }

                                    onAccepted: addClassBtn.clicked()
                                }

                                Button {
                                    id: addClassBtn
                                    Layout.preferredWidth: 28
                                    Layout.preferredHeight: 28
                                    text: "+"
                                    font.pixelSize: Theme.fontSizeNormal
                                    font.bold: true

                                    background: Rectangle {
                                        color: addClassBtn.hovered ? Theme.primary : Theme.bgCard
                                        radius: Theme.radiusSmall
                                        border.color: Theme.primary
                                        border.width: 1
                                    }

                                    contentItem: Text {
                                        text: parent.text
                                        color: addClassBtn.hovered ? Theme.textMain : Theme.primary
                                        font: parent.font
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }

                                    onClicked: {
                                        if (newClassField.text.trim()) {
                                            taxonomyModel.addClass(newClassField.text.trim())
                                            newClassField.clear()
                                        }
                                    }
                                }
                            }

                            // 类别搜索（类别多时快速定位）
                            TextField {
                                id: classFilterField
                                visible: appController.projectOpen
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                placeholderText: "搜索类别…"
                                placeholderTextColor: Theme.textDisabled
                                color: Theme.textMain
                                font.pixelSize: Theme.fontSizeSmall
                                font.family: Theme.fontFamily

                                background: Rectangle {
                                    color: Theme.bgInput
                                    radius: Theme.radiusSmall
                                    border.color: classFilterField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                                    border.width: 1
                                }
                            }

                            // 类别列表（可滚动；ColumnLayout+Repeater 使搜索过滤不留空行）
                            ScrollView {
                                visible: appController.projectOpen
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(taxListCol.implicitHeight + 6, 300)
                                clip: true

                                ColumnLayout {
                                    id: taxListCol
                                    width: parent.width - 6
                                    x: 3
                                    spacing: Theme.spacingTiny

                                    Repeater {
                                        model: taxonomyModel

                                        Rectangle {
                                            width: parent.width
                                            height: 32
                                            // 搜索过滤：不匹配的类别从列表隐藏（Layout 跳过不可见项）
                                            visible: classFilterField.text === ""
                                                     || model.className.toLowerCase()
                                                        .indexOf(classFilterField.text.toLowerCase()) >= 0
                                            color: classMouseArea.containsMouse ? Theme.bgHover : "transparent"
                                            radius: Theme.radiusSmall

                                            MouseArea {
                                                id: classMouseArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                            }

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: Theme.spacingSmall
                                                anchors.rightMargin: Theme.spacingSmall
                                                spacing: Theme.spacingSmall

                                                // 类别色块
                                                Rectangle {
                                                    width: 14
                                                    height: 14
                                                    radius: 2
                                                    color: Theme.classColors[model.classIndex % Theme.classColors.length]
                                                }

                                                // 类别名称
                                                Text {
                                                    Layout.fillWidth: true
                                                    text: model.className
                                                    font.pixelSize: Theme.fontSizeSmall
                                                    font.family: Theme.fontFamily
                                                    color: Theme.textMain
                                                    elide: Text.ElideRight
                                                }

                                        // 重命名按钮
                                        SvgIcon {
                                            icon: "edit"
                                            width: 12
                                            height: 12
                                            color: classEditBtn.containsMouse ? Theme.primaryGlow : Theme.textMuted
                                            visible: classMouseArea.containsMouse
                                            anchors.verticalCenter: parent.verticalCenter

                                            MouseArea {
                                                id: classEditBtn
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                hoverEnabled: true
                                                onClicked: appController.currentPage = "taxonomy"
                                            }
                                        }

                                        // 删除按钮
                                        SvgIcon {
                                            icon: "close"
                                            width: 12
                                            height: 12
                                            color: classDelBtn.containsMouse ? Theme.danger : Theme.textMuted
                                            visible: classMouseArea.containsMouse
                                            anchors.verticalCenter: parent.verticalCenter

                                            MouseArea {
                                                id: classDelBtn
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                hoverEnabled: true
                                                onClicked: taxonomyModel.removeClass(model.classIndex)
                                            }
                                        } // SvgIcon
                                    } // RowLayout
                                } // Rectangle
                                } // Repeater
                            } // ColumnLayout
                            } // ScrollView

                            // 类别统计
                            Text {
                                visible: appController.projectOpen
                                text: "共 " + taxonomyModel.rowCount() + " 个类别"
                                font.pixelSize: Theme.fontSizeCaption
                                font.family: Theme.fontFamily
                                color: Theme.textMuted
                            }
                        }
                    }

                    // 刷新列表按钮
                    Button {
                        id: refreshBtn
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        text: "刷新列表"
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily

                        background: Rectangle {
                            color: refreshBtn.hovered ? Theme.bgHover : "transparent"
                            border.color: Theme.borderColor
                            border.width: 1
                            radius: Theme.radiusSmall
                        }

                        contentItem: Text {
                            text: parent.text
                            color: Theme.textMuted
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: projectModel.refresh()
                    }
                }
            }
        }

        // === 中心内容区 ===
        Rectangle {
            SplitView.fillWidth: true
            color: Theme.bgMain

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingXLarge
                spacing: 0

                // 区块标题：最近使用项（14px，底部20px间距）
                SectionTitle {
                    Layout.fillWidth: true
                    Layout.bottomMargin: Theme.spacingLarge + Theme.spacingNormal
                    text: "最近使用项"
                }

                StatusTag {
                    visible: pageRoot.projectActionMessage !== ""
                    text: pageRoot.projectActionMessage
                    tone: pageRoot.projectActionTone
                    Layout.alignment: Qt.AlignLeft
                    Layout.bottomMargin: Theme.spacingNormal
                }

                // 项目卡片列表
                ListView {
                    id: projectList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: projectModel
                    spacing: Theme.spacingSmall

                    // 空状态：统一 EmptyState，主按钮直达新建项目
                    EmptyState {
                        anchors.centerIn: parent
                        width: parent.width - Theme.spacingXLarge * 2
                        visible: projectList.count === 0
                        icon: "folder"
                        title: "暂无项目"
                        description: "创建项目后即可导入数据集并开始训练"
                        actionText: "新建项目"
                        onActionClicked: newProjectDialog.open()
                    }

                    delegate: Rectangle {
                        id: cardRect
                        width: projectList.width
                        height: 64
                        color: {
                            if (appController.currentProjectId === model.projectId) return Theme.bgSelected
                            if (cardMouseArea.containsMouse) return Theme.bgHover
                            return Theme.bgCard
                        }
                        radius: Theme.radiusNormal
                        border.color: {
                            if (appController.currentProjectId === model.projectId) return Theme.primaryGlow
                            if (cardMouseArea.containsMouse) return Theme.primaryGlow
                            return Theme.borderColor
                        }
                        border.width: 1

                        // 悬浮/激活发光阴影
                        layer.enabled: cardMouseArea.containsMouse || appController.currentProjectId === model.projectId
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowColor: Theme.primaryGlow
                            shadowBlur: 0.25
                        }

                        // 选中态左边框高亮
                        Rectangle {
                            visible: appController.currentProjectId === model.projectId
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 3
                            radius: 1
                            color: Theme.primaryGlow
                        }

                        // 颜色过渡动画
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                        Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                        // 鼠标交互区域
                        MouseArea {
                            id: cardMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onDoubleClicked: openProject(model.projectId, model.name)
                        }

                        // 卡片内容布局
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingNormal
                            anchors.rightMargin: Theme.spacingNormal
                            spacing: Theme.spacingNormal

                            // 项目首字母图标（26×26，渐变背景 + 悬浮缩放动效）
                            Rectangle {
                                id: logoRect
                                Layout.preferredWidth: 26
                                Layout.preferredHeight: 26
                                radius: Theme.radiusSmall

                                scale: cardMouseArea.containsMouse ? 1.1 : 1.0
                                Behavior on scale { NumberAnimation { duration: Theme.animDurationFast; easing.type: Easing.OutBack } }

                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: Theme.primary }
                                    GradientStop { position: 1.0; color: Theme.primaryGlow }
                                }
                                opacity: appController.currentProjectId === model.projectId ? 1.0 : 0.7

                                Text {
                                    anchors.centerIn: parent
                                    text: model.name ? model.name.substring(0, 2).toUpperCase() : "P"
                                    font.bold: true
                                    font.pixelSize: Theme.fontSizeSmall
                                    font.family: Theme.fontFamily
                                    color: Theme.textMain
                                }
                            }

                            // 项目名 + 路径（左列）
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingTiny

                                // 项目名（14px bold white）
                                Text {
                                    text: model.name
                                    font.bold: true
                                    font.pixelSize: Theme.fontSizeNormal + 1
                                    font.family: Theme.fontFamily
                                    color: Theme.textMain
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                // 路径（11px muted）
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingSmall

                                    Text {
                                        text: model.path
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.family: Theme.fontFamilyMono
                                        color: Theme.textMuted
                                        Layout.fillWidth: true
                                        elide: Text.ElideMiddle
                                    }

                                    // 根路径不存在时提示（换机/移盘后常见）
                                    StatusTag {
                                        visible: !pathExists(model.path)
                                        text: "路径缺失"
                                        tone: "warning"
                                    }
                                }
                            }

                            // 弹性占位，将右侧内容推到最右
                            Item { Layout.fillWidth: true }

                            // 最后修改时间（12px muted）
                            Text {
                                text: "最后修改: " + model.createdAt
                                font.pixelSize: Theme.fontSizeSmall
                                font.family: Theme.fontFamilyMono
                                color: Theme.textMuted
                            }

                            // 当前项目徽章
                            Rectangle {
                                visible: appController.currentProjectId === model.projectId
                                color: Qt.alpha(Theme.primary, 0.15)
                                border.color: Theme.primary
                                border.width: 1
                                radius: Theme.radiusSmall
                                width: 64
                                height: 22

                                Text {
                                    anchors.centerIn: parent
                                    text: "当前项目"
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.bold: true
                                    font.family: Theme.fontFamilyMono
                                    color: Theme.primaryGlow
                                }
                            }

                            // 打开按钮（非激活项目显示）
                            Button {
                                id: openItemBtn
                                visible: appController.currentProjectId !== model.projectId
                                Layout.preferredHeight: 28
                                text: "打开"
                                font.pixelSize: Theme.fontSizeSmall
                                font.family: Theme.fontFamily
                                font.bold: true

                                background: Rectangle {
                                    color: openItemBtn.hovered ? Theme.primary : Theme.bgCard
                                    radius: Theme.radiusSmall
                                    border.color: Theme.primary
                                    border.width: 1
                                }

                                contentItem: Text {
                                    text: parent.text
                                    color: openItemBtn.hovered ? Theme.textMain : Theme.primary
                                    font: parent.font
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: openProject(model.projectId, model.name)
                            }

                            // 删除按钮
                            Button {
                                id: delItemBtn
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28

                                background: Rectangle {
                                    color: delItemBtn.hovered ? Qt.alpha(Theme.danger, 0.15) : "transparent"
                                    radius: Theme.radiusSmall
                                }

                                contentItem: SvgIcon {
                                    icon: "close"
                                    width: 10
                                    height: 10
                                    anchors.centerIn: parent
                                    color: delItemBtn.hovered ? Theme.danger : Theme.textMuted
                                }

                                onClicked: {
                                    deleteConfirmDialog.previewAndOpen(model.projectId)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // === 新建项目对话框（6个字段） ===
    Dialog {
        id: newProjectDialog
        title: "新建项目"
        modal: true
        anchors.centerIn: parent
        width: 500
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

            // 对话框标题
            Text {
                text: "创建新的缺陷检测治理项目"
                font.pixelSize: Theme.fontSizeSubheading
                font.bold: true
                font.family: Theme.fontFamily
                color: Theme.textMain
            }

            // 1. 项目名称
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                Text {
                    text: "项目名称 *"
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }

                TextField {
                    id: projectNameField
                    Layout.fillWidth: true
                    placeholderText: "例如: 电池表面缺陷检测"
                    placeholderTextColor: Theme.textDisabled
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeNormal
                    font.family: Theme.fontFamily

                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: projectNameField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                        border.width: 1
                    }
                }
            }

            // 2. 存储路径（含路径校验）
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                Text {
                    text: "存储路径 *"
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingNormal

                    TextField {
                        id: projectPathField
                        Layout.fillWidth: true
                        placeholderText: "选择一个本地文件夹路径"
                        placeholderTextColor: Theme.textDisabled
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: {
                                if (projectPathField.activeFocus) return Theme.primaryGlow
                                if (pathValidationResult.errors.length > 0) return Theme.danger
                                if (pathValidationResult.warnings.length > 0) return Theme.warning
                                return Theme.borderColor
                            }
                            border.width: 1
                        }

                        onTextChanged: {
                            if (text.length > 0) {
                                var result = projectService.validateProjectPath(text)
                                pathValidationResult.errors = result.errors
                                pathValidationResult.warnings = result.warnings
                                pathValidationResult.valid = result.valid
                            } else {
                                pathValidationResult.errors = []
                                pathValidationResult.warnings = []
                                pathValidationResult.valid = true
                            }
                        }
                    }

                    Button {
                        Layout.preferredHeight: 36
                        text: "浏览..."
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily

                        background: Rectangle {
                            color: parent.hovered ? Theme.bgHover : Theme.bgCard
                            radius: Theme.radiusSmall
                            border.color: Theme.borderColor
                            border.width: 1
                        }

                        contentItem: Text {
                            text: parent.text
                            color: Theme.textMain
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: folderDialog.open()
                    }
                }

                // 路径校验结果提示
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingTiny
                    visible: pathValidationResult.errors.length > 0 || pathValidationResult.warnings.length > 0

                    Repeater {
                        model: pathValidationResult.errors
                        delegate: Text {
                            text: "错误：" + modelData
                            color: Theme.danger
                            font.pixelSize: Theme.fontSizeCaption
                            font.family: Theme.fontFamily
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                    }
                    Repeater {
                        model: pathValidationResult.warnings
                        delegate: Text {
                            text: "警告：" + modelData
                            color: Theme.warning
                            font.pixelSize: Theme.fontSizeCaption
                            font.family: Theme.fontFamily
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            // 3. 项目描述
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                Text {
                    text: "项目描述"
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }

                TextField {
                    id: projectDescField
                    Layout.fillWidth: true
                    placeholderText: "可选，简要描述项目用途"
                    placeholderTextColor: Theme.textDisabled
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeNormal
                    font.family: Theme.fontFamily

                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: projectDescField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                        border.width: 1
                    }
                }
            }

            // 4. 初始图库名称 + 5. 图库类型（一行两列）
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingLarge

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSmall

                    Text {
                        text: "初始图库名称"
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily
                        color: Theme.textMuted
                    }

                    TextField {
                        id: galleryNameField
                        Layout.fillWidth: true
                        text: "默认图库"
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: galleryNameField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                            border.width: 1
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSmall

                    Text {
                        text: "图库类型"
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily
                        color: Theme.textMuted
                    }

                    ComboBox {
                        id: taskTypeCombo
                        Layout.fillWidth: true
                        model: ["目标检测", "旋转框检测", "分类", "异常检测"]
                        currentIndex: 0

                        background: Rectangle {
                            color: Theme.bgInputDropdown
                            radius: Theme.radiusSmall
                            border.color: taskTypeCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                            border.width: 1
                        }

                        contentItem: Text {
                            text: taskTypeCombo.displayText
                            color: Theme.textMain
                            font.pixelSize: Theme.fontSizeNormal
                            font.family: Theme.fontFamily
                            leftPadding: Theme.spacingNormal
                            verticalAlignment: Text.AlignVCenter
                        }

                        delegate: ItemDelegate {
                            width: taskTypeCombo.width
                            contentItem: Text {
                                text: modelData
                                color: highlighted ? Theme.textMain : Theme.textMuted
                                font.pixelSize: Theme.fontSizeNormal
                                font.family: Theme.fontFamily
                                verticalAlignment: Text.AlignVCenter
                            }
                            highlighted: taskTypeCombo.highlightedIndex === index
                            background: Rectangle {
                                color: highlighted ? Theme.bgHover : Theme.bgInputDropdown
                            }
                        }
                    }
                }
            }

            // 6. 基准路径
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                Text {
                    text: "基准路径"
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingNormal

                    TextField {
                        id: basePathField
                        Layout.fillWidth: true
                        placeholderText: "可选，指定基准数据路径"
                        placeholderTextColor: Theme.textDisabled
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: basePathField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                            border.width: 1
                        }
                    }

                    Button {
                        Layout.preferredHeight: 36
                        text: "浏览..."
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamily

                        background: Rectangle {
                            color: parent.hovered ? Theme.bgHover : Theme.bgCard
                            radius: Theme.radiusSmall
                            border.color: Theme.borderColor
                            border.width: 1
                        }

                        contentItem: Text {
                            text: parent.text
                            color: Theme.textMain
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: baseFolderDialog.open()
                    }
                }
            }

            // 底部操作按钮
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingNormal

                Item { Layout.fillWidth: true }

                Button {
                    id: cancelBtn
                    Layout.preferredHeight: 36
                    Layout.preferredWidth: 100
                    text: "取消"
                    font.pixelSize: Theme.fontSizeNormal
                    font.family: Theme.fontFamily

                    background: Rectangle {
                        color: cancelBtn.hovered ? Theme.bgHover : Theme.bgCard
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

                    onClicked: newProjectDialog.reject()
                }

                Button {
                    id: confirmCreateBtn
                    Layout.preferredHeight: 36
                    Layout.preferredWidth: 120
                    text: "创建"
                    font.pixelSize: Theme.fontSizeNormal
                    font.bold: true
                    font.family: Theme.fontFamily

                    background: Rectangle {
                        color: confirmCreateBtn.hovered ? Theme.primaryGlow : Theme.primary
                        radius: Theme.radiusSmall
                    }

                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: newProjectDialog.accept()
                }
            }
        }

        // 确认创建
        onAccepted: {
            if (projectNameField.text && projectPathField.text) {
                if (!pathValidationResult.valid) return
                var taskTypeMap = ["detect", "obb", "classify", "anomaly"]
                var selectedTaskType = taskTypeMap[Math.max(0, taskTypeCombo.currentIndex)]
                var pid = projectService.createProject(projectNameField.text, projectPathField.text, selectedTaskType)
                if (pid) {
                    projectModel.refresh()
                    projectService.openProject(pid)
                    appController.openProject(pid, projectNameField.text)
                    var taxonomies = taxonomyService.listTaxonomies(pid)
                    if (taxonomies.length > 0) {
                        taxonomyModel.taxonomyId = taxonomies[0].id
                    }
                } else {
                    projectModel.refresh()
                }
                resetDialogFields()
            }
        }

        onRejected: resetDialogFields()

        // 重置对话框所有字段
        function resetDialogFields() {
            projectNameField.clear()
            projectPathField.clear()
            projectDescField.clear()
            galleryNameField.text = "默认图库"
            taskTypeCombo.currentIndex = 0
            basePathField.clear()
            pathValidationResult.errors = []
            pathValidationResult.warnings = []
            pathValidationResult.valid = true
        }
    }

    // === 删除项目确认对话框 ===
    // 危险操作统一二次确认：默认焦点「取消」，列明连带删除项
    ConfirmDialog {
        id: deleteConfirmDialog
        confirmTitle: "确认删除项目"
        message: "确定要彻底删除此项目吗？"
        confirmText: "删除"
        cancelText: "取消"
        // 连带影响项：由 previewProjectDeletion 填充
        impactItems: []

        function previewAndOpen(projectId) {
            deleteConfirmDialog.projectId = projectId
            var summary = projectService.previewProjectDeletion(projectId)
            var items = []
            if (summary) {
                if (summary.datasetCount > 0) items.push("数据集 " + summary.datasetCount + " 个")
                if (summary.sampleCount > 0) items.push("样本 " + summary.sampleCount + " 条")
                if (summary.snapshotCount > 0) items.push("数据冻结版 " + summary.snapshotCount + " 个")
                if (summary.trainingRunCount > 0) items.push("训练任务 " + summary.trainingRunCount + " 个")
                if (summary.modelCount > 0) items.push("模型版本 " + summary.modelCount + " 个")
                if (summary.taxonomyCount > 0) items.push("类别体系 " + summary.taxonomyCount + " 份")
            }
            if (items.length === 0)
                items.push("本地工程目录（如存在）")
            deleteConfirmDialog.impactItems = items
            openConfirm()
        }

        property string projectId: ""

        onConfirmed: {
            if (projectId) {
                projectService.deleteProject(projectId)
                projectModel.refresh()
                projectId = ""
            }
        }
        onCancelled: projectId = ""
    }

    // === 工具函数：URL转本地路径 ===
    function urlToPath(url) {
        var s = url.toString()
        if (s.startsWith("file:///")) {
            s = s.substring(7)
            // Windows路径：/C:/xxx → C:/xxx
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

    // === 打开项目：设置 currentProjectId + taxonomyId ===
    function openProject(projectId, projectName) {
        if (appController.currentProjectId !== projectId) {
            projectService.openProject(projectId)
            appController.openProject(projectId, projectName)
            var taxonomies = taxonomyService.listTaxonomies(projectId)
            if (taxonomies.length > 0) {
                taxonomyModel.taxonomyId = taxonomies[0].id
            }
            projectActionMessage = "已打开项目：" + projectName
            projectActionTone = "success"
        }
    }

    // === 文件夹选择对话框 × 3 ===

    // 项目存储路径选择
    FolderDialog {
        id: folderDialog
        title: "选择项目存储路径"
        onAccepted: {
            projectPathField.text = urlToPath(folderDialog.selectedFolder)
        }
    }

    // 基准数据路径选择
    FolderDialog {
        id: baseFolderDialog
        title: "选择基准数据路径"
        onAccepted: {
            basePathField.text = urlToPath(baseFolderDialog.selectedFolder)
        }
    }

    // 导入已有项目目录选择
    FolderDialog {
        id: importFolderDialog
        title: "选择已有项目目录"
        onAccepted: {
            var importPath = urlToPath(importFolderDialog.selectedFolder)
            var pid = projectService.importProject(importPath)
            if (pid) {
                projectModel.refresh()
                projectService.openProject(pid)
                var projInfo = projectService.getCurrentProject()
                appController.openProject(pid, projInfo.name || "导入项目")
                var taxonomies = taxonomyService.listTaxonomies(pid)
                if (taxonomies.length > 0) {
                    taxonomyModel.taxonomyId = taxonomies[0].id
                }
                projectActionMessage = "已导入并打开项目：" + (projInfo.name || "导入项目")
                projectActionTone = "success"
            } else {
                projectModel.refresh()
                projectActionMessage = "导入项目失败，请确认所选目录包含有效 project.json"
                projectActionTone = "danger"
            }
        }
    }
}
