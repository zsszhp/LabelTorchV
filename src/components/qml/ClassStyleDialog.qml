// ClassStyleDialog.qml - 类别样式编辑（名称/颜色/快捷键一体），行内 ✎ 按钮唤起
// 持久化：名称走 taxonomies.class_definitions_json；颜色/快捷键走 taxonomy_class_styles
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme

Dialog {
    id: root

    modal: true
    anchors.centerIn: parent
    width: 380
    standardButtons: Dialog.NoButton

    // 目标类别索引（class_id）
    property int classIndex: -1
    property color selectedColor: Theme.classColors[0]
    property string className: ""

    function openFor(index) {
        classIndex = index
        className = taxonomyModel.data(taxonomyModel.index(index, 0), Qt.UserRole + 1) || ("class_" + index)
        var style = taxonomyModel.getClassStyle(index)
        selectedColor = style.color !== "" ? style.color : Theme.classColors[index % Theme.classColors.length]
        shortcutField.text = style.shortcut
        nameField.text = className
        open()
    }

    background: Rectangle {
        color: Theme.bgCard
        border.color: Theme.borderColor
        border.width: 1
        radius: Theme.radiusLarge
    }

    ColumnLayout {
        width: parent.width - Theme.spacingXLarge * 2
        x: Theme.spacingXLarge
        y: Theme.spacingXLarge
        spacing: Theme.spacingLarge

        Text {
            text: "编辑类别：" + root.className
            font.pixelSize: Theme.fontSizeSubheading
            font.bold: true
            font.family: Theme.fontFamily
            color: Theme.textMain
        }

        // 名称
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSmall

            Text {
                text: "名称"
                font.pixelSize: Theme.fontSizeSmall
                font.family: Theme.fontFamily
                color: Theme.textMuted
            }

            TextField {
                id: nameField
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                color: Theme.textMain
                font.pixelSize: Theme.fontSizeNormal
                font.family: Theme.fontFamily

                background: Rectangle {
                    color: Theme.bgInput
                    radius: Theme.radiusSmall
                    border.color: nameField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                    border.width: 1
                }
            }
        }

        // 颜色
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSmall

            Text {
                text: "颜色"
                font.pixelSize: Theme.fontSizeSmall
                font.family: Theme.fontFamily
                color: Theme.textMuted
            }

            Row {
                spacing: Theme.spacingSmall
                Repeater {
                    model: Theme.classColors
                    Rectangle {
                        width: 26
                        height: 26
                        radius: 6
                        color: modelData
                        border.color: root.selectedColor.toString().toUpperCase() === modelData.toUpperCase()
                                      ? Theme.textMain : Theme.borderColor
                        border.width: root.selectedColor.toString().toUpperCase() === modelData.toUpperCase() ? 2 : 1

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectedColor = modelData
                        }
                    }
                }
            }
        }

        // 快捷键
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSmall

            Text {
                text: "快捷键"
                font.pixelSize: Theme.fontSizeSmall
                font.family: Theme.fontFamily
                color: Theme.textMuted
            }

            TextField {
                id: shortcutField
                Layout.preferredWidth: 60
                Layout.preferredHeight: 30
                maximumLength: 1
                color: Theme.textMain
                font.pixelSize: Theme.fontSizeNormal
                horizontalAlignment: TextInput.AlignHCenter

                background: Rectangle {
                    color: Theme.bgInput
                    radius: Theme.radiusSmall
                    border.color: shortcutField.activeFocus ? Theme.primaryGlow : Theme.borderColor
                    border.width: 1
                }
            }

            Text {
                text: "标注页按该数字键可快速切换到此类别"
                font.pixelSize: Theme.fontSizeCaption
                font.family: Theme.fontFamily
                color: Theme.textMuted
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }

        // 底部按钮
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingNormal

            Item { Layout.fillWidth: true }

            Button {
                text: "取消"
                onClicked: root.close()

                background: Rectangle {
                    color: parent.hovered ? Theme.bgHover : Theme.bgCard
                    border.color: Theme.borderColor
                    radius: Theme.radiusSmall
                }
                contentItem: Label {
                    text: parent.text
                    color: Theme.textMain
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                text: "保存"
                onClicked: {
                    var name = nameField.text.trim()
                    if (name !== "" && name !== root.className)
                        taxonomyModel.renameClass(root.classIndex, name)
                    taxonomyModel.setClassStyle(root.classIndex,
                                                root.selectedColor.toString(),
                                                shortcutField.text.trim())
                    root.close()
                }

                background: Rectangle {
                    color: parent.hovered ? Theme.primaryGlow : Theme.primary
                    radius: Theme.radiusSmall
                }
                contentItem: Label {
                    text: parent.text
                    color: "#FFFFFF"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
