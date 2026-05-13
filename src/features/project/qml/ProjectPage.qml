import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LabelTorch.Shell

Item {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "项目管理"
                font.pixelSize: Theme.fontSizeTitle
                font.bold: true
                color: Theme.textPrimary
                font.family: Theme.fontFamily
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "新建项目"
                font.family: Theme.fontFamily
                onClicked: newProjectDialog.open()
            }

            Button {
                text: "刷新"
                font.family: Theme.fontFamily
                onClicked: projectModel.refresh()
            }
        }

        Label {
            text: "创建或打开一个项目开始工作"
            font.pixelSize: Theme.fontSizeNormal
            color: Theme.textSecondary
            font.family: Theme.fontFamily
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: projectModel
            spacing: Theme.spacingSmall

            delegate: Rectangle {
                width: ListView.view.width
                height: 64
                color: mouseArea.containsMouse ? Theme.bgHover : Theme.bgCard
                radius: Theme.radiusNormal
                border.color: appController.currentProjectId === model.projectId ? Theme.accentPrimary : "transparent"
                border.width: appController.currentProjectId === model.projectId ? 2 : 0

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    Column {
                        Layout.fillWidth: true
                        Label { text: model.name; font.bold: true; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeNormal; font.family: Theme.fontFamily }
                        Label { text: model.path; color: Theme.textMuted; font.pixelSize: Theme.fontSizeSmall; font.family: Theme.fontFamily }
                    }

                    Label { text: model.createdAt; color: Theme.textMuted; font.pixelSize: Theme.fontSizeSmall; font.family: Theme.fontFamily }

                    Button {
                        text: appController.currentProjectId === model.projectId ? "已打开" : "打开"
                        enabled: appController.currentProjectId !== model.projectId
                        font.family: Theme.fontFamily
                        onClicked: {
                            projectService.openProject(model.projectId)
                            appController.openProject(model.projectId, model.name)
                            var taxonomies = taxonomyService.listTaxonomies(model.projectId)
                            if (taxonomies.length > 0) {
                                taxonomyModel.taxonomyId = taxonomies[0].id
                            }
                        }
                    }

                    Button {
                        text: "删除"
                        font.family: Theme.fontFamily
                        onClicked: deleteConfirmDialog.projectId = model.projectId
                    }
                }

                MouseArea {
                    id: mouseArea
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

    Dialog {
        id: newProjectDialog
        title: "新建项目"
        modal: true
        anchors.centerIn: parent
        width: 400

        ColumnLayout {
            width: parent.width
            spacing: 12

            Label { text: "项目名称"; color: Theme.textPrimary; font.family: Theme.fontFamily }
            TextField {
                id: projectNameField
                Layout.fillWidth: true
                placeholderText: "输入项目名称"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                background: Rectangle { color: Theme.bgInput; radius: Theme.radiusSmall; border.color: projectNameField.activeFocus ? Theme.borderFocus : Theme.borderNormal; border.width: 1 }
            }

            Label { text: "项目路径"; color: Theme.textPrimary; font.family: Theme.fontFamily }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: projectPathField
                    Layout.fillWidth: true
                    placeholderText: "选择项目目录"
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    background: Rectangle { color: Theme.bgInput; radius: Theme.radiusSmall; border.color: projectPathField.activeFocus ? Theme.borderFocus : Theme.borderNormal; border.width: 1 }
                }
                Button {
                    text: "浏览"
                    font.family: Theme.fontFamily
                    onClicked: folderDialog.open()
                }
            }
        }

        standardButtons: Dialog.Ok | Dialog.Cancel
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

    Dialog {
        id: deleteConfirmDialog
        title: "确认删除"
        modal: true
        anchors.centerIn: parent
        width: 320

        property string projectId: ""

        Label {
            text: "确定要删除此项目吗？此操作不可撤销。"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }

        standardButtons: Dialog.Yes | Dialog.No
        onYes: {
            if (projectId) {
                projectService.deleteProject(projectId)
                projectModel.refresh()
                projectId = ""
            }
        }
        onNo: projectId = ""
    }

    FileDialog {
        id: folderDialog
        title: "选择项目目录"
        currentFolder: "file:///C:/"
        onAccepted: {
            var path = folderDialog.selectedFile.toString()
            if (path.startsWith("file:///")) path = path.substring(8)
            else if (path.startsWith("file://")) path = path.substring(7)
            projectPathField.text = decodeURIComponent(path)
        }
    }
}
