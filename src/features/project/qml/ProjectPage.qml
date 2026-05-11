// ProjectPage.qml - 项目管理页面
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // 标题栏
        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "项目管理"
                font.pixelSize: 24
                font.bold: true
                color: "#cdd6f4"
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "新建项目"
                onClicked: newProjectDialog.open()
            }

            Button {
                text: "刷新"
                onClicked: projectModel.refresh()
            }
        }

        Label {
            text: "创建或打开一个项目开始工作"
            font.pixelSize: 14
            color: "#a6adc8"
        }

        // 项目列表
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: projectModel
            spacing: 8

            delegate: Rectangle {
                width: ListView.view.width
                height: 64
                color: mouseArea.containsMouse ? "#313244" : "#313244"
                radius: 8
                border.color: appController.currentProjectId === model.projectId ? "#89b4fa" : "transparent"
                border.width: appController.currentProjectId === model.projectId ? 2 : 0

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    Column {
                        Layout.fillWidth: true
                        Label { text: model.name; font.bold: true; color: "#cdd6f4"; font.pixelSize: 14 }
                        Label { text: model.path; color: "#6c7086"; font.pixelSize: 11 }
                    }

                    Label { text: model.createdAt; color: "#6c7086"; font.pixelSize: 11 }

                    Button {
                        text: appController.currentProjectId === model.projectId ? "已打开" : "打开"
                        enabled: appController.currentProjectId !== model.projectId
                        onClicked: {
                            projectService.openProject(model.projectId)
                            appController.openProject(model.projectId, model.name)
                            // 加载该项目对应的类别体系
                            var taxonomies = taxonomyService.listTaxonomies(model.projectId)
                            if (taxonomies.length > 0) {
                                taxonomyModel.taxonomyId = taxonomies[0].id
                            }
                        }
                    }

                    Button {
                        text: "删除"
                        onClicked: {
                            projectService.deleteProject(model.projectId)
                            projectModel.refresh()
                        }
                    }
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }
        }
    }

    // 新建项目对话框
    Dialog {
        id: newProjectDialog
        title: "新建项目"
        modal: true
        anchors.centerIn: parent
        width: 400

        ColumnLayout {
            width: parent.width
            spacing: 12

            Label { text: "项目名称"; color: "#cdd6f4" }
            TextField {
                id: projectNameField
                Layout.fillWidth: true
                placeholderText: "输入项目名称"
                color: "#cdd6f4"
                background: Rectangle { color: "#313244"; radius: 4; border.color: projectNameField.activeFocus ? "#89b4fa" : "#45475a"; border.width: 1 }
            }

            Label { text: "项目路径"; color: "#cdd6f4" }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: projectPathField
                    Layout.fillWidth: true
                    placeholderText: "选择项目目录"
                    color: "#cdd6f4"
                    background: Rectangle { color: "#313244"; radius: 4; border.color: projectPathField.activeFocus ? "#89b4fa" : "#45475a"; border.width: 1 }
                }
                Button {
                    text: "浏览"
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
