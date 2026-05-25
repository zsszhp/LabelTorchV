// ImportPage.qml - 数据导入页面（支持 YOLO txt 和 COCO JSON 格式）
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
            Layout.preferredHeight: 340
            color: Theme.bgCard
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                // 格式选择
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label { text: "标签格式"; color: Theme.textPrimary; font.pixelSize: 13; Layout.preferredWidth: 80 }
                    ComboBox {
                        id: formatCombo
                        model: ["YOLO TXT", "COCO JSON"]
                        currentIndex: 0
                        Layout.fillWidth: true

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
                            implicitHeight: contentItem.implicitHeight
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
                                text: modelData
                                color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                font.pixelSize: 13
                                verticalAlignment: Text.AlignVCenter
                            }
                            highlighted: formatCombo.highlightedIndex === index
                            background: Rectangle {
                                color: highlighted ? Theme.bgInput : Theme.bgPrimary
                            }
                        }
                    }
                }

                // 数据集名称
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label { text: "数据集名称"; color: Theme.textPrimary; font.pixelSize: 13; Layout.preferredWidth: 80 }
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
                    Label { text: "图片目录"; color: Theme.textPrimary; font.pixelSize: 13; Layout.preferredWidth: 80 }
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

                // 标签目录（YOLO TXT 模式）
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: formatCombo.currentIndex === 0
                    Label { text: "标签目录"; color: Theme.textPrimary; font.pixelSize: 13; Layout.preferredWidth: 80 }
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

                // JSON 标签文件（COCO JSON 模式）
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: formatCombo.currentIndex === 1
                    Label { text: "JSON文件"; color: Theme.textPrimary; font.pixelSize: 13; Layout.preferredWidth: 80 }
                    TextField {
                        id: jsonLabelField
                        Layout.fillWidth: true
                        placeholderText: "选择 COCO JSON 标签文件"
                        color: Theme.textPrimary
                        background: Rectangle { color: Theme.bgInput; radius: 4; border.color: jsonLabelField.activeFocus ? Theme.accentPrimary : Theme.borderNormal; border.width: 1 }
                    }
                    Button {
                        text: "浏览"
                        onClicked: jsonFileDialog.open()
                    }
                }

                // 导入按钮
                Button {
                    text: "开始导入"
                    highlighted: true
                    enabled: {
                        if (!datasetNameField.text.trim() || !imageDirField.text.trim()) return false
                        if (formatCombo.currentIndex === 0 && !labelDirField.text.trim()) return false
                        if (formatCombo.currentIndex === 1 && !jsonLabelField.text.trim()) return false
                        return true
                    }
                    onClicked: {
                        var dsId = ""
                        if (formatCombo.currentIndex === 0) {
                            // YOLO TXT 格式
                            dsId = datasetService.importDataset(
                                appController.currentProjectId,
                                datasetNameField.text.trim(),
                                imageDirField.text.trim(),
                                labelDirField.text.trim()
                            )
                        } else {
                            // COCO JSON 格式
                            dsId = datasetService.importDatasetJson(
                                appController.currentProjectId,
                                datasetNameField.text.trim(),
                                imageDirField.text.trim(),
                                jsonLabelField.text.trim()
                            )
                        }
                        if (dsId) {
                            datasetModel.refresh()
                            datasetNameField.clear()
                            imageDirField.clear()
                            labelDirField.clear()
                            jsonLabelField.clear()
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

    // 图片目录选择对话框
    FolderDialog {
        id: imageFolderDialog
        onAccepted: {
            var path = selectedFolder.toString()
            if (path.startsWith("file:///")) path = path.substring(8)
            else if (path.startsWith("file://")) path = path.substring(7)
            imageDirField.text = decodeURIComponent(path)
        }
    }

    // 标签目录选择对话框（YOLO TXT 模式）
    FolderDialog {
        id: labelFolderDialog
        onAccepted: {
            var path = selectedFolder.toString()
            if (path.startsWith("file:///")) path = path.substring(8)
            else if (path.startsWith("file://")) path = path.substring(7)
            labelDirField.text = decodeURIComponent(path)
        }
    }

    // JSON 标签文件选择对话框（COCO JSON 模式）
    FileDialog {
        id: jsonFileDialog
        title: "选择 COCO JSON 标签文件"
        nameFilters: ["JSON 文件 (*.json)", "所有文件 (*)"]
        onAccepted: {
            var path = selectedFile.toString()
            if (path.startsWith("file:///")) path = path.substring(8)
            else if (path.startsWith("file://")) path = path.substring(7)
            jsonLabelField.text = decodeURIComponent(path)
        }
    }
}
