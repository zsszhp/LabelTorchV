// ComparePage.qml - Experiment Comparison
import QtQuick
import QtQuick.Controls
import LabelTorch.Shell
import QtQuick.Layouts

Item {
    id: root

    property string currentProjectId: ""

    // Internal state
    property var selectedVersionIds: []
    property string compareMode: "horizontal"  // "horizontal" or "vertical"
    property var comparisonData: []
    property bool hasCompared: false

    onCurrentProjectIdChanged: {
        modelVersionModel.setProjectId(currentProjectId)
        selectedVersionIds = []
        comparisonData = []
        hasCompared = false
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // Left panel: Version picker with checkboxes
        Rectangle {
            Layout.preferredWidth: 380
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
                        text: "Select Versions"
                        color: Theme.accentPrimary
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: selectedVersionIds.length + " selected"
                        color: Theme.textMuted
                        font.pixelSize: 12
                    }
                }

                // Comparison mode selector
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "Mode:"
                        color: Theme.textPrimary
                        font.pixelSize: 12
                    }

                    Button {
                        id: horizontalBtn
                        text: "Horizontal"
                        font.pixelSize: 11
                        flat: true
                        palette.buttonText: compareMode === "horizontal" ? Theme.accentPrimary : Theme.textMuted
                        onClicked: compareMode = "horizontal"

                        background: Rectangle {
                            radius: 4
                            color: compareMode === "horizontal" ? Theme.bgInput : "transparent"
                            border.color: compareMode === "horizontal" ? Theme.accentPrimary : "transparent"
                            border.width: compareMode === "horizontal" ? 1 : 0
                        }
                    }

                    Button {
                        id: verticalBtn
                        text: "Vertical (Chain)"
                        font.pixelSize: 11
                        flat: true
                        palette.buttonText: compareMode === "vertical" ? Theme.accentPrimary : Theme.textMuted
                        onClicked: compareMode = "vertical"

                        background: Rectangle {
                            radius: 4
                            color: compareMode === "vertical" ? Theme.bgInput : "transparent"
                            border.color: compareMode === "vertical" ? Theme.accentPrimary : "transparent"
                            border.width: compareMode === "vertical" ? 1 : 0
                        }
                    }
                }

                // Mode description
                Label {
                    Layout.fillWidth: true
                    text: compareMode === "horizontal"
                          ? "Compare versions trained on the same dataset snapshot"
                          : "Compare versions in the same incremental training chain"
                    color: Theme.textMuted
                    font.pixelSize: 10
                    font.italic: true
                    wrapMode: Text.WordWrap
                }

                // Version list with checkboxes
                ListView {
                    id: versionCheckList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: modelVersionModel
                    spacing: 4

                    Label {
                        anchors.centerIn: parent
                        visible: versionCheckList.count === 0
                        text: "No model versions yet"
                        color: Theme.textMuted
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                    }

                    delegate: Rectangle {
                        width: versionCheckList.width
                        height: 56
                        radius: 6
                        color: isChecked ? Theme.bgInput : (checkMouseArea.containsMouse ? "#252536" : Theme.bgPrimary)
                        border.color: isChecked ? Theme.accentPrimary : "transparent"
                        border.width: isChecked ? 1 : 0

                        property bool isChecked: selectedVersionIds.indexOf(model.versionId) >= 0

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 8

                            CheckBox {
                                id: versionCheckBox
                                checked: parent.parent.isChecked
                                onCheckedChanged: {
                                    var ids = selectedVersionIds
                                    var idx = ids.indexOf(model.versionId)
                                    if (checked && idx < 0) {
                                        ids.push(model.versionId)
                                    } else if (!checked && idx >= 0) {
                                        ids.splice(idx, 1)
                                    }
                                    selectedVersionIds = ids.slice()  // trigger binding update
                                }

                                indicator: Rectangle {
                                    implicitWidth: 16
                                    implicitHeight: 16
                                    x: versionCheckBox.leftPadding
                                    y: parent.height / 2 - height / 2
                                    radius: 3
                                    color: versionCheckBox.checked ? Theme.accentPrimary : Theme.bgCard
                                    border.color: versionCheckBox.checked ? Theme.accentPrimary : Theme.textMuted

                                    Label {
                                        anchors.centerIn: parent
                                        text: versionCheckBox.checked ? "\u2713" : ""
                                        color: Theme.bgPrimary
                                        font.pixelSize: 12
                                        font.bold: true
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    text: model.versionId.substring(0, 8) + "..."
                                    color: Theme.accentPrimary
                                    font.pixelSize: 12
                                    font.family: "monospace"
                                }

                                Label {
                                    text: "Run: " + model.runId.substring(0, 8) + "..." +
                                          (model.parentVersionId && model.parentVersionId !== "" ? "  Parent: " + model.parentVersionId.substring(0, 8) + "..." : "")
                                    color: Theme.textMuted
                                    font.pixelSize: 10
                                    font.family: "monospace"
                                }
                            }
                        }

                        MouseArea {
                            id: checkMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: versionCheckBox.checked = !versionCheckBox.checked
                        }
                    }
                }

                // Action buttons
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        text: "Compare"
                        enabled: selectedVersionIds.length >= 2
                        font.pixelSize: 12

                        background: Rectangle {
                            radius: 6
                            color: parent.enabled ? Theme.accentPrimary : Theme.bgInput
                            implicitHeight: 32
                            implicitWidth: 120
                        }

                        contentItem: Label {
                            text: parent.text
                            color: parent.enabled ? Theme.bgPrimary : Theme.textMuted
                            font.pixelSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            var ids = []
                            for (var i = 0; i < selectedVersionIds.length; i++) {
                                ids.push(selectedVersionIds[i])
                            }
                            comparisonData = metricService.compareMultipleVersions(ids)
                            hasCompared = true
                        }
                    }

                    Button {
                        text: "Clear"
                        enabled: selectedVersionIds.length > 0
                        font.pixelSize: 12
                        flat: true
                        palette.buttonText: Theme.accentError

                        onClicked: {
                            selectedVersionIds = []
                            comparisonData = []
                            hasCompared = false
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "Select All"
                        enabled: versionCheckList.count > 0
                        font.pixelSize: 11
                        flat: true
                        palette.buttonText: Theme.accentSuccess

                        onClicked: {
                            var ids = []
                            for (var i = 0; i < versionCheckList.count; i++) {
                                var idx = versionCheckList.model.index(i, 0)
                                var vid = versionCheckList.model.data(idx, 257)  // IdRole = Qt.UserRole + 1 = 257
                                if (vid && vid !== "") ids.push(vid)
                            }
                            selectedVersionIds = ids
                        }
                    }
                }
            }
        }

        // Right panel: Comparison results
        Rectangle {
            Layout.fillWidth: true
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
                        text: "Comparison Results"
                        color: Theme.accentPrimary
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        visible: hasCompared && comparisonData.length >= 2
                        text: compareMode === "horizontal" ? "Horizontal: Same Snapshot" : "Vertical: Incremental Chain"
                        color: compareMode === "horizontal" ? Theme.accentSuccess : Theme.accentWarning
                        font.pixelSize: 11
                    }
                }

                // Empty state
                Label {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: !hasCompared || comparisonData.length < 2
                    text: !hasCompared
                          ? "Select 2 or more versions and click Compare"
                          : "Need at least 2 versions with metrics to compare"
                    color: Theme.textMuted
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                // Comparison table
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: hasCompared && comparisonData.length >= 2
                    clip: true

                    ColumnLayout {
                        width: Math.max(parent.parent.width - 24, implicitWidth)
                        spacing: 6

                        // Version header row
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            // Metric name column header
                            Rectangle {
                                width: 140
                                height: 36
                                color: "#11111b"
                                radius: 4

                                Label {
                                    anchors.centerIn: parent
                                    text: "Metric"
                                    color: Theme.textPrimary
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                            }

                            // One column per version
                            Repeater {
                                model: comparisonData

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 36
                                    color: "#11111b"
                                    radius: 4

                                    ColumnLayout {
                                        anchors.centerIn: parent
                                        spacing: 0

                                        Label {
                                            text: modelData.versionId.substring(0, 8) + "..."
                                            color: Theme.accentPrimary
                                            font.pixelSize: 11
                                            font.family: "monospace"
                                            Layout.alignment: Qt.AlignHCenter
                                        }
                                        Label {
                                            text: {
                                                if (compareMode === "horizontal" && modelData.snapshotId) {
                                                    return "Snap: " + modelData.snapshotId.substring(0, 8) + "..."
                                                }
                                                if (compareMode === "vertical" && modelData.parentVersionId && modelData.parentVersionId !== "") {
                                                    return "Parent: " + modelData.parentVersionId.substring(0, 8) + "..."
                                                }
                                                return "Root version"
                                            }
                                            color: Theme.textMuted
                                            font.pixelSize: 9
                                            font.family: "monospace"
                                            Layout.alignment: Qt.AlignHCenter
                                        }
                                    }
                                }
                            }
                        }

                        // Metric rows
                        Repeater {
                            id: metricRowRepeater
                            model: ["mAP50", "mAP50-95", "precision", "recall", "fitness"]

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    // Metric name
                                    Rectangle {
                                        width: 140
                                        height: 40
                                        color: Theme.bgPrimary
                                        radius: 4

                                        Label {
                                            anchors.centerIn: parent
                                            text: modelData
                                            color: Theme.textPrimary
                                            font.pixelSize: 12
                                            font.bold: true
                                        }
                                    }

                                    // Value for each version
                                    Repeater {
                                        model: comparisonData

                                        Rectangle {
                                            Layout.fillWidth: true
                                            height: 40
                                            radius: 4

                                            // Determine color based on best/worst
                                            color: {
                                                var metricName = metricRowRepeater.model[index]
                                                var val = modelData.metrics[metricName]
                                                if (val === undefined || isNaN(Number(val))) return Theme.bgPrimary

                                                // Collect all valid values for this metric across versions
                                                var values = []
                                                for (var i = 0; i < comparisonData.length; i++) {
                                                    var mv = comparisonData[i].metrics[metricName]
                                                    if (mv !== undefined && !isNaN(Number(mv))) {
                                                        values.push(Number(mv))
                                                    }
                                                }

                                                if (values.length < 2) return Theme.bgPrimary

                                                var numVal = Number(val)
                                                var maxVal = Math.max.apply(null, values)
                                                var minVal = Math.min.apply(null, values)

                                                if (numVal === maxVal) return "#a6e3a120"  // green bg for best
                                                if (numVal === minVal) return "#f38ba820"   // red bg for worst
                                                return Theme.bgPrimary
                                            }

                                            border.color: {
                                                var metricName = metricRowRepeater.model[index]
                                                var val = modelData.metrics[metricName]
                                                if (val === undefined || isNaN(Number(val))) return "transparent"

                                                var values = []
                                                for (var i = 0; i < comparisonData.length; i++) {
                                                    var mv = comparisonData[i].metrics[metricName]
                                                    if (mv !== undefined && !isNaN(Number(mv))) {
                                                        values.push(Number(mv))
                                                    }
                                                }

                                                if (values.length < 2) return "transparent"

                                                var numVal = Number(val)
                                                var maxVal = Math.max.apply(null, values)
                                                var minVal = Math.min.apply(null, values)

                                                if (numVal === maxVal) return Theme.accentSuccess  // green border for best
                                                if (numVal === minVal) return Theme.accentError   // red border for worst
                                                return "transparent"
                                            }
                                            border.width: 1

                                            Label {
                                                anchors.centerIn: parent
                                                text: {
                                                    var metricName = metricRowRepeater.model[index]
                                                    var val = modelData.metrics[metricName]
                                                    if (val === undefined) return "N/A"
                                                    var numVal = Number(val)
                                                    if (isNaN(numVal)) return String(val)
                                                    return numVal.toFixed(4)
                                                }
                                                color: {
                                                    var metricName = metricRowRepeater.model[index]
                                                    var val = modelData.metrics[metricName]
                                                    if (val === undefined || isNaN(Number(val))) return Theme.textMuted

                                                    var values = []
                                                    for (var i = 0; i < comparisonData.length; i++) {
                                                        var mv = comparisonData[i].metrics[metricName]
                                                        if (mv !== undefined && !isNaN(Number(mv))) {
                                                            values.push(Number(mv))
                                                        }
                                                    }

                                                    if (values.length < 2) return Theme.textPrimary

                                                    var numVal = Number(val)
                                                    var maxVal = Math.max.apply(null, values)
                                                    var minVal = Math.min.apply(null, values)

                                                    if (numVal === maxVal) return Theme.accentSuccess  // green text for best
                                                    if (numVal === minVal) return Theme.accentError   // red text for worst
                                                    return Theme.textPrimary
                                                }
                                                font.pixelSize: 13
                                                font.bold: true
                                                font.family: "monospace"
                                            }
                                        }
                                    }
                                }

                                // Separator
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Theme.bgInput
                                }
                            }
                        }

                        // Legend
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 8
                            spacing: 16

                            RowLayout {
                                spacing: 4
                                Rectangle {
                                    width: 12
                                    height: 12
                                    radius: 2
                                    color: "#a6e3a120"
                                    border.color: Theme.accentSuccess
                                    border.width: 1
                                }
                                Label {
                                    text: "Best"
                                    color: Theme.accentSuccess
                                    font.pixelSize: 11
                                }
                            }

                            RowLayout {
                                spacing: 4
                                Rectangle {
                                    width: 12
                                    height: 12
                                    radius: 2
                                    color: "#f38ba820"
                                    border.color: Theme.accentError
                                    border.width: 1
                                }
                                Label {
                                    text: "Worst"
                                    color: Theme.accentError
                                    font.pixelSize: 11
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Label {
                                text: comparisonData.length + " versions compared"
                                color: Theme.textMuted
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }
        }
    }
}
