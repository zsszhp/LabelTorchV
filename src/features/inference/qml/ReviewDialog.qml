// ReviewDialog.qml - Dialog for reviewing inference candidates
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    title: "Review Candidate"
    modal: true
    width: 480
    height: 400

    property string batchId: ""
    property int candidateIndex: -1
    property var candidate: null

    // Catppuccin Mocha palette
    palette.window: "#1e1e2e"
    palette.windowText: "#cdd6f4"
    palette.base: "#181825"
    palette.text: "#cdd6f4"
    palette.button: "#313244"
    palette.buttonText: "#cdd6f4"
    palette.highlight: "#89b4fa"
    palette.highlightedText: "#1e1e2e"

    background: Rectangle {
        color: "#1e1e2e"
        radius: 8
        border.color: "#313244"
        border.width: 1
    }

    header: Rectangle {
        color: "#181825"
        height: 44
        radius: 8

        Label {
            anchors.centerIn: parent
            text: "Review Candidate"
            color: "#89b4fa"
            font.pixelSize: 15
            font.bold: true
        }
    }

    contentItem: Rectangle {
        color: "#1e1e2e"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            // Image placeholder area
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                color: "#181825"
                radius: 6
                border.color: "#313244"
                border.width: 1

                Label {
                    anchors.centerIn: parent
                    text: "Image Preview"
                    color: "#6c7086"
                    font.pixelSize: 14
                }
            }

            // Candidate info
            Rectangle {
                Layout.fillWidth: true
                height: 80
                color: "#181825"
                radius: 6

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: "Class:"
                            color: "#a6adc8"
                            font.pixelSize: 13
                        }

                        Label {
                            text: root.candidate ? (root.candidate.className || ("Class " + root.candidate.classIndex)) : "N/A"
                            color: "#cdd6f4"
                            font.pixelSize: 13
                            font.bold: true
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            text: "Confidence:"
                            color: "#a6adc8"
                            font.pixelSize: 13
                        }

                        Label {
                            text: root.candidate ? parseFloat(root.candidate.confidence).toFixed(3) : "N/A"
                            color: root.candidate && root.candidate.confidence >= 0.5 ? "#a6e3a1" : "#f9e2af"
                            font.pixelSize: 13
                            font.bold: true
                            font.family: "monospace"
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: "Box:"
                            color: "#a6adc8"
                            font.pixelSize: 13
                        }

                        Label {
                            text: root.candidate ?
                                "cx=" + parseFloat(root.candidate.cx).toFixed(3) +
                                " cy=" + parseFloat(root.candidate.cy).toFixed(3) +
                                " w=" + parseFloat(root.candidate.w).toFixed(3) +
                                " h=" + parseFloat(root.candidate.h).toFixed(3) : "N/A"
                            color: "#6c7086"
                            font.pixelSize: 12
                            font.family: "monospace"
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: "State:"
                            color: "#a6adc8"
                            font.pixelSize: 13
                        }

                        Rectangle {
                            height: 20
                            width: stateLabel.implicitWidth + 12
                            radius: 3
                            color: {
                                switch (root.candidate ? root.candidate.state : "") {
                                case "confirmed": return "#a6e3a130"
                                case "rejected": return "#f38ba830"
                                case "edited": return "#f9e2af30"
                                default: return "#89b4fa30"
                                }
                            }
                            border.color: {
                                switch (root.candidate ? root.candidate.state : "") {
                                case "confirmed": return "#a6e3a1"
                                case "rejected": return "#f38ba8"
                                case "edited": return "#f9e2af"
                                default: return "#89b4fa"
                                }
                            }
                            border.width: 1

                            Label {
                                id: stateLabel
                                anchors.centerIn: parent
                                text: root.candidate ? root.candidate.state : "pending"
                                color: {
                                    switch (root.candidate ? root.candidate.state : "") {
                                    case "confirmed": return "#a6e3a1"
                                    case "rejected": return "#f38ba8"
                                    case "edited": return "#f9e2af"
                                    default: return "#89b4fa"
                                    }
                                }
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }
                }
            }
        }
    }

    footer: Rectangle {
        color: "#181825"
        height: 52
        radius: 8

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 8

            // Confirm button
            Button {
                text: "Confirm"
                Layout.fillWidth: true
                Layout.preferredHeight: 34

                background: Rectangle {
                    color: parent.pressed ? "#74c7a0" : "#a6e3a1"
                    radius: 6
                }

                contentItem: Label {
                    text: parent.text
                    color: "#1e1e2e"
                    font.pixelSize: 13
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    if (root.batchId !== "" && root.candidateIndex >= 0) {
                        assistedLabelService.confirmCandidate(root.batchId, root.candidateIndex)
                        root.candidate.state = "confirmed"
                        root.accept()
                    }
                }
            }

            // Reject button
            Button {
                text: "Reject"
                Layout.fillWidth: true
                Layout.preferredHeight: 34

                background: Rectangle {
                    color: parent.pressed ? "#d6758e" : "#f38ba8"
                    radius: 6
                }

                contentItem: Label {
                    text: parent.text
                    color: "#1e1e2e"
                    font.pixelSize: 13
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    if (root.batchId !== "" && root.candidateIndex >= 0) {
                        assistedLabelService.rejectCandidate(root.batchId, root.candidateIndex)
                        root.candidate.state = "rejected"
                        root.reject()
                    }
                }
            }

            // Edit button
            Button {
                text: "Edit"
                Layout.fillWidth: true
                Layout.preferredHeight: 34

                background: Rectangle {
                    color: parent.pressed ? "#e0cb85" : "#f9e2af"
                    radius: 6
                }

                contentItem: Label {
                    text: parent.text
                    color: "#1e1e2e"
                    font.pixelSize: 13
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    // Mark as edited - actual editing handled by annotation canvas
                    root.candidate.state = "edited"
                    root.close()
                }
            }
        }
    }
}
