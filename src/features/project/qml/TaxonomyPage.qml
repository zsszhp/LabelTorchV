// TaxonomyPage.qml - 类别体系编辑页面
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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
                text: "类别体系"
                font.pixelSize: 24
                font.bold: true
                color: "#cdd6f4"
            }

            Item { Layout.fillWidth: true }

            Label {
                text: taxonomyModel.taxonomyId ? "版本: v" + taxonomyService.getTaxonomyVersion(taxonomyModel.taxonomyId) : ""
                font.pixelSize: 13
                color: "#a6adc8"
            }
        }

        // 提示
        Label {
            visible: !taxonomyModel.taxonomyId
            text: "请先打开一个项目以管理类别体系"
            font.pixelSize: 14
            color: "#a6adc8"
            Layout.fillWidth: true
        }

        // 类别列表区域
        Rectangle {
            visible: taxonomyModel.taxonomyId
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#181825"
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // 添加类别行
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    TextField {
                        id: newClassField
                        Layout.fillWidth: true
                        placeholderText: "输入类别名称..."
                        color: "#cdd6f4"
                        font.pixelSize: 14

                        background: Rectangle {
                            color: "#313244"
                            radius: 4
                            border.color: newClassField.activeFocus ? "#89b4fa" : "#45475a"
                            border.width: 1
                        }

                        onAccepted: addClassBtn.clicked()
                    }

                    Button {
                        id: addClassBtn
                        text: "添加"
                        onClicked: {
                            if (newClassField.text.trim()) {
                                taxonomyModel.addClass(newClassField.text.trim())
                                newClassField.clear()
                            }
                        }
                    }
                }

                // 类别列表
                ListView {
                    id: classListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: taxonomyModel
                    spacing: 4

                    delegate: Rectangle {
                        width: classListView.width
                        height: 40
                        color: mouseArea.containsMouse ? "#313244" : "#1e1e2e"
                        radius: 4

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 8

                            // 类别序号
                            Rectangle {
                                width: 28
                                height: 28
                                radius: 4
                                color: "#45475a"

                                Label {
                                    anchors.centerIn: parent
                                    text: model.classIndex
                                    font.pixelSize: 12
                                    color: "#cdd6f4"
                                }
                            }

                            // 类别名称（可内联编辑）
                            Label {
                                id: classLabel
                                Layout.fillWidth: true
                                text: model.className
                                font.pixelSize: 14
                                color: "#cdd6f4"
                                visible: !editLoader.active
                            }

                            Loader {
                                id: editLoader
                                active: false
                                Layout.fillWidth: true

                                sourceComponent: TextField {
                                    text: model.className
                                    color: "#cdd6f4"
                                    font.pixelSize: 14
                                    horizontalAlignment: TextInput.AlignLeft

                                    background: Rectangle {
                                        color: "#313244"
                                        radius: 4
                                        border.color: "#89b4fa"
                                        border.width: 1
                                    }

                                    onAccepted: {
                                        taxonomyModel.renameClass(model.classIndex, text)
                                        editLoader.active = false
                                    }
                                    onActiveFocusChanged: {
                                        if (!activeFocus) editLoader.active = false
                                    }

                                    Component.onCompleted: forceActiveFocus()
                                }
                            }

                            ToolButton {
                                text: "\u270F"
                                font.pixelSize: 14
                                onClicked: editLoader.active = true

                                background: Rectangle {
                                    color: parent.hovered ? "#45475a" : "transparent"
                                    radius: 4
                                }
                                contentItem: Label {
                                    text: parent.text
                                    font.pixelSize: parent.font.pixelSize
                                    color: "#89b4fa"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            ToolButton {
                                text: "\u2715"
                                font.pixelSize: 14
                                onClicked: taxonomyModel.removeClass(model.classIndex)

                                background: Rectangle {
                                    color: parent.hovered ? "#45475a" : "transparent"
                                    radius: 4
                                }
                                contentItem: Label {
                                    text: parent.text
                                    font.pixelSize: parent.font.pixelSize
                                    color: "#f38ba8"
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onDoubleClicked: editLoader.active = true
                        }
                    }
                }

                // 底部统计
                Label {
                    Layout.fillWidth: true
                    text: "共 " + taxonomyModel.rowCount() + " 个类别"
                    font.pixelSize: 12
                    color: "#6c7086"
                }
            }
        }
    }
}
