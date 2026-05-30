// ModelPage.qml - 版本中心
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme
import QtQuick.Layouts

Item {
    id: root

    property string currentProjectId: appController.currentProjectId
    property string selectedVersionId: ""
    property var selectedVersion: null

    onCurrentProjectIdChanged: {
        modelVersionModel.setProjectId(currentProjectId)
        selectedVersionId = ""
        selectedVersion = null
    }

    // 未打开项目时的空状态提示
    ColumnLayout {
        anchors.centerIn: parent
        visible: currentProjectId === ""
        spacing: 16

        Label {
            text: "🏷️ 请先打开一个项目"
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeTitle
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            text: "完成训练后，在此管理模型版本"
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeNormal
            Layout.alignment: Qt.AlignHCenter
        }

        Button {
            text: "前往项目中心"
            font.family: Theme.fontFamily
            Layout.alignment: Qt.AlignHCenter
            background: Rectangle {
                color: parent.hovered ? Theme.accentPrimary : Theme.bgTertiary
                radius: Theme.radiusSmall
                border.color: Theme.accentPrimary
                border.width: 1
                implicitWidth: 140
                implicitHeight: 36
            }
            contentItem: Label {
                text: parent.text
                color: Theme.accentPrimary
                font.pixelSize: Theme.fontSizeNormal
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: appController.currentPageIndex = 0
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        visible: currentProjectId !== ""

        // 标签栏
        TabBar {
            id: modelTabBar
            Layout.fillWidth: true
            background: Rectangle { color: "transparent" }

            TabButton {
                text: "版本列表"
                font.pixelSize: 13
                width: implicitWidth + 24

                background: Rectangle {
                    color: modelTabBar.currentIndex === 0 ? Theme.bgInput : "transparent"
                    radius: 6
                }

                contentItem: Label {
                    text: parent.text
                    color: modelTabBar.currentIndex === 0 ? Theme.accentPrimary : Theme.textMuted
                    font.pixelSize: 13
                    font.bold: modelTabBar.currentIndex === 0
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            TabButton {
                text: "版本对比"
                font.pixelSize: 13
                width: implicitWidth + 24

                background: Rectangle {
                    color: modelTabBar.currentIndex === 1 ? Theme.bgInput : "transparent"
                    radius: 6
                }

                contentItem: Label {
                    text: parent.text
                    color: modelTabBar.currentIndex === 1 ? Theme.accentPrimary : Theme.textMuted
                    font.pixelSize: 13
                    font.bold: modelTabBar.currentIndex === 1
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // 标签页内容堆叠布局
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: modelTabBar.currentIndex

            // 标签页0：版本列表
            RowLayout {
                spacing: 12

                // 左侧面板：版本列表
                Rectangle {
                    Layout.preferredWidth: 400
                    Layout.fillHeight: true
                    color: Theme.bgCard
                    radius: 8

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        // 标题栏
                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                text: "模型版本"
                                color: Theme.accentPrimary
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Item { Layout.fillWidth: true }

                            Label {
                                text: modelVersionModel.count + " 个版本"
                                color: Theme.textMuted
                                font.pixelSize: 12
                            }

                            Button {
                                text: "刷新"
                                flat: true
                                palette.buttonText: Theme.accentPrimary
                                font.pixelSize: 12
                                onClicked: modelVersionModel.refresh()
                            }
                        }

                        // 版本列表
                        ListView {
                            id: versionList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: modelVersionModel
                            spacing: 4

                            Label {
                                anchors.centerIn: parent
                                visible: versionList.count === 0
                                text: "暂无模型版本\n完成训练后将自动注册版本"
                                color: Theme.textMuted
                                font.pixelSize: 14
                                horizontalAlignment: Text.AlignHCenter
                            }

                            delegate: Rectangle {
                                width: versionList.width
                                height: 72
                                radius: 6
                                color: selectedVersionId === model.versionId ? Theme.bgInput : (mouseArea.containsMouse ? Theme.bgSecondary : Theme.bgPrimary)
                                border.color: selectedVersionId === model.versionId ? Theme.accentPrimary : "transparent"
                                border.width: selectedVersionId === model.versionId ? 1 : 0

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    anchors.topMargin: 8
                                    anchors.bottomMargin: 8
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Label {
                                            text: model.versionId.substring(0, 8) + "..."
                                            color: Theme.accentPrimary
                                            font.pixelSize: 13
                                            font.family: "monospace"
                                        }

                                        Row {
                                            spacing: 4
                                            Layout.fillWidth: true

                                            Repeater {
                                                model: {
                                                    var metricsStr = model.metricsJson
                                                    if (!metricsStr) return []
                                                    try {
                                                        var obj = JSON.parse(metricsStr)
                                                        return obj.tags || []
                                                    } catch(e) {
                                                        return []
                                                    }
                                                }

                                                Rectangle {
                                                    height: 20
                                                    width: tagText.implicitWidth + 12
                                                    radius: 4
                                                    color: {
                                                        switch(modelData) {
                                                        case "baseline": return "#3B9AFF20"
                                                        case "best-so-far": return "#34D39920"
                                                        case "production-candidate": return "#FBBF2420"
                                                        default: return "#546E7A20"
                                                        }
                                                    }
                                                    border.color: {
                                                        switch(modelData) {
                                                        case "baseline": return Theme.accentPrimary
                                                        case "best-so-far": return Theme.accentSuccess
                                                        case "production-candidate": return Theme.accentWarning
                                                        default: return Theme.borderNormal
                                                        }
                                                    }
                                                    border.width: 1

                                                    Label {
                                                        id: tagText
                                                        anchors.centerIn: parent
                                                        text: modelData
                                                        color: {
                                                            switch(modelData) {
                                                            case "baseline": return Theme.accentPrimary
                                                            case "best-so-far": return Theme.accentSuccess
                                                            case "production-candidate": return Theme.accentWarning
                                                            default: return Theme.textSecondary
                                                            }
                                                        }
                                                        font.pixelSize: 10
                                                        font.bold: true
                                                    }
                                                }
                                            }
                                        }

                                        Item { Layout.fillWidth: true }

                                        Label {
                                            visible: model.parentVersionId && model.parentVersionId !== ""
                                            text: "parent: " + model.parentVersionId.substring(0, 8) + "..."
                                            color: Theme.textMuted
                                            font.pixelSize: 10
                                            font.family: "monospace"
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Label {
                                            text: "Run: " + model.runId.substring(0, 8) + "..."
                                            color: Theme.textSecondary
                                            font.pixelSize: 11
                                            font.family: "monospace"
                                        }

                                        Label {
                                            text: model.createdAt || "N/A"
                                            color: Theme.textMuted
                                            font.pixelSize: 11
                                        }
                                    }
                                }

                                MouseArea {
                                    id: mouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        selectedVersionId = model.versionId
                                        var details = modelRegistry.getModelVersion(model.versionId)
                                        selectedVersion = details
                                        metricChart.versionId = model.versionId
                                        metricChart.metricsJson = model.metricsJson || "{}"
                                    }
                                }
                            }
                        }
                    }
                }

                // 右侧面板：版本详情与指标
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.bgCard
                    radius: 8

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        // 空状态提示
                        Label {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: selectedVersionId === ""
                            text: "选择一个模型版本查看详情"
                            color: Theme.textMuted
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        // 版本详情标题
                        RowLayout {
                            Layout.fillWidth: true
                            visible: selectedVersionId !== ""

                            Label {
                                text: "版本详情"
                                color: Theme.accentPrimary
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Item { Layout.fillWidth: true }

                            Button {
                                visible: selectedVersionId !== ""
                                text: "删除"
                                flat: true
                                palette.buttonText: Theme.accentError
                                font.pixelSize: 12

                                onClicked: {
                                    if (modelRegistry.deleteModelVersion(selectedVersionId)) {
                                        selectedVersionId = ""
                                        selectedVersion = null
                                        modelVersionModel.refresh()
                                    }
                                }
                            }
                        }

                        // 版本信息
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 120
                            visible: selectedVersionId !== ""
                            color: Theme.bgPrimary
                            radius: 6

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: "ID:"; color: Theme.textMuted; font.pixelSize: 12; Layout.preferredWidth: 100 }
                                    Label {
                                        text: selectedVersionId
                                        color: Theme.textPrimary
                                        font.pixelSize: 12
                                        font.family: "monospace"
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: "运行ID:"; color: Theme.textMuted; font.pixelSize: 12; Layout.preferredWidth: 100 }
                                    Label {
                                        text: selectedVersion ? selectedVersion.runId || "N/A" : "N/A"
                                        color: Theme.textPrimary
                                        font.pixelSize: 12
                                        font.family: "monospace"
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: "最佳权重:"; color: Theme.textMuted; font.pixelSize: 12; Layout.preferredWidth: 100 }
                                    Label {
                                        text: selectedVersion ? selectedVersion.bestWeightPath || "N/A" : "N/A"
                                        color: Theme.accentSuccess
                                        font.pixelSize: 12
                                        font.family: "monospace"
                                        Layout.fillWidth: true
                                        elide: Text.ElideMiddle
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: "最新权重:"; color: Theme.textMuted; font.pixelSize: 12; Layout.preferredWidth: 100 }
                                    Label {
                                        text: selectedVersion ? selectedVersion.lastWeightPath || "N/A" : "N/A"
                                        color: Theme.textSecondary
                                        font.pixelSize: 12
                                        font.family: "monospace"
                                        Layout.fillWidth: true
                                        elide: Text.ElideMiddle
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: "父版本:"; color: Theme.textMuted; font.pixelSize: 12; Layout.preferredWidth: 100 }
                                    Label {
                                        text: {
                                            if (!selectedVersion) return "无"
                                            var pv = selectedVersion.parentVersionId
                                            if (!pv || pv === "") return "无"
                                            return pv.substring(0, 8) + "..."
                                        }
                                        color: Theme.accentPrimary
                                        font.pixelSize: 12
                                        font.family: "monospace"
                                    }
                                }
                            }
                        }

                        // 标签管理
                        RowLayout {
                            Layout.fillWidth: true
                            visible: selectedVersionId !== ""
                            spacing: 8

                            Label {
                                text: "标签:"
                                color: Theme.textPrimary
                                font.pixelSize: 13
                            }

                            Button {
                                text: "基线"
                                flat: true
                                palette.buttonText: Theme.accentPrimary
                                font.pixelSize: 11
                                onClicked: {
                                    modelRegistry.setTag(selectedVersionId, "baseline")
                                    modelVersionModel.refresh()
                                    selectedVersion = modelRegistry.getModelVersion(selectedVersionId)
                                    metricChart.metricsJson = selectedVersion.metricsJson || "{}"
                                }
                            }

                            Button {
                                text: "最佳"
                                flat: true
                                palette.buttonText: Theme.accentSuccess
                                font.pixelSize: 11
                                onClicked: {
                                    modelRegistry.setTag(selectedVersionId, "best-so-far")
                                    modelVersionModel.refresh()
                                    selectedVersion = modelRegistry.getModelVersion(selectedVersionId)
                                    metricChart.metricsJson = selectedVersion.metricsJson || "{}"
                                }
                            }

                            Button {
                                text: "生产"
                                flat: true
                                palette.buttonText: Theme.accentWarning
                                font.pixelSize: 11
                                onClicked: {
                                    modelRegistry.setTag(selectedVersionId, "production-candidate")
                                    modelVersionModel.refresh()
                                    selectedVersion = modelRegistry.getModelVersion(selectedVersionId)
                                    metricChart.metricsJson = selectedVersion.metricsJson || "{}"
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Button {
                                text: "清除标签"
                                flat: true
                                palette.buttonText: Theme.accentError
                                font.pixelSize: 11
                                onClicked: {
                                    modelRegistry.removeTag(selectedVersionId, "baseline")
                                    modelRegistry.removeTag(selectedVersionId, "best-so-far")
                                    modelRegistry.removeTag(selectedVersionId, "production-candidate")
                                    modelVersionModel.refresh()
                                    selectedVersion = modelRegistry.getModelVersion(selectedVersionId)
                                    metricChart.metricsJson = selectedVersion.metricsJson || "{}"
                                }
                            }
                        }

                        // 版本谱系可视化
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: lineageContent.height + 16
                            visible: selectedVersionId !== ""
                            color: Theme.bgInput
                            radius: 6

                            ColumnLayout {
                                id: lineageContent
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 8
                                spacing: 4

                                Label {
                                    text: "版本谱系"
                                    color: Theme.accentPrimary
                                    font.pixelSize: 12
                                    font.bold: true
                                }

                                Label {
                                    id: lineageLabel
                                    color: Theme.textSecondary
                                    font.pixelSize: 11
                                    font.family: "monospace"
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true

                                    text: {
                                        if (!selectedVersion) return ""
                                        var chain = []
                                        var current = selectedVersion
                                        var depth = 0
                                        while (current && depth < 10) {
                                            var tag = ""
                                            if (current.metricsJson) {
                                                try {
                                                    var m = JSON.parse(current.metricsJson)
                                                    if (m.tags && m.tags.length > 0) {
                                                        tag = " [" + m.tags.join(",") + "]"
                                                    }
                                                } catch(e) {}
                                            }
                                            chain.push(current.versionId.substring(0, 8) + tag)
                                            if (current.parentVersionId && current.parentVersionId !== "") {
                                                current = modelRegistry.getModelVersion(current.parentVersionId)
                                            } else {
                                                break
                                            }
                                            depth++
                                        }
                                        if (chain.length <= 1) return "无父版本（根版本）"
                                        return chain.reverse().join(" -> ")
                                    }
                                }
                            }
                        }

                        // 指标展示
                        MetricChart {
                            id: metricChart
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: selectedVersionId !== ""
                        }
                    }
                }
            }

            // 标签页1：版本对比
            ComparePage {
                currentProjectId: root.currentProjectId
            }
        }
    }
}
