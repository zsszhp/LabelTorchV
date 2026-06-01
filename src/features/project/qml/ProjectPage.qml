// ProjectPage.qml - V4 项目中心（赛博蓝科技风）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LabelTorch.Theme

Item {
    id: pageRoot

    // 背景装饰光晕
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.bgPrimary }
            GradientStop { position: 1.0; color: Theme.bgPrimary }
        }

        // 右上角装饰光晕
        Rectangle {
            width: 500
            height: 500
            radius: 250
            color: Qt.alpha(Theme.accentPrimary, 0.06)
            x: parent.width - 300
            y: -250
        }

        // 左下角装饰光晕
        Rectangle {
            width: 400
            height: 400
            radius: 200
            color: Qt.alpha(Theme.accentSecondary, 0.05)
            x: -150
            y: parent.height - 250
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
                    color: refreshBtn.hovered ? Theme.bgHover : Theme.bgTertiary
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
                background: Rectangle {
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Theme.accentPrimary }
                        GradientStop { position: 1.0; color: Theme.accentPrimary }
                    }
                    radius: Theme.radiusNormal
                }
                contentItem: Label {
                    text: newProjectBtn.text
                    color: Theme.textPrimary
                    font: newProjectBtn.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: newProjectDialog.open()
            }

            // 导入项目按钮
            Button {
                id: importProjectBtn
                text: "导入项目"
                font.family: Theme.fontFamily
                font.bold: true
                palette.buttonText: Theme.accentPrimary
                background: Rectangle {
                    color: importProjectBtn.hovered ? Theme.bgHover : Theme.bgTertiary
                    border.color: Theme.accentPrimary
                    border.width: 1
                    radius: Theme.radiusNormal
                }
                onClicked: importFolderDialog.open()
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
            cellWidth: 340
            cellHeight: 200
            model: projectModel

            // 空状态提示
            Label {
                anchors.centerIn: parent
                visible: projectGrid.count === 0
                text: "还没有项目\n点击「+ 新建项目」开始"
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeSubheading
                font.family: Theme.fontFamily
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            delegate: Item {
                width: 320
                height: 180

                Rectangle {
                    id: cardBg
                    anchors.fill: parent
                    color: cardMouseArea.containsMouse ? Theme.bgHover : Theme.bgCard
                    radius: Theme.radiusLarge
                    border.color: {
                        if (appController.currentProjectId === model.projectId) {
                            return Theme.accentPrimary
                        }
                        return cardMouseArea.containsMouse ? Theme.accentPrimary : Theme.border
                    }
                    border.width: appController.currentProjectId === model.projectId ? 2 : 1

                    // 顶部渐变装饰条
                    Rectangle {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 3
                        radius: Theme.radiusLarge
                        visible: appController.currentProjectId === model.projectId
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Theme.accentPrimary }
                            GradientStop { position: 1.0; color: Theme.accentSecondary }
                        }
                    }

                    Behavior on border.color {
                        ColorAnimation { duration: Theme.animDuration }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingLarge
                        spacing: Theme.spacingSmall

                        // 第一行：项目图标与激活标签
                        RowLayout {
                            Layout.fillWidth: true

                            // 装饰性项目首字母图标
                            Rectangle {
                                width: 40
                                height: 40
                                radius: Theme.radiusNormal
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: appController.currentProjectId === model.projectId ? Theme.accentPrimary : Theme.accentSecondary }
                                    GradientStop { position: 1.0; color: appController.currentProjectId === model.projectId ? Theme.accentPrimary : Theme.accentSecondary }
                                }
                                opacity: appController.currentProjectId === model.projectId ? 1.0 : 0.6

                                Label {
                                    anchors.centerIn: parent
                                    text: model.name ? model.name.charAt(0).toUpperCase() : "P"
                                    font.bold: true
                                    font.pixelSize: Theme.fontSizeLarge
                                    color: Theme.textPrimary
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
                                height: 22

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
                                font.pixelSize: Theme.fontSizeCaption
                                font.family: Theme.fontFamilyMono
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
                                text: appController.currentProjectId === model.projectId ? "当前激活" : "打开项目"
                                font.pixelSize: Theme.fontSizeSmall
                                font.family: Theme.fontFamily
                                font.bold: true
                                background: Rectangle {
                                    color: {
                                        if (appController.currentProjectId === model.projectId) {
                                            return Theme.accentSuccess
                                        }
                                        return openBtn.hovered ? Theme.accentPrimary : Theme.bgTertiary
                                    }
                                    radius: Theme.radiusSmall
                                    border.color: appController.currentProjectId === model.projectId ? Theme.accentSuccess : (openBtn.hovered ? Theme.accentPrimary : Theme.border)
                                    border.width: 1
                                }
                                contentItem: Label {
                                    text: openBtn.text
                                    color: appController.currentProjectId === model.projectId ? Theme.textPrimary : (openBtn.hovered ? Theme.textPrimary : Theme.textSecondary)
                                    font: openBtn.font
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: {
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

    // === 文件夹选择 ===
    FolderDialog {
        id: folderDialog
        title: "选择项目存储路径"
        onAccepted: {
            projectPathField.text = urlToPath(folderDialog.selectedFolder)
        }
    }

    // === 导入项目文件夹选择 ===
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
            } else {
                projectModel.refresh()
            }
        }
    }
}
