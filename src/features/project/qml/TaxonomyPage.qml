// TaxonomyPage.qml - 类别体系编辑页面
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingXLarge
        spacing: Theme.spacingLarge

        // 标题栏
        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "类别体系"
                font.pixelSize: Theme.fontSizeTitle
                font.bold: true
                font.family: Theme.fontFamily
                color: Theme.textPrimary
            }

            Item { Layout.fillWidth: true }

            Label {
                text: taxonomyModel.taxonomyId ? "版本: v" + taxonomyService.getTaxonomyVersion(taxonomyModel.taxonomyId) : ""
                font.pixelSize: Theme.fontSizeNormal
                font.family: Theme.fontFamily
                color: Theme.textSecondary
            }
        }

        // 提示
        Label {
            visible: !taxonomyModel.taxonomyId
            text: "请先打开一个项目以管理类别体系"
            font.pixelSize: Theme.fontSizeSubheading
            font.family: Theme.fontFamily
            color: Theme.textSecondary
            Layout.fillWidth: true
        }

        // 类别列表区域
        Rectangle {
            visible: taxonomyModel.taxonomyId
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgCard
            radius: Theme.radiusNormal

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingNormal
                spacing: Theme.spacingNormal

                // 添加类别行
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingNormal

                    TextField {
                        id: newClassField
                        Layout.fillWidth: true
                        placeholderText: "输入类别名称..."
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: newClassField.activeFocus ? Theme.accentPrimary : Theme.borderNormal
                            border.width: 1
                        }

                        onAccepted: addClassBtn.clicked()
                    }

                    Button {
                        id: addClassBtn
                        text: "添加"
                        font.family: Theme.fontFamily
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
                    spacing: Theme.spacingTiny

                    delegate: Rectangle {
                        width: classListView.width
                        height: 40
                        color: mouseArea.containsMouse ? Theme.bgHover : Theme.bgTertiary
                        radius: Theme.radiusSmall

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingNormal
                            anchors.rightMargin: Theme.spacingNormal
                            spacing: Theme.spacingNormal

                            // 类别序号
                            Rectangle {
                                width: 28
                                height: 28
                                radius: Theme.radiusSmall
                                color: Theme.borderNormal

                                Label {
                                    anchors.centerIn: parent
                                    text: model.classIndex
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.family: Theme.fontFamily
                                    color: Theme.textPrimary
                                }
                            }

                            // 类别名称（可内联编辑）
                            Label {
                                id: classLabel
                                Layout.fillWidth: true
                                text: model.className
                                font.pixelSize: Theme.fontSizeNormal
                                font.family: Theme.fontFamily
                                color: Theme.textPrimary
                                visible: !editLoader.active
                            }

                            Loader {
                                id: editLoader
                                active: false
                                Layout.fillWidth: true

                                sourceComponent: TextField {
                                    text: model.className
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeNormal
                                    font.family: Theme.fontFamily
                                    horizontalAlignment: TextInput.AlignLeft

                                    background: Rectangle {
                                        color: Theme.bgInput
                                        radius: Theme.radiusSmall
                                        border.color: Theme.accentPrimary
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
                                font.pixelSize: Theme.fontSizeNormal
                                onClicked: editLoader.active = true

                                background: Rectangle {
                                    color: parent.hovered ? Theme.bgHover : "transparent"
                                    radius: Theme.radiusSmall
                                }
                                contentItem: Label {
                                    text: parent.text
                                    font.pixelSize: parent.font.pixelSize
                                    color: Theme.accentPrimary
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            ToolButton {
                                text: "\u2715"
                                font.pixelSize: Theme.fontSizeNormal
                                onClicked: taxonomyModel.removeClass(model.classIndex)

                                background: Rectangle {
                                    color: parent.hovered ? Theme.bgHover : "transparent"
                                    radius: Theme.radiusSmall
                                }
                                contentItem: Label {
                                    text: parent.text
                                    font.pixelSize: parent.font.pixelSize
                                    color: Theme.accentError
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
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }
            }
        }
    }
}
