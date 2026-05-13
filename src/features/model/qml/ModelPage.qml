// ModelPage.qml - Version Center
import QtQuick
import QtQuick.Controls
import LabelTorch.Shell
import QtQuick.Layouts

Item {
    id: root

    property string currentProjectId: ""
    property string selectedVersionId: ""
    property var selectedVersion: null

    onCurrentProjectIdChanged: {
        modelVersionModel.setProjectId(currentProjectId)
        selectedVersionId = ""
        selectedVersion = null
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // Tab bar
        TabBar {
            id: modelTabBar
            Layout.fillWidth: true
            background: Rectangle { color: "transparent" }

            TabButton {
                text: "Versions"
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
                text: "Compare"
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

        // Stack layout for tab content
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: modelTabBar.currentIndex

            // Tab 0: Versions (original content)
            RowLayout {
                spacing: 12

                // Left panel: Version list
                Rectangle {
                    Layout.preferredWidth: 400
                    Layout.fillHeight: true
                    color: Theme.bgCard
                    radius: 8

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        // Header
                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                text: "Model Versions"
                                color: Theme.accentPrimary
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Item { Layout.fillWidth: true }

                            Label {
                                text: modelVersionModel.count + " versions"
                                color: Theme.textMuted
                                font.pixelSize: 12
                            }

                            Button {
                                text: "Refresh"
                                flat: true
                                palette.buttonText: Theme.accentPrimary
                                font.pixelSize: 12
                                onClicked: modelVersionModel.refresh()
                            }
                        }

                        // Version list
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
                                text: "No model versions yet\nComplete a training run to register a version"
                                color: Theme.textMuted
                                font.pixelSize: 14
                                horizontalAlignment: Text.AlignHCenter
                            }

                            delegate: Rectangle {
                                width: versionList.width
                                height: 72
                                radius: 6
                                color: selectedVersionId === model.versionId ? Theme.bgInput : (mouseArea.containsMouse ? "#252536" : Theme.bgPrimary)
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
                                                        case "baseline": return "#89b4fa20"
                                                        case "best-so-far": return "#a6e3a120"
                                                        case "production-candidate": return "#f9e2af20"
                                                        default: return "#45475a20"
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

                // Right panel: Version details + metrics
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.bgCard
                    radius: 8

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        // Empty state label
                        Label {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: selectedVersionId === ""
                            text: "Select a model version to view details"
                            color: Theme.textMuted
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        // Version details header
                        RowLayout {
                            Layout.fillWidth: true
                            visible: selectedVersionId !== ""

                            Label {
                                text: "Version Details"
                                color: Theme.accentPrimary
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Item { Layout.fillWidth: true }

                            Button {
                                visible: selectedVersionId !== ""
                                text: "Delete"
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

                        // Version info
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
                                    Label { text: "Run ID:"; color: Theme.textMuted; font.pixelSize: 12; Layout.preferredWidth: 100 }
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
                                    Label { text: "Best Weights:"; color: Theme.textMuted; font.pixelSize: 12; Layout.preferredWidth: 100 }
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
                                    Label { text: "Last Weights:"; color: Theme.textMuted; font.pixelSize: 12; Layout.preferredWidth: 100 }
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
                                    Label { text: "Parent:"; color: Theme.textMuted; font.pixelSize: 12; Layout.preferredWidth: 100 }
                                    Label {
                                        text: {
                                            if (!selectedVersion) return "None"
                                            var pv = selectedVersion.parentVersionId
                                            if (!pv || pv === "") return "None"
                                            return pv.substring(0, 8) + "..."
                                        }
                                        color: Theme.accentPrimary
                                        font.pixelSize: 12
                                        font.family: "monospace"
                                    }
                                }
                            }
                        }

                        // Tag management
                        RowLayout {
                            Layout.fillWidth: true
                            visible: selectedVersionId !== ""
                            spacing: 8

                            Label {
                                text: "Tags:"
                                color: Theme.textPrimary
                                font.pixelSize: 13
                            }

                            Button {
                                text: "Baseline"
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
                                text: "Best-so-far"
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
                                text: "Production"
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
                                text: "Clear Tags"
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

                        // Lineage chain visualization
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: lineageContent.height + 16
                            visible: selectedVersionId !== ""
                            color: "#11111b"
                            radius: 6

                            ColumnLayout {
                                id: lineageContent
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 8
                                spacing: 4

                                Label {
                                    text: "Version Lineage"
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
                                        if (chain.length <= 1) return "No parent (root version)"
                                        return chain.reverse().join(" -> ")
                                    }
                                }
                            }
                        }

                        // Metrics display
                        MetricChart {
                            id: metricChart
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: selectedVersionId !== ""
                        }
                    }
                }
            }

            // Tab 1: Compare
            ComparePage {
                currentProjectId: root.currentProjectId
            }
        }
    }
}
