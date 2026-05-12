import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Shell

Rectangle {
    id: root
    color: Theme.bgPrimary

    property string datasetId: ""
    property var classDist: []

    function refresh() {
        if (!datasetId) return
        classDist = datasetService.getClassDistribution(datasetId)
        classListModel.clear()
        for (var i = 0; i < classDist.length; i++) {
            classListModel.append(classDist[i])
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
                    text: "类别管理"
                    font.pixelSize: Theme.fontSizeNormal
                    font.bold: true
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: "刷新"
                    flat: true
                    font.pixelSize: Theme.fontSizeSmall
                    palette.buttonText: Theme.accentPrimary
                    onClicked: refresh()
                }
            }
        }

        ListView {
            id: classListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: ListModel {
                id: classListModel
            }

            delegate: Rectangle {
                width: classListView.width
                height: 48
                color: index % 2 === 0 ? Theme.bgPrimary : Theme.bgSecondary

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingLarge
                    anchors.rightMargin: Theme.spacingLarge

                    Rectangle {
                        width: 16
                        height: 16
                        radius: 3
                        color: Theme.classColor(model.classId)
                    }

                    Label {
                        text: model.className || ("class_" + model.classId)
                        font.pixelSize: Theme.fontSizeNormal
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                    }

                    Label {
                        text: "ID: " + model.classId
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: model.count + " 个标注"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                    }
                }
            }

            ScrollBar.vertical: ScrollBar { active: true }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: Theme.bgSecondary

            Label {
                anchors.centerIn: parent
                text: classListModel.count + " 个类别"
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.textMuted
                font.family: Theme.fontFamily
            }
        }
    }

    onDatasetIdChanged: refresh()
}
