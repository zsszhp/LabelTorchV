import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Shell

Rectangle {
    id: root
    color: Theme.bgPrimary

    property string datasetId: ""
    property string taxonomyId: ""
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
                id: delegateRoot
                width: classListView.width
                height: 48
                color: index % 2 === 0 ? Theme.bgPrimary : Theme.bgSecondary

                property bool editing: false

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
                        visible: !delegateRoot.editing
                        text: model.className || ("class_" + model.classId)
                        font.pixelSize: Theme.fontSizeNormal
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                    }

                    TextField {
                        id: nameEdit
                        visible: delegateRoot.editing
                        Layout.fillWidth: true
                        text: model.className || ("class_" + model.classId)
                        font.pixelSize: Theme.fontSizeNormal
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        background: Rectangle { color: Theme.bgInput; radius: Theme.radiusSmall; border.color: Theme.accentPrimary; border.width: 1 }
                        onAccepted: {
                            if (taxonomyId) {
                                datasetService.updateClassName(taxonomyId, model.classId, text)
                            }
                            classListModel.setProperty(index, "className", text)
                            delegateRoot.editing = false
                        }
                        onActiveFocusChanged: {
                            if (!activeFocus && delegateRoot.editing) {
                                delegateRoot.editing = false
                            }
                        }
                        Keys.onEscapePressed: {
                            delegateRoot.editing = false
                        }
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

                    Button {
                        visible: !delegateRoot.editing
                        text: "✏"
                        flat: true
                        font.pixelSize: Theme.fontSizeSmall
                        palette.buttonText: Theme.textMuted
                        onClicked: {
                            delegateRoot.editing = true
                            nameEdit.forceActiveFocus()
                            nameEdit.selectAll()
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onDoubleClicked: {
                        delegateRoot.editing = true
                        nameEdit.forceActiveFocus()
                        nameEdit.selectAll()
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
