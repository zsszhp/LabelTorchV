// TaxonomyPage.qml - 类别体系编辑页面
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme
import LabelTorch.Components

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
                color: Theme.textMain
            }

            Item { Layout.fillWidth: true }

            Label {
                text: taxonomyModel.taxonomyId ? "版本: v" + taxonomyService.getTaxonomyVersion(taxonomyModel.taxonomyId) : ""
                font.pixelSize: Theme.fontSizeNormal
                font.family: Theme.fontFamily
                color: Theme.textMuted
            }
        }

        // 提示
        Label {
            visible: !taxonomyModel.taxonomyId
            text: "请先打开一个项目以管理类别体系"
            font.pixelSize: Theme.fontSizeSubheading
            font.family: Theme.fontFamily
            color: Theme.textMuted
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
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: newClassField.activeFocus ? Theme.primary : Theme.borderColor
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
                        id: classRow
                        // 每行只查一次样式（原绑定每行重复调用 3-4 次 C++）
                        readonly property var classStyle: taxonomyModel.getClassStyle(model.classIndex)
                        width: classListView.width
                        height: 40
                        color: mouseArea.containsMouse ? Theme.bgHover : Theme.bgCard
                        radius: Theme.radiusSmall

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onDoubleClicked: editLoader.active = true
                        }

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
                                color: Theme.borderColor

                                Label {
                                    anchors.centerIn: parent
                                    text: model.classIndex
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.family: Theme.fontFamily
                                    color: Theme.textMain
                                }
                            }

                            // 类别名称（可内联编辑；已废弃槽位只读）
                            Label {
                                id: classLabel
                                Layout.fillWidth: true
                                // 空名为废弃占位：class_id 仍占用，显示「已废弃」
                                text: model.className === "" ? "已废弃" : model.className
                                font.pixelSize: Theme.fontSizeNormal
                                font.family: Theme.fontFamily
                                color: model.className === "" ? Theme.textMuted : Theme.textMain
                                visible: !editLoader.active
                            }

                            Loader {
                                id: editLoader
                                active: false
                                Layout.fillWidth: true

                                sourceComponent: TextField {
                                    // 已废弃槽位可改名复活，初始文本保留原空名
                                    text: model.className
                                    color: Theme.textMain
                                    font.pixelSize: Theme.fontSizeNormal
                                    font.family: Theme.fontFamily
                                    horizontalAlignment: TextInput.AlignLeft

                                    background: Rectangle {
                                        color: Theme.bgInput
                                        radius: Theme.radiusSmall
                                        border.color: Theme.primary
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

                            // 类别样式：颜色圆点 + 快捷键角标（点击颜色圆点编辑样式）
                            Rectangle {
                                id: styleDot
                                width: 20
                                height: 20
                                radius: 10
                                // 已存颜色优先，否则用主题类别配色
                                color: classRow.classStyle.color !== ""
                                       ? classRow.classStyle.color
                                       : Theme.classColors[model.classIndex % Theme.classColors.length]
                                border.color: Theme.borderColor
                                border.width: 1

                                // 快捷键角标（右下角小字）
                                Label {
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.rightMargin: -2
                                    anchors.bottomMargin: -2
                                    text: classRow.classStyle.shortcut !== ""
                                          ? classRow.classStyle.shortcut : ""
                                    font.pixelSize: 8
                                    font.bold: true
                                    color: Theme.bgMain
                                    background: Rectangle {
                                        color: Theme.warning
                                        radius: 2
                                        visible: parent.text !== ""
                                        width: parent.implicitWidth + 4
                                        height: parent.implicitHeight
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: styleEditor.openFor(model.classIndex)
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
                                    color: Theme.primary
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            ToolButton {
                                text: "\u2715"
                                font.pixelSize: Theme.fontSizeNormal
                                // 已废弃槽位无需再删（物理删除另走带引用检查的流程）
                                visible: model.className !== ""
                                onClicked: taxonomyModel.removeClass(model.classIndex)

                                background: Rectangle {
                                    color: parent.hovered ? Theme.bgHover : "transparent"
                                    radius: Theme.radiusSmall
                                }
                                contentItem: Label {
                                    text: parent.text
                                    font.pixelSize: parent.font.pixelSize
                                    color: Theme.danger
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }
                }

                // 空态：已打开项目但当前体系还没有任何类别
                EmptyState {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: taxonomyModel.taxonomyId !== "" && classListView.count === 0
                    icon: "marker"
                    title: "暂无类别"
                    description: "在上方输入类别名称并回车，为当前项目添加第一个类别"
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

        // === 类别样式编辑弹窗（颜色 + 快捷键，持久化到 taxonomy_class_styles） ===
        Dialog {
            id: styleEditor
            modal: true
            anchors.centerIn: parent
            width: 360
            standardButtons: Dialog.NoButton

            property int classIndex: -1
            property color selectedColor: Theme.classColors[0]
            property string shortcutText: ""
            property string className: ""

            function openFor(index) {
                classIndex = index
                className = taxonomyModel.data(taxonomyModel.index(index, 0), Qt.UserRole + 1)
                var style = taxonomyModel.getClassStyle(index)
                selectedColor = style.color !== "" ? style.color : Theme.classColors[index % Theme.classColors.length]
                shortcutText = style.shortcut
                open()
            }

            background: Rectangle {
                color: Theme.bgCard
                border.color: Theme.borderColor
                radius: Theme.radiusLarge
            }

            ColumnLayout {
                width: parent.width - Theme.spacingXLarge * 2
                x: Theme.spacingXLarge
                y: Theme.spacingXLarge
                spacing: Theme.spacingLarge

                Text {
                    text: "编辑类别样式：" + styleEditor.className
                    font.pixelSize: Theme.fontSizeSubheading
                    font.bold: true
                    color: Theme.textMain
                }

                // 颜色选择：预设色板 + 当前色预览
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSmall

                    Text {
                        text: "颜色"
                        font.pixelSize: Theme.fontSizeSmall
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
                                border.color: styleEditor.selectedColor.toUpperCase() === modelData.toUpperCase()
                                              ? Theme.textMain : Theme.borderColor
                                border.width: styleEditor.selectedColor.toUpperCase() === modelData.toUpperCase() ? 2 : 1

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: styleEditor.selectedColor = modelData
                                }
                            }
                        }
                    }
                }

                // 快捷键输入（单字符）
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSmall

                    Text {
                        text: "快捷键"
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.textMuted
                    }

                    TextField {
                        id: shortcutField
                        Layout.preferredWidth: 60
                        Layout.preferredHeight: 30
                        text: styleEditor.shortcutText
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
                        color: Theme.textMuted
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingNormal

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "取消"
                        onClicked: styleEditor.close()
                        background: Rectangle {
                            color: parent.hovered ? Theme.bgHover : Theme.bgCard
                            border.color: Theme.borderColor
                            radius: Theme.radiusSmall
                        }
                        contentItem: Label { text: parent.text; color: Theme.textMain; horizontalAlignment: Text.AlignHCenter }
                    }

                    Button {
                        text: "保存"
                        highlighted: true
                        onClicked: {
                            taxonomyModel.setClassStyle(styleEditor.classIndex,
                                                        styleEditor.selectedColor.toString(),
                                                        shortcutField.text.trim())
                            styleEditor.close()
                        }
                        background: Rectangle {
                            color: parent.hovered ? Theme.primaryGlow : Theme.primary
                            radius: Theme.radiusSmall
                        }
                        contentItem: Label { text: parent.text; color: "#FFFFFF"; horizontalAlignment: Text.AlignHCenter }
                    }
                }
            }
        }
    }
}
