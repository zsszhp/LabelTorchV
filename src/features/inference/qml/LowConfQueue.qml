// LowConfQueue.qml - Low-confidence sample queue browser
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string batchId: ""
    property real confThreshold: 0.3
    property var lowConfSamples: []

    function refresh() {
        if (batchId === "") return
        lowConfSamples = assistedLabelService.getLowConfidenceSamples(batchId, confThreshold)
        queueListModel.clear()
        for (var i = 0; i < lowConfSamples.length; i++) {
            queueListModel.append(lowConfSamples[i])
        }
    }

    function dismissItem(index) {
        queueListModel.remove(index)
    }

    ListModel {
        id: queueListModel
    }

    Rectangle {
        anchors.fill: parent
        color: "#1e1e2e"
        radius: 8

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // Header with threshold slider
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Label {
                    text: "Low-Confidence Queue"
                    color: "#f9e2af"
                    font.pixelSize: 16
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: "Threshold:"
                    color: "#cdd6f4"
                    font.pixelSize: 12
                }

                Slider {
                    id: thresholdSlider
                    Layout.preferredWidth: 160
                    from: 0.0
                    to: 1.0
                    value: root.confThreshold
                    stepSize: 0.05

                    onValueChanged: {
                        root.confThreshold = value
                    }

                    background: Rectangle {
                        x: thresholdSlider.leftPadding
                        y: thresholdSlider.topPadding + thresholdSlider.availableHeight / 2 - height / 2
                        width: thresholdSlider.availableWidth
                        height: 4
                        radius: 2
                        color: "#313244"

                        Rectangle {
                            width: thresholdSlider.visualPosition * parent.width
                            height: parent.height
                            color: "#f9e2af"
                            radius: 2
                        }
                    }

                    handle: Rectangle {
                        x: thresholdSlider.leftPadding + thresholdSlider.visualPosition * (thresholdSlider.availableWidth - width)
                        y: thresholdSlider.topPadding + thresholdSlider.availableHeight / 2 - height / 2
                        width: 14
                        height: 14
                        radius: 7
                        color: thresholdSlider.pressed ? "#e0cb85" : "#f9e2af"
                    }
                }

                Label {
                    text: thresholdSlider.value.toFixed(2)
                    color: "#a6adc8"
                    font.pixelSize: 12
                    font.family: "monospace"
                    Layout.preferredWidth: 36
                }

                Button {
                    text: "Refresh"
                    Layout.preferredHeight: 28

                    background: Rectangle {
                        color: parent.pressed ? "#45475a" : "#313244"
                        radius: 4
                        border.color: "#f9e2af"
                        border.width: 1
                    }

                    contentItem: Label {
                        text: parent.text
                        color: "#f9e2af"
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: refresh()
                }
            }

            // Count label
            Label {
                Layout.fillWidth: true
                text: queueListModel.count + " low-confidence candidate" + (queueListModel.count !== 1 ? "s" : "") + " found"
                color: queueListModel.count > 0 ? "#f9e2af" : "#6c7086"
                font.pixelSize: 12
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#313244"
            }

            // Queue list
            ListView {
                id: queueList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: queueListModel
                spacing: 4

                Label {
                    anchors.centerIn: parent
                    visible: queueList.count === 0
                    text: batchId === "" ? "Select a batch to view low-confidence samples" : "No low-confidence samples below threshold"
                    color: "#6c7086"
                    font.pixelSize: 14
                }

                delegate: Rectangle {
                    width: queueList.width
                    height: 52
                    radius: 6
                    color: delegateMouseArea.containsMouse ? "#313244" : "#252536"
                    border.color: "#313244"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        // Confidence indicator dot
                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: {
                                var conf = parseFloat(model.confidence)
                                if (conf < 0.1) return "#f38ba8"
                                if (conf < 0.2) return "#fab387"
                                return "#f9e2af"
                            }
                        }

                        // Candidate index (truncated ID)
                        Label {
                            text: "#" + model.candidateIndex
                            color: "#89b4fa"
                            font.pixelSize: 12
                            font.family: "monospace"
                            font.bold: true
                            Layout.preferredWidth: 48
                        }

                        // Class name
                        Label {
                            text: model.className || ("Class " + model.classIndex)
                            color: "#cdd6f4"
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }

                        // Confidence score
                        Label {
                            text: "conf: " + parseFloat(model.confidence).toFixed(3)
                            color: parseFloat(model.confidence) < 0.2 ? "#f38ba8" : "#f9e2af"
                            font.pixelSize: 11
                            font.family: "monospace"
                        }

                        // Box coordinates
                        Label {
                            text: "[" + parseFloat(model.cx).toFixed(2) + ", " +
                                  parseFloat(model.cy).toFixed(2) + ", " +
                                  parseFloat(model.w).toFixed(2) + ", " +
                                  parseFloat(model.h).toFixed(2) + "]"
                            color: "#6c7086"
                            font.pixelSize: 10
                            font.family: "monospace"
                        }

                        Item { Layout.fillWidth: true }

                        // Send to Annotation button
                        Button {
                            text: "Annotate"
                            Layout.preferredHeight: 26
                            Layout.preferredWidth: 70

                            background: Rectangle {
                                color: parent.pressed ? "#74c7a0" : "#a6e3a1"
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
                                // Navigate to annotation page for this candidate
                                if (typeof annotationPage !== "undefined") {
                                    annotationPage.loadCandidate(root.batchId, model.candidateIndex)
                                }
                            }
                        }

                        // Dismiss button
                        Button {
                            text: "Dismiss"
                            Layout.preferredHeight: 26
                            Layout.preferredWidth: 64

                            background: Rectangle {
                                color: parent.pressed ? "#45475a" : "#313244"
                                radius: 4
                                border.color: "#f38ba8"
                                border.width: 1
                            }

                            contentItem: Label {
                                text: parent.text
                                color: "#f38ba8"
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            onClicked: dismissItem(index)
                        }
                    }

                    MouseArea {
                        id: delegateMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }
            }
        }
    }
}
