// ClassMappingPage.qml - V2 类别映射向导（高保真复刻版）
// 布局：左侧数据集选择 + 源类别列表 + 目标类别体系 + 映射规则编辑器 + 预览面板
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme
import LabelTorch.Components

Item {
    id: root

    property string currentDatasetId: ""
    property string mappingActionMessage: ""
    property string mappingActionTone: "neutral"
    property var sourceClasses: []
    property var targetClasses: []
    property var mappingRules: ({})

    function loadSourceClasses() {
        if (!currentDatasetId) {
            sourceClasses = []
            return
        }
        var schema = classMappingService.getSourceSchema(currentDatasetId)
        if (schema && schema.rawClassNamesJson) {
            try {
                sourceClasses = JSON.parse(schema.rawClassNamesJson)
            } catch(e) {
                sourceClasses = []
            }
        } else {
            sourceClasses = []
        }
    }

    function loadTargetClasses() {
        var classes = []
        for (var i = 0; i < taxonomyModel.rowCount(); i++) {
            var idx = taxonomyModel.index(i, 0)
            var className = taxonomyModel.data(idx, 1)
            var classIndex = taxonomyModel.data(idx, 0)
            classes.push({name: className, index: classIndex})
        }
        targetClasses = classes
    }

    function parseMappingRules(text) {
        var rules = {}
        var lines = text.split("\n")
        for (var i = 0; i < lines.length; i++) {
            var line = lines[i].trim()
            if (!line || line.startsWith("#")) continue
            var parts = line.split(":")
            if (parts.length >= 2) {
                var src = parts[0].trim()
                var dst = parts.slice(1).join(":").trim()
                if (src && dst) rules[src] = dst
            }
        }
        return rules
    }

    function generateMappingSuggestions() {
        var suggestions = []
        for (var i = 0; i < sourceClasses.length; i++) {
            var srcName = sourceClasses[i]
            var matchedName = ""
            for (var j = 0; j < targetClasses.length; j++) {
                var dstName = targetClasses[j].name
                if (srcName.toLowerCase().indexOf(dstName.toLowerCase()) >= 0 ||
                    dstName.toLowerCase().indexOf(srcName.toLowerCase()) >= 0) {
                    matchedName = dstName
                    break
                }
            }
            suggestions.push({source: srcName, target: matchedName, hasMatch: matchedName !== ""})
        }
        return suggestions
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: Theme.bgSide

            Rectangle {
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.borderColor
                width: parent.width
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingXLarge
                anchors.rightMargin: Theme.spacingXLarge
                spacing: Theme.spacingNormal

                Text {
                    text: "类别映射向导"
                    font.pixelSize: Theme.fontSizeHeading
                    font.weight: Font.DemiBold
                    font.family: Theme.fontFamily
                    color: Theme.textMain
                }

                StatusTag {
                    visible: root.mappingActionMessage !== ""
                    text: root.mappingActionMessage
                    tone: root.mappingActionTone
                }

                Item { Layout.fillWidth: true }

                Text {
                    visible: !appController.projectOpen
                    text: "请先打开项目"
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.danger
                }
            }
        }

        Rectangle {
            visible: appController.projectOpen
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgMain

            RowLayout {
                anchors.fill: parent
                spacing: Theme.spacingNormal
                anchors.margins: Theme.spacingXLarge

                Rectangle {
                    Layout.preferredWidth: 320
                    Layout.fillHeight: true
                    color: Theme.bgCard
                    radius: Theme.radiusNormal
                    border.color: Theme.borderColor
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingNormal
                        spacing: Theme.spacingNormal

                        Text {
                            text: "源数据集"
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: Font.DemiBold
                            color: Theme.textSecondary
                        }

                        ComboBox {
                            id: datasetCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            model: datasetModel
                            textRole: "name"
                            valueRole: "id"

                            background: Rectangle {
                                color: parent.pressed ? Theme.bgInput : Theme.bgCard
                                border.color: Theme.borderColor
                                border.width: 1
                                radius: Theme.radiusSmall
                            }

                            contentItem: Text {
                                text: parent.displayText
                                font.pixelSize: Theme.fontSizeSmall
                                font.family: Theme.fontFamily
                                color: Theme.textMain
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 10
                            }

                            onActivated: {
                                root.currentDatasetId = currentValue
                                root.loadSourceClasses()
                                root.loadTargetClasses()
                                root.mappingRules = root.generateMappingSuggestions()
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 1
                            color: Theme.borderColor
                        }

                        Text {
                            text: "源类别 (" + root.sourceClasses.length + ")"
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: Font.DemiBold
                            color: Theme.textSecondary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: Theme.borderColor
                            border.width: 1

                            ScrollView {
                                anchors.fill: parent
                                clip: true

                                ColumnLayout {
                                    width: parent.width - 2
                                    spacing: 1

                                    Repeater {
                                        model: root.sourceClasses

                                        Rectangle {
                                            width: parent.width
                                            height: 28
                                            color: index % 2 === 0 ? Theme.bgCard : Theme.bgInput
                                            radius: 2

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8
                                                spacing: 8

                                                Text {
                                                    text: modelData
                                                    font.pixelSize: Theme.fontSizeCaption
                                                    font.family: Theme.fontFamilyMono
                                                    color: Theme.textMain
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.spacingNormal

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 60
                        color: Theme.bgCard
                        radius: Theme.radiusNormal
                        border.color: Theme.borderColor
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingNormal
                            anchors.rightMargin: Theme.spacingNormal
                            spacing: Theme.spacingSmall

                            Text {
                                text: "映射建议"
                                font.pixelSize: Theme.fontSizeSmall
                                font.weight: Font.DemiBold
                                color: Theme.textSecondary
                            }

                            Item { Layout.fillWidth: true }

                            Button {
                                text: "自动生成"
                                Layout.preferredHeight: 28
                                font.pixelSize: Theme.fontSizeCaption

                                background: Rectangle {
                                    color: parent.hovered ? Theme.bgHover : Theme.bgCard
                                    border.color: Theme.primary
                                    border.width: 1
                                    radius: Theme.radiusSmall
                                }

                                contentItem: Text {
                                    text: parent.text
                                    font.pixelSize: Theme.fontSizeCaption
                                    color: Theme.primary
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                onClicked: {
                                    root.mappingRules = root.generateMappingSuggestions()
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: Theme.bgCard
                        radius: Theme.radiusNormal
                        border.color: Theme.borderColor
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingNormal
                            spacing: Theme.spacingSmall

                            Text {
                                text: "映射规则"
                                font.pixelSize: Theme.fontSizeSmall
                                font.weight: Font.DemiBold
                                color: Theme.textSecondary
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                color: Theme.bgInput
                                radius: Theme.radiusSmall
                                border.color: Theme.borderColor
                                border.width: 1

                                ScrollView {
                                    anchors.fill: parent
                                    clip: true

                                    ColumnLayout {
                                        width: parent.width - 2
                                        spacing: 2

                                        Repeater {
                                            model: root.mappingRules

                                            RowLayout {
                                                width: parent.width
                                                height: 36
                                                spacing: Theme.spacingSmall

                                                Text {
                                                    text: modelData.source
                                                    font.pixelSize: Theme.fontSizeCaption
                                                    font.family: Theme.fontFamilyMono
                                                    color: Theme.textMuted
                                                    Layout.preferredWidth: 100
                                                    elide: Text.ElideRight
                                                }

                                                Text {
                                                    text: "→"
                                                    font.pixelSize: Theme.fontSizeSmall
                                                    color: Theme.primary
                                                }

                                                ComboBox {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 28
                                                    model: root.targetClasses.map(function(c) { return c.name })
                                                    currentIndex: model.indexOf(modelData.target) >= 0 ? model.indexOf(modelData.target) : 0

                                                    background: Rectangle {
                                                        color: parent.pressed ? Theme.bgInput : Theme.bgCard
                                                        border.color: Theme.borderColor
                                                        border.width: 1
                                                        radius: Theme.radiusSmall
                                                    }

                                                    contentItem: Text {
                                                        text: parent.displayText
                                                        font.pixelSize: Theme.fontSizeCaption
                                                        font.family: Theme.fontFamily
                                                        color: Theme.textMain
                                                        verticalAlignment: Text.AlignVCenter
                                                        leftPadding: 8
                                                    }

                                                    onActivated: {
                                                        root.mappingRules[index].target = currentValue
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 320
                    Layout.fillHeight: true
                    color: Theme.bgCard
                    radius: Theme.radiusNormal
                    border.color: Theme.borderColor
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingNormal
                        spacing: Theme.spacingNormal

                        Text {
                            text: "预览与执行"
                            font.pixelSize: Theme.fontSizeSmall
                            font.weight: Font.DemiBold
                            color: Theme.textSecondary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: Theme.borderColor
                            border.width: 1

                            TextArea {
                                id: previewText
                                anchors.fill: parent
                                anchors.margins: 10
                                readOnly: true
                                wrapMode: TextArea.Wrap
                                font.pixelSize: Theme.fontSizeCaption
                                font.family: Theme.fontFamilyMono
                                color: Theme.textSecondary
                                placeholderText: "点击预览按钮查看映射结果"
                                background: Rectangle { color: "transparent" }
                            }
                        }

                        Button {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            text: "预览映射"

                            background: Rectangle {
                                color: parent.hovered ? Qt.lighter(Theme.primary, 1.1) : Theme.primary
                                radius: Theme.radiusNormal
                            }

                            contentItem: Text {
                                text: parent.text
                                font.pixelSize: Theme.fontSizeNormal
                                font.weight: Font.DemiBold
                                font.family: Theme.fontFamily
                                color: "#FFFFFF"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                var rules = {}
                                for (var i = 0; i < root.mappingRules.length; i++) {
                                    var r = root.mappingRules[i]
                                    if (r.source && r.target) rules[r.source] = r.target
                                }
                                var preview = classMappingService.previewMapping(root.currentDatasetId, rules)
                                previewText.text = "总标签数：" + (preview.totalLabels || 0) + "\n" +
                                                   "受影响样本：" + (preview.affectedSamples || 0) + "\n" +
                                                   "新分布：" + JSON.stringify(preview.newClassDistribution, null, 2)
                                root.mappingActionMessage = "映射预览完成"
                                root.mappingActionTone = "info"
                            }
                        }

                        Button {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            text: "执行映射"

                            background: Rectangle {
                                color: parent.hovered ? Qt.lighter(Theme.success, 1.1) : Theme.success
                                radius: Theme.radiusNormal
                            }

                            contentItem: Text {
                                text: parent.text
                                font.pixelSize: Theme.fontSizeNormal
                                font.weight: Font.DemiBold
                                font.family: Theme.fontFamily
                                color: "#FFFFFF"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                var rules = {}
                                for (var i = 0; i < root.mappingRules.length; i++) {
                                    var r = root.mappingRules[i]
                                    if (r.source && r.target) rules[r.source] = r.target
                                }
                                var schema = classMappingService.getSourceSchema(root.currentDatasetId)
                                var taxonomyId = taxonomyModel.taxonomyId || ""
                                if (!taxonomyId) {
                                    root.mappingActionMessage = "请先创建项目类别体系"
                                    root.mappingActionTone = "warning"
                                    return
                                }
                                var mappingId = classMappingService.createMapping(
                                    root.currentDatasetId, schema.id, taxonomyId, rules
                                )
                                if (mappingId !== "") {
                                    root.mappingActionMessage = "映射创建成功，点击确认应用"
                                    root.mappingActionTone = "success"
                                    var confirmed = classMappingService.applyMapping(mappingId)
                                    if (confirmed) {
                                        root.mappingActionMessage = "映射已应用，标签文件已更新"
                                        root.mappingActionTone = "success"
                                    } else {
                                        root.mappingActionMessage = "映射应用失败"
                                        root.mappingActionTone = "danger"
                                    }
                                } else {
                                    root.mappingActionMessage = "映射创建失败"
                                    root.mappingActionTone = "danger"
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            visible: !appController.projectOpen
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgMain

            Column {
                anchors.centerIn: parent
                spacing: Theme.spacingNormal

                Text {
                    text: "请先在「项目」页面打开或创建项目"
                    font.pixelSize: Theme.fontSizeSubheading
                    font.family: Theme.fontFamily
                    color: Theme.textMuted
                }
            }
        }
    }
}
