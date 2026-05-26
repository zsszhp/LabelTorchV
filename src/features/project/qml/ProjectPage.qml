import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LabelTorch.Theme

Item {
    id: pageRoot

    // 背景光晕装饰
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.bgPrimary }
            GradientStop { position: 1.0; color: "#030408" }
        }

        Rectangle {
            width: 400
            height: 400
            radius: 200
            color: Qt.alpha(Theme.accentSecondary, 0.15)
            x: parent.width - 250
            y: -150
            // 模糊效果（底层 QML 可选，直接使用半透明色彩叠加）
        }

        Rectangle {
            width: 300
            height: 300
            radius: 150
            color: Qt.alpha(Theme.accentPrimary, 0.10)
            x: -100
            y: parent.height - 200
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingXLarge
        spacing: Theme.spacingLarge

        // === 顶栏：标题与操作 ===
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingNormal

            ColumnLayout {
                spacing: 2
                Label {
                    text: "项目中心"
                    font.pixelSize: Theme.fontSizeDisplay
                    font.bold: true
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                }
                Label {
                    text: "创建或选择一个缺陷检测项目，开启 AI 智能分析与数据集治理"
                    font.pixelSize: Theme.fontSizeNormal
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                }
            }

            Item { Layout.fillWidth: true }

            // 刷新按钮
            Button {
                id: refreshBtn
                text: "刷新 ↻"
                font.family: Theme.fontFamily
                font.bold: true
                palette.buttonText: Theme.textSecondary
                background: Rectangle {
                    color: refreshBtn.hovered ? Theme.bgHover : Theme.bgSecondary
                    border.color: Theme.border
                    border.width: 1
                    radius: Theme.radiusNormal
                }
                onClicked: projectModel.refresh()
            }

            // 新建项目按钮
            Button {
                id: newProjectBtn
                text: "+ 新建项目"
                font.family: Theme.fontFamily
                font.bold: true
                palette.buttonText: "#FFFFFF"
                background: Rectangle {
                    color: newProjectBtn.hovered ? Qt.lighter(Theme.accentPrimary, 1.1) : Theme.accentPrimary
                    radius: Theme.radiusNormal
                    // 按钮微光发光效果
                    layer.enabled: newProjectBtn.hovered
                }
                onClicked: newProjectDialog.open()
            }
        }

        // 分割线
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.divider
        }

        // === 项目卡片网格布局 ===
        GridView {
            id: projectGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cellWidth: 320
            cellHeight: 180
            model: projectModel

            delegate: Item {
                width: 300
                height: 160

                Rectangle {
                    id: cardBg
                    anchors.fill: parent
                    color: cardMouseArea.containsMouse ? Theme.bgHover : Qt.alpha(Theme.bgCard, Theme.glassOpacity)
                    radius: Theme.radiusLarge
                    border.color: {
                        if (appController.currentProjectId === model.projectId) {
                            return Theme.accentPrimary
                        }
                        return cardMouseArea.containsMouse ? Theme.accentSecondary : Theme.border
                    }
                    border.width: appController.currentProjectId === model.projectId ? 2 : 1

                    // 卡片阴影或发光阴影效果
                    Behavior on border.color {
                        ColorAnimation { duration: Theme.animDuration }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingLarge
                        spacing: Theme.spacingSmall

                        // 第一行：项目小图标与当前激活标签
                        RowLayout {
                            Layout.fillWidth: true

                            // 装饰性项目首字母图标
                            Rectangle {
                                width: 36
                                height: 36
                                radius: Theme.radiusNormal
                                color: appController.currentProjectId === model.projectId ? Qt.alpha(Theme.accentPrimary, 0.2) : Qt.alpha(Theme.textSecondary, 0.1)

                                Label {
                                    anchors.centerIn: parent
                                    text: model.name ? model.name.charAt(0).toUpperCase() : "P"
                                    font.bold: true
                                    font.pixelSize: Theme.fontSizeLarge
                                    color: appController.currentProjectId === model.projectId ? Theme.accentPrimary : Theme.textPrimary
                                    font.family: Theme.fontFamily
                                }
                            }

                            Item { Layout.fillWidth: true }

                            // 激活徽章
                            Rectangle {
                                visible: appController.currentProjectId === model.projectId
                                color: Qt.alpha(Theme.accentPrimary, 0.15)
                                border.color: Theme.accentPrimary
                                border.width: 1
                                radius: Theme.radiusSmall
                                width: 56
                                height: 20

                                Label {
                                    anchors.centerIn: parent
                                    text: "ACTIVE"
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.bold: true
                                    color: Theme.accentPrimary
                                    font.family: Theme.fontFamilyMono
                                }
                            }
                        }

                        // 第二行：项目名称及路径
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                text: model.name
                                font.bold: true
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeNormal
                                font.family: Theme.fontFamily
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Label {
                                text: model.path
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSizeSmall
                                font.family: Theme.fontFamily
                                Layout.fillWidth: true
                                elide: Text.ElideMiddle
                            }
                        }

                        Item { Layout.fillHeight: true }

                        // 第三行：创建时间与按钮组
                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                text: model.createdAt
                                color: Theme.textDisabled
                                font.pixelSize: Theme.fontSizeCaption
                                font.family: Theme.fontFamilyMono
                            }

                            Item { Layout.fillWidth: true }

                            // 打开/进入按钮
                            Button {
                                id: openBtn
                                text: appController.currentProjectId === model.projectId ? "已打开" : "打开"
                                enabled: appController.currentProjectId !== model.projectId
                                font.pixelSize: Theme.fontSizeSmall
                                font.family: Theme.fontFamily
                                font.bold: true
                                palette.buttonText: enabled ? Theme.textPrimary : Theme.textDisabled
                                background: Rectangle {
                                    color: {
                                        if (!openBtn.enabled) return "transparent"
                                        return openBtn.hovered ? Theme.accentSecondary : Theme.bgTertiary
                                    }
                                    radius: Theme.radiusSmall
                                    border.color: openBtn.enabled ? Theme.border : "transparent"
                                    border.width: 1
                                }
                                onClicked: {
                                    projectService.openProject(model.projectId)
                                    appController.openProject(model.projectId, model.name)
                                    var taxonomies = taxonomyService.listTaxonomies(model.projectId)
                                    if (taxonomies.length > 0) {
                                        taxonomyModel.taxonomyId = taxonomies[0].id
                                    }
                                }
                            }

                            // 删除按钮
                            Button {
                                id: delBtn
                                text: "删除"
                                font.pixelSize: Theme.fontSizeSmall
                                font.family: Theme.fontFamily
                                palette.buttonText: delBtn.hovered ? Theme.accentError : Theme.textMuted
                                background: Rectangle {
                                    color: "transparent"
                                }
                                onClicked: deleteConfirmDialog.projectId = model.projectId
                            }
                        }
                    }

                    MouseArea {
                        id: cardMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onDoubleClicked: {
                            if (appController.currentProjectId !== model.projectId) {
                                projectService.openProject(model.projectId)
                                appController.openProject(model.projectId, model.name)
                                var taxonomies = taxonomyService.listTaxonomies(model.projectId)
                                if (taxonomies.length > 0) {
                                    taxonomyModel.taxonomyId = taxonomies[0].id
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // === 新建项目对话框 ===
    Dialog {
        id: newProjectDialog
        title: "新建检测项目"
        modal: true
        anchors.centerIn: parent
        width: 440
        standardButtons: Dialog.Ok | Dialog.Cancel

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
                text: "创建新的缺陷检测治理项目"
                font.pixelSize: Theme.fontSizeSubheading
                font.bold: true
                color: Theme.textPrimary
                font.family: Theme.fontFamily
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                Label { text: "项目名称"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall }
                TextField {
                    id: projectNameField
                    Layout.fillWidth: true
                    placeholderText: "例如: 电池表面缺陷检测"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: projectNameField.activeFocus ? Theme.borderFocus : Theme.borderNormal
                        border.width: 1
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSmall

                Label { text: "存储路径"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingNormal
                    TextField {
                        id: projectPathField
                        Layout.fillWidth: true
                        placeholderText: "选择一个本地文件夹路径"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        background: Rectangle {
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: projectPathField.activeFocus ? Theme.borderFocus : Theme.borderNormal
                            border.width: 1
                        }
                    }
                    Button {
                        id: browseBtn
                        text: "浏览..."
                        font.family: Theme.fontFamily
                        palette.buttonText: Theme.textPrimary
                        background: Rectangle {
                            color: browseBtn.hovered ? Theme.bgHover : Theme.bgTertiary
                            radius: Theme.radiusSmall
                            border.color: Theme.border
                            border.width: 1
                        }
                        onClicked: folderDialog.open()
                    }
                }
            }
        }

        onAccepted: {
            if (projectNameField.text && projectPathField.text) {
                var pid = projectService.createProject(projectNameField.text, projectPathField.text)
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
                projectNameField.clear()
                projectPathField.clear()
            }
        }
    }

    // === 删除确认对话框 ===
    Dialog {
        id: deleteConfirmDialog
        title: "确认移除项目"
        modal: true
        anchors.centerIn: parent
        width: 360
        standardButtons: Dialog.Yes | Dialog.No

        property string projectId: ""

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
                text: "确定要彻底删除此项目吗？"
                font.bold: true
                color: Theme.accentError
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSubheading
            }

            Label {
                text: "警告：此操作不可撤销。对应的本地工程数据将不再受管辖。"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        onAccepted: {
            if (projectId) {
                projectService.deleteProject(projectId)
                projectModel.refresh()
                projectId = ""
            }
        }
        onRejected: projectId = ""
    }

    // === 文件夹选择 ===
    FolderDialog {
        id: folderDialog
        title: "选择项目存储路径"
        onAccepted: {
            var path = folderDialog.selectedFolder.toString()
            if (path.startsWith("file:///")) path = path.substring(8)
            else if (path.startsWith("file://")) path = path.substring(7)
            projectPathField.text = decodeURIComponent(path)
        }
    }
}
