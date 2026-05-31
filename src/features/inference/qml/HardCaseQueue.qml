// HardCaseQueue.qml - Hard-case priority queue for FP/FN/low-conf review
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme

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
        if (reason === "false_negative") return "漏检"
        if (reason === "low_confidence") return "低置信度"
        if (reason === "false_positive") return "误检"
        return reason
    }

    function getPriorityColor(reason) {
        if (reason === "false_negative") return Theme.accentError
        if (reason === "low_confidence") return Theme.accentWarning
        if (reason === "false_positive") return Theme.accentPrimary
        return Theme.textDisabled
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
        color: Theme.bgPrimary
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
                    text: "难例队列"
                    color: Theme.accentSecondary
                    font.pixelSize: 16
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: "排序："
                    color: Theme.textPrimary
                    font.pixelSize: 12
                }

                ComboBox {
                    id: sortCombo
                    Layout.preferredWidth: 120
                    model: ["priority", "confidence", "reason"]
                    currentIndex: 0

                    contentItem: Label {
                        text: sortCombo.currentText
                        color: Theme.textPrimary
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                    }

                    background: Rectangle {
                        color: Theme.bgHover
                        radius: 4
                        border.color: sortCombo.activeFocus ? Theme.accentSecondary : Theme.textDisabled
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
                            color: Theme.bgPrimary
                            border.color: Theme.textDisabled
                            radius: 4
                        }
                    }

                    delegate: ItemDelegate {
                        width: sortCombo.width
                        contentItem: Label {
                            text: modelData
                            color: highlighted ? Theme.accentSecondary : Theme.textPrimary
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                        }
                        highlighted: sortCombo.highlightedIndex === index
                        background: Rectangle {
                            color: highlighted ? Theme.bgHover : Theme.bgPrimary
                        }
                    }

                    onActivated: {
                        sortBy = currentText
                        rebuildModel()
                    }
                }

                Button {
                    text: "刷新"
                    Layout.preferredHeight: 28

                    background: Rectangle {
                        color: parent.pressed ? Theme.textDisabled : Theme.bgHover
                        radius: 4
                        border.color: Theme.accentSecondary
                        border.width: 1
                    }

                    contentItem: Label {
                        text: parent.text
                        color: Theme.accentSecondary
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
                color: Theme.accentWarning
                border.color: Theme.accentWarning
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8

                    Label {
                        text: "!"
                        color: Theme.accentWarning
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Label {
                        text: countByReason("false_negative") + " 个漏检需要立即审核"
                        color: Theme.accentWarning
                        font.pixelSize: 12
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Label {
                        text: "模型在这些样本中漏检了目标"
                        color: Theme.accentWarning
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
                    text: batchId === "" ? "选择批次查看难例" : "未发现难例"
                    color: Theme.textDisabled
                    font.pixelSize: 14
                }

                delegate: Rectangle {
                    width: hardCaseList.width
                    height: 52
                    radius: 6
                    color: delegateMouseArea.containsMouse ? Theme.bgHover : Theme.bgSecondary
                    border.color: {
                        var pc = getPriorityColor(model.reason)
                        Qt.rgba(pc.r, pc.g, pc.b, 0.3)
                    }
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
                            color: {
                                var pc = getPriorityColor(model.reason)
                                Qt.rgba(pc.r, pc.g, pc.b, 0.15)
                            }
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
                                return sid || ("候选 #" + model.candidateIndex)
                            }
                            color: Theme.textPrimary
                            font.pixelSize: 12
                            font.family: "monospace"
                            Layout.preferredWidth: 110
                        }

                        // Class name (for FP/LC)
                        Label {
                            visible: model.reason !== "false_negative" && model.className !== ""
                            text: model.className || ""
                            color: Theme.textMuted
                            font.pixelSize: 11
                            Layout.preferredWidth: 80
                        }

                        // Confidence score
                        Label {
                            visible: model.reason !== "false_negative"
                            text: "置信度: " + (parseFloat(model.confidence) || 0).toFixed(3)
                            color: (parseFloat(model.confidence) || 0) < 0.2 ? Theme.accentError : Theme.accentWarning
                            font.pixelSize: 11
                            font.family: "monospace"
                        }

                        // Priority number
                        Label {
                            text: "P" + model.priority
                            color: Theme.textDisabled
                            font.pixelSize: 10
                            font.family: "monospace"
                        }

                        Item { Layout.fillWidth: true }

                        // Review button
                        Button {
                            text: "审核"
                            Layout.preferredHeight: 26
                            Layout.preferredWidth: 64

                            background: Rectangle {
                                color: parent.pressed ? Theme.accentSecondary : Theme.accentSecondary
                                radius: 4
                            }

                            contentItem: Label {
                                text: parent.text
                                color: Theme.bgPrimary
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
                color: Theme.bgHover
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
                        color: Theme.accentError
                    }

                    Label {
                        text: "漏检: " + countByReason("false_negative")
                        color: Theme.accentError
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
                        color: Theme.accentWarning
                    }

                    Label {
                        text: "低置信度: " + countByReason("low_confidence")
                        color: Theme.accentWarning
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
                        color: Theme.accentPrimary
                    }

                    Label {
                        text: "误检: " + countByReason("false_positive")
                        color: Theme.accentPrimary
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: "总计: " + hardCases.length
                    color: Theme.textPrimary
                    font.pixelSize: 12
                }
            }
        }
    }
}
