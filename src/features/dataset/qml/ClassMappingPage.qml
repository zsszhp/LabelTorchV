// ClassMappingPage.qml - 类别映射页面
import QtQuick
import QtQuick.Controls
import LabelTorch.Shell
import QtQuick.Layouts

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "类别映射"
                font.pixelSize: 24
                font.bold: true
                color: Theme.textPrimary
            }

            Item { Layout.fillWidth: true }

            Label {
                visible: !appController.projectOpen
                text: "请先打开项目"
                color: Theme.accentError
                font.pixelSize: 14
            }
        }

        // 映射工作区
        Rectangle {
            visible: appController.projectOpen
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgCard
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                // 数据集选择
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label { text: "数据集"; color: Theme.textPrimary; Layout.preferredWidth: 60 }
                    ComboBox {
                        id: datasetCombo
                        Layout.fillWidth: true
                        model: datasetModel
                        textRole: "name"
                        valueRole: "datasetId"
                        onActivated: {
                            var schema = classMappingService.getSourceSchema(currentValue)
                            sourceSchemaText.text = schema.rawClassNamesJson || "无"
                        }

                        background: Rectangle { color: Theme.bgInput; radius: 4; border.color: Theme.borderNormal }
                        contentItem: Label { text: datasetCombo.displayText; color: Theme.textPrimary; font.pixelSize: 14 }
                    }
                }

                // 源类别列表
                Label { text: "源类别 (来自导入标签)"; color: Theme.textSecondary; font.pixelSize: 13 }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    color: Theme.bgPrimary
                    radius: 4

                    ScrollView {
                        anchors.fill: parent
                        TextArea {
                            id: sourceSchemaText
                            text: "选择数据集查看源类别"
                            color: Theme.textPrimary
                            font.pixelSize: 12
                            font.family: "Consolas"
                            readOnly: true
                            wrapMode: TextArea.Wrap
                            background: Rectangle { color: "transparent" }
                        }
                    }
                }

                // 目标类别体系
                Label { text: "目标类别体系 (项目类别)"; color: Theme.textSecondary; font.pixelSize: 13 }

                Label {
                    text: taxonomyModel.taxonomyId ? "当前 " + taxonomyModel.rowCount() + " 个类别" : "未选择类别体系"
                    color: Theme.accentPrimary
                    font.pixelSize: 12
                }

                // 映射规则（简化版：文本输入格式 源:目标 每行一个）
                Label { text: "映射规则 (格式: 源类别名:目标类别名, 每行一个)"; color: Theme.textSecondary; font.pixelSize: 13 }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    color: Theme.bgPrimary
                    radius: 4

                    ScrollView {
                        anchors.fill: parent
                        TextArea {
                            id: mappingRulesText
                            placeholderText: "class_0:defect_a\nclass_1:defect_b"
                            color: Theme.textPrimary
                            font.pixelSize: 12
                            font.family: "Consolas"
                            wrapMode: TextArea.Wrap
                            background: Rectangle { color: "transparent" }
                        }
                    }
                }

                // 操作按钮
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Button {
                        text: "预览映射"
                        enabled: datasetCombo.currentValue && mappingRulesText.text.trim()
                        onClicked: {
                            var rules = parseMappingRules(mappingRulesText.text)
                            var preview = classMappingService.previewMapping(datasetCombo.currentValue, rules)
                            previewText.text = "总标签数: " + preview.totalLabels + "\n受影响: " + preview.affectedLabels + "\n新分布: " + JSON.stringify(preview.newClassDistribution)
                        }
                    }

                    Button {
                        text: "执行映射"
                        highlighted: true
                        enabled: datasetCombo.currentValue && mappingRulesText.text.trim()
                        onClicked: {
                            var rules = parseMappingRules(mappingRulesText.text)
                            var schema = classMappingService.getSourceSchema(datasetCombo.currentValue)
                            var revId = classMappingService.createMapping(
                                datasetCombo.currentValue,
                                schema.id,
                                taxonomyModel.taxonomyId,
                                rules
                            )
                            if (revId && classMappingService.applyMapping(revId)) {
                                previewText.text = "映射已执行并应用"
                                taxonomyModel.refresh()
                            } else {
                                previewText.text = "映射执行失败"
                            }
                        }
                    }
                }

                // 预览结果
                Label {
                    id: previewText
                    Layout.fillWidth: true
                    text: ""
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    function parseMappingRules(text) {
        var rules = {}
        var lines = text.trim().split('\n')
        for (var i = 0; i < lines.length; i++) {
            var line = lines[i].trim()
            if (line && line.indexOf(':') > 0) {
                var parts = line.split(':')
                rules[parts[0].trim()] = parts[1].trim()
            }
        }
        return rules
    }
}
