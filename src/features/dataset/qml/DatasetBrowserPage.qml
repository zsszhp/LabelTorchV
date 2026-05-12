import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Shell

Rectangle {
    id: root
    color: Theme.bgPrimary

    property string currentDatasetId: ""
    property string currentDatasetName: ""
    property int totalSamples: 0
    property int currentPage: 0
    property int pageSize: 60

    function loadDataset(datasetId, datasetName) {
        root.currentDatasetId = datasetId
        root.currentDatasetName = datasetName
        root.currentPage = 0
        root.totalSamples = datasetService.getSampleCount(datasetId)
        refreshSamples()
    }

    function refreshSamples() {
        if (!root.currentDatasetId) return
        var offset = currentPage * pageSize
        var samples = datasetService.listSamples(root.currentDatasetId, offset, pageSize)
        sampleModel.clear()
        for (var i = 0; i < samples.length; i++) {
            sampleModel.append(samples[i])
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: Theme.bgSecondary

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLarge
                anchors.rightMargin: Theme.spacingLarge

                Label {
                    text: currentDatasetId ? "数据集: " + currentDatasetName : "选择一个数据集浏览"
                    font.pixelSize: Theme.fontSizeNormal
                    font.bold: true
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                }

                Item { Layout.fillWidth: true }

                Label {
                    visible: currentDatasetId
                    text: "共 " + totalSamples + " 个样本"
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                }

                Button {
                    visible: currentDatasetId
                    text: "刷新"
                    flat: true
                    font.pixelSize: Theme.fontSizeSmall
                    palette.buttonText: Theme.accentPrimary
                    onClicked: refreshSamples()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                color: Theme.bgSecondary
                visible: appController.projectOpen

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Label {
                        Layout.fillWidth: true
                        text: "数据集列表"
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        leftPadding: Theme.spacingNormal
                        topPadding: Theme.spacingNormal
                    }

                    ListView {
                        id: datasetListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: datasetModel
                        delegate: ItemDelegate {
                            width: datasetListView.width
                            height: 44
                            highlighted: model.datasetId === root.currentDatasetId

                            contentItem: Column {
                                spacing: 2
                                leftPadding: 12
                                Label {
                                    text: model.name
                                    font.pixelSize: Theme.fontSizeNormal
                                    color: highlighted ? Theme.accentPrimary : Theme.textPrimary
                                    font.family: Theme.fontFamily
                                }
                                Label {
                                    text: model.sampleCount + " 样本"
                                    font.pixelSize: Theme.fontSizeSmall
                                    color: Theme.textMuted
                                    font.family: Theme.fontFamily
                                }
                            }

                            background: Rectangle {
                                color: highlighted ? Theme.bgSelected : (parent.hovered ? Theme.bgPrimary : "transparent")
                            }

                            onClicked: root.loadDataset(model.datasetId, model.name)
                        }

                        Connections {
                            target: appController
                            function onCurrentProjectIdChanged() {
                                if (appController.projectOpen) {
                                    datasetModel.projectId = appController.currentProjectId
                                    datasetModel.refresh()
                                }
                            }
                        }

                        Component.onCompleted: {
                            if (appController.projectOpen) {
                                datasetModel.projectId = appController.currentProjectId
                                datasetModel.refresh()
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.bgPrimary

                GridView {
                    id: sampleGrid
                    anchors.fill: parent
                    anchors.margins: Theme.spacingNormal
                    visible: currentDatasetId
                    clip: true
                    cellWidth: 140
                    cellHeight: 140

                    model: ListModel {
                        id: sampleModel
                    }

                    delegate: Rectangle {
                        width: sampleGrid.cellWidth - 4
                        height: sampleGrid.cellHeight - 4
                        color: Theme.bgCard
                        radius: Theme.radiusSmall
                        border.color: Theme.borderNormal
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.margins: 4
                            spacing: 2

                            Image {
                                width: parent.width - 8
                                height: parent.height - 30
                                fillMode: Image.PreserveAspectCrop
                                source: model.imagePath || ""
                                cache: true
                                asynchronous: true

                                Rectangle {
                                    anchors.fill: parent
                                    visible: parent.status === Image.Error || parent.status === Image.Null
                                    color: Theme.bgTertiary

                                    Label {
                                        anchors.centerIn: parent
                                        text: "无预览"
                                        font.pixelSize: Theme.fontSizeSmall
                                        color: Theme.textMuted
                                        font.family: Theme.fontFamily
                                    }
                                }
                            }

                            Label {
                                width: parent.width - 8
                                text: {
                                    var path = model.imagePath || ""
                                    var parts = path.split("/")
                                    return parts.length > 0 ? parts[parts.length - 1] : ""
                                }
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                elide: Text.ElideRight
                            }

                            Label {
                                width: parent.width - 8
                                text: model.validationStatus === "valid" ? "有效" : "异常"
                                font.pixelSize: Theme.fontSizeSmall
                                color: model.validationStatus === "valid" ? Theme.accentSuccess : Theme.accentError
                                font.family: Theme.fontFamily
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onDoubleClicked: {
                                console.log("Open sample:", model.id)
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar { active: true }
                }

                Label {
                    anchors.centerIn: parent
                    visible: !currentDatasetId
                    text: "请从左侧选择一个数据集"
                    font.pixelSize: Theme.fontSizeLarge
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: Theme.bgSecondary
            visible: currentDatasetId

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLarge
                anchors.rightMargin: Theme.spacingLarge

                Label {
                    text: "第 " + (currentPage + 1) + " 页"
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: "上一页"
                    flat: true
                    enabled: currentPage > 0
                    font.pixelSize: Theme.fontSizeSmall
                    palette.buttonText: Theme.accentPrimary
                    onClicked: {
                        currentPage--
                        refreshSamples()
                    }
                }

                Button {
                    text: "下一页"
                    flat: true
                    enabled: (currentPage + 1) * pageSize < totalSamples
                    font.pixelSize: Theme.fontSizeSmall
                    palette.buttonText: Theme.accentPrimary
                    onClicked: {
                        currentPage++
                        refreshSamples()
                    }
                }
            }
        }
    }
}
