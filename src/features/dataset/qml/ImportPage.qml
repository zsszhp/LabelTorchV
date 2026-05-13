// ImportPage.qml - YOLO txt 数据导入页面
import QtQuick
import QtQuick.Controls
import LabelTorch.Shell
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // 标题栏
        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "数据导入"
                font.pixelSize: 24
                font.bold: true
                color: Theme.textPrimary
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "刷新"
                onClicked: datasetModel.refresh()
            }
        }

        // 提示
        Label {
            visible: !appController.projectOpen
            text: "请先打开一个项目再导入数据"
            font.pixelSize: 14
            color: Theme.accentError
            Layout.fillWidth: true
        }

        // 导入表单
        Rectangle {
            visible: appController.projectOpen
            Layout.fillWidth: true
            Layout.preferredHeight: 280
            color: Theme.bgCard
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Label {
                    text: "YOLO txt 格式导入"
                    font.pixelSize: 16
                    font.bold: true
                    color: Theme.textPrimary
                }

                // 数据集名称
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label { text: "数据集名称"; color: Theme.textPrimary; Layout.preferredWidth: 80 }
                    TextField {
                        id: datasetNameField
                        Layout.fillWidth: true
                        placeholderText: "输入数据集名称"
                        color: Theme.textPrimary
                        background: Rectangle { color: Theme.bgInput; radius: 4; border.color: datasetNameField.activeFocus ? Theme.accentPrimary : Theme.borderNormal; border.width: 1 }
                    }
                }

                // 图片目录
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label { text: "图片目录"; color: Theme.textPrimary; Layout.preferredWidth: 80 }
                    TextField {
                        id: imageDirField
                        Layout.fillWidth: true
                        placeholderText: "选择图片目录 (images/)"
                        color: Theme.textPrimary
                        background: Rectangle { color: Theme.bgInput; radius: 4; border.color: imageDirField.activeFocus ? Theme.accentPrimary : Theme.borderNormal; border.width: 1 }
                    }
                    Button {
                        text: "浏览"
                        onClicked: imageFolderDialog.open()
                    }
                }

                // 标签目录
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label { text: "标签目录"; color: Theme.textPrimary; Layout.preferredWidth: 80 }
                    TextField {
                        id: labelDirField
                        Layout.fillWidth: true
                        placeholderText: "选择标签目录 (labels/)"
                        color: Theme.textPrimary
                        background: Rectangle { color: Theme.bgInput; radius: 4; border.color: labelDirField.activeFocus ? Theme.accentPrimary : Theme.borderNormal; border.width: 1 }
                    }
                    Button {
                        text: "浏览"
                        onClicked: labelFolderDialog.open()
                    }
                }

                // 导入按钮
                Button {
                    text: "开始导入"
                    highlighted: true
                    enabled: datasetNameField.text.trim() && imageDirField.text.trim() && labelDirField.text.trim()
                    onClicked: {
                        var dsId = datasetService.importDataset(
                            appController.currentProjectId,
                            datasetNameField.text.trim(),
                            imageDirField.text.trim(),
                            labelDirField.text.trim()
                        )
                        if (dsId) {
                            datasetModel.refresh()
                            datasetNameField.clear()
                            imageDirField.clear()
                            labelDirField.clear()
                        }
                    }
                }
            }
        }

        // 已导入数据集列表
        Label {
            visible: appController.projectOpen
            text: "已导入数据集"
            font.pixelSize: 16
            font.bold: true
            color: Theme.textPrimary
        }

        ListView {
            visible: appController.projectOpen
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: datasetModel
            spacing: 8

            delegate: Rectangle {
                width: ListView.view.width
                height: 72
                color: Theme.bgInput
                radius: 8

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    Column {
                        Layout.fillWidth: true
                        Label { text: model.name; font.bold: true; color: Theme.textPrimary; font.pixelSize: 14 }
                        Label { text: model.imageRoot; color: Theme.textMuted; font.pixelSize: 11; elide: Text.ElideMiddle; width: parent.width }
                    }

                    Rectangle {
                        width: 12; height: 12; radius: 6
                        color: model.importStatus === "completed" ? Theme.accentSuccess :
                               model.importStatus === "failed" ? Theme.accentError : Theme.accentWarning
                    }

                    Label { text: model.sampleCount + " 张"; color: Theme.textSecondary; font.pixelSize: 12 }
                    Label { text: model.importStatus; color: Theme.textMuted; font.pixelSize: 11 }

                    Button {
                        text: "删除"
                        onClicked: {
                            datasetService.deleteDataset(model.datasetId)
                            datasetModel.refresh()
                        }
                    }
                }
            }
        }
    }

    FolderDialog {
        id: imageFolderDialog
        onSelectedFolderChanged: imageDirField.text = selectedFolder
    }

    FolderDialog {
        id: labelFolderDialog
        onSelectedFolderChanged: labelDirField.text = selectedFolder
    }
}
