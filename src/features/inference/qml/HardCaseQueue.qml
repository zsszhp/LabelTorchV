// HardCaseQueue.qml - Hard-case priority queue for FP/FN/low-conf review
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string batchId: ""
    property string datasetId: ""
    property real lowConfThreshold: 0.3
    property var hardCases: []
    property string sortBy: "priority"  // "priority", "confidence", "reason"

    function refresh() {
        if (batchId === "" || datasetId === "") return
        hardCases = assistedLabelService.getHardCaseQueue(batchId, datasetId, lowConfThreshold)
        rebuildModel()
    }

    function rebuildModel() {
        hardCaseListModel.clear()
        var sorted = hardCases.slice()
        if (sortBy === "priority") {
            sorted.sort(function(a, b) { return b.priority - a.priority })
        } else if (sortBy === "confidence") {
            sorted.sort(function(a, b) { return a.confidence - b.confidence })
        } else if (sortBy === "reason") {
            sorted.sort(function(a, b) {
                var order = { "false_negative": 0, "low_confidence": 1, "false_positive": 2 }
                return (order[a.reason] || 3) - (order[b.reason] || 3)
            })
        }
        for (var i = 0; i < sorted.length; i++) {
            hardCaseListModel.append(sorted[i])
        }
    }

    function getReasonLabel(reason) {
        if (reason === "false_negative") return "False Negative"
        if (reason === "low_confidence") return "Low Confidence"
        if (reason === "false_positive") return "False Positive"
        return reason
    }

    function getPriorityColor(reason) {
        if (reason === "false_negative") return "#f38ba8"   // red
        if (reason === "low_confidence") return "#f9e2af"   // yellow
        if (reason === "false_positive") return "#89b4fa"   // blue
        return "#6c7086"
    }

    function countByReason(reason) {
        var count = 0
        for (var i = 0; i < hardCases.length; i++) {
            if (hardCases[i].reason === reason) count++
        }
        return count
    }

    ListModel {
        id: hardCaseListModel
    }

    Rectangle {
        anchors.fill: parent
        color: "#1e1e2e"
        radius: 8

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // Header with sort options
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Label {
                    text: "Hard-Case Queue"
                    color: "#cba6f7"
                    font.pixelSize: 16
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: "Sort:"
                    color: "#cdd6f4"
                    font.pixelSize: 12
                }

                ComboBox {
                    id: sortCombo
                    Layout.preferredWidth: 120
                    model: ["priority", "confidence", "reason"]
                    currentIndex: 0

                    contentItem: Label {
                        text: sortCombo.currentText
                        color: "#cdd6f4"
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                    }

                    background: Rectangle {
                        color: "#313244"
                        radius: 4
                        border.color: sortCombo.activeFocus ? "#cba6f7" : "#45475a"
                        border.width: 1
                    }

                    popup: Popup {
                        y: sortCombo.height
                        width: sortCombo.width
                        implicitHeight: Math.min(contentItem.implicitHeight, 200)
                        padding: 1

                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: sortCombo.popup.visible ? sortCombo.delegateModel : null
                            currentIndex: sortCombo.highlightedIndex
                        }

                        background: Rectangle {
                            color: "#1e1e2e"
                            border.color: "#45475a"
                            radius: 4
                        }
                    }

                    delegate: ItemDelegate {
                        width: sortCombo.width
                        contentItem: Label {
                            text: modelData
                            color: highlighted ? "#cba6f7" : "#cdd6f4"
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                        }
                        highlighted: sortCombo.highlightedIndex === index
                        background: Rectangle {
                            color: highlighted ? "#313244" : "#1e1e2e"
                        }
                    }

                    onActivated: {
                        sortBy = currentText
                        rebuildModel()
                    }
                }

                Button {
                    text: "Refresh"
                    Layout.preferredHeight: 28

                    background: Rectangle {
                        color: parent.pressed ? "#45475a" : "#313244"
                        radius: 4
                        border.color: "#cba6f7"
                        border.width: 1
                    }

                    contentItem: Label {
                        text: parent.text
                        color: "#cba6f7"
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: refresh()
                }
            }

            // High-priority warning banner
            Rectangle {
                Layout.fillWidth: true
                height: 36
                radius: 6
                visible: countByReason("false_negative") > 0
                color: "#f9e2af20"
                border.color: "#f9e2af"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8

                    Label {
                        text: "!"
                        color: "#f9e2af"
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Label {
                        text: countByReason("false_negative") + " false negative(s) need immediate review"
                        color: "#f9e2af"
                        font.pixelSize: 12
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Label {
                        text: "Model missed objects in these samples"
                        color: "#e0cb85"
                        font.pixelSize: 11
                    }
                }
            }

            // Queue list
            ListView {
                id: hardCaseList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: hardCaseListModel
                spacing: 4

                Label {
                    anchors.centerIn: parent
                    visible: hardCaseList.count === 0
                    text: batchId === "" ? "Select a batch to view hard cases" : "No hard cases found"
                    color: "#6c7086"
                    font.pixelSize: 14
                }

                delegate: Rectangle {
                    width: hardCaseList.width
                    height: 52
                    radius: 6
                    color: delegateMouseArea.containsMouse ? "#313244" : "#252536"
                    border.color: Qt.rgba(
                        parseFloat(getPriorityColor(model.reason).substring(1, 3)) / 255,
                        parseFloat(getPriorityColor(model.reason).substring(3, 5)) / 255,
                        parseFloat(getPriorityColor(model.reason).substring(5, 7)) / 255,
                        0.3
                    )
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        // Priority indicator dot
                        Rectangle {
                            width: 12
                            height: 12
                            radius: 6
                            color: getPriorityColor(model.reason)

                            // Pulse animation for false negatives
                            SequentialAnimation on opacity {
                                running: model.reason === "false_negative"
                                loops: Animation.Infinite
                                NumberAnimation { from: 1.0; to: 0.4; duration: 800 }
                                NumberAnimation { from: 0.4; to: 1.0; duration: 800 }
                            }
                        }

                        // Reason label
                        Rectangle {
                            height: 22
                            width: reasonLabel.implicitWidth + 12
                            radius: 4
                            color: Qt.rgba(
                                parseFloat(getPriorityColor(model.reason).substring(1, 3)) / 255,
                                parseFloat(getPriorityColor(model.reason).substring(3, 5)) / 255,
                                parseFloat(getPriorityColor(model.reason).substring(5, 7)) / 255,
                                0.15
                            )
                            border.color: getPriorityColor(model.reason)
                            border.width: 1

                            Label {
                                id: reasonLabel
                                anchors.centerIn: parent
                                text: getReasonLabel(model.reason)
                                color: getPriorityColor(model.reason)
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }

                        // Sample ID (truncated)
                        Label {
                            text: {
                                var sid = model.sampleId || ""
                                if (sid.length > 12) return sid.substring(0, 12) + "..."
                                return sid || ("Cand #" + model.candidateIndex)
                            }
                            color: "#cdd6f4"
                            font.pixelSize: 12
                            font.family: "monospace"
                            Layout.preferredWidth: 110
                        }

                        // Class name (for FP/LC)
                        Label {
                            visible: model.reason !== "false_negative" && model.className !== ""
                            text: model.className || ""
                            color: "#a6adc8"
                            font.pixelSize: 11
                            Layout.preferredWidth: 80
                        }

                        // Confidence score
                        Label {
                            visible: model.reason !== "false_negative"
                            text: "conf: " + parseFloat(model.confidence).toFixed(3)
                            color: parseFloat(model.confidence) < 0.2 ? "#f38ba8" : "#f9e2af"
                            font.pixelSize: 11
                            font.family: "monospace"
                        }

                        // Priority number
                        Label {
                            text: "P" + model.priority
                            color: "#6c7086"
                            font.pixelSize: 10
                            font.family: "monospace"
                        }

                        Item { Layout.fillWidth: true }

                        // Review button
                        Button {
                            text: "Review"
                            Layout.preferredHeight: 26
                            Layout.preferredWidth: 64

                            background: Rectangle {
                                color: parent.pressed ? "#b07ce0" : "#cba6f7"
                                radius: 4
                            }

                            contentItem: Label {
                                text: parent.text
                                color: "#1e1e2e"
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: {
                                // Navigate to annotation page for this sample
                                if (typeof annotationPage !== "undefined") {
                                    annotationPage.loadSample(model.sampleId || "")
                                }
                                if (typeof stackView !== "undefined") {
                                    stackView.push("qrc:/LabelTorch.Annotation/qml/AnnotationPage.qml", {
                                        "sampleId": model.sampleId || "",
                                        "batchId": root.batchId
                                    })
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: delegateMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#313244"
            }

            // Summary stats
            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                // False Negative count
                RowLayout {
                    spacing: 6

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: "#f38ba8"
                    }

                    Label {
                        text: "FN: " + countByReason("false_negative")
                        color: "#f38ba8"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                // Low-Confidence count
                RowLayout {
                    spacing: 6

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: "#f9e2af"
                    }

                    Label {
                        text: "Low-Conf: " + countByReason("low_confidence")
                        color: "#f9e2af"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                // False Positive count
                RowLayout {
                    spacing: 6

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: "#89b4fa"
                    }

                    Label {
                        text: "FP: " + countByReason("false_positive")
                        color: "#89b4fa"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: "Total: " + hardCases.length
                    color: "#cdd6f4"
                    font.pixelSize: 12
                }
            }
        }
    }
}
