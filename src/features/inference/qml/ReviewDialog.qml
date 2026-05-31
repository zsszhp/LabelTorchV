// ReviewDialog.qml - Dialog for reviewing inference candidates
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme

Dialog {
    id: root

    title: "审核候选结果"
    modal: true
    width: 480
    height: 400

    property string batchId: ""
    property int candidateIndex: -1
    property var candidate: null

    palette.window: Theme.bgPrimary
    palette.windowText: Theme.textPrimary
    palette.base: Theme.bgInput
    palette.text: Theme.textPrimary
    palette.button: Theme.bgTertiary
    palette.buttonText: Theme.textPrimary
    palette.highlight: Theme.accentPrimary
    palette.highlightedText: Theme.textPrimary

    background: Rectangle {
        color: Theme.bgPrimary
        radius: 8
        border.color: Theme.bgHover
        border.width: 1
    }

    header: Rectangle {
        color: Theme.bgInput
        height: 44
        radius: 8

        Label {
            anchors.centerIn: parent
            text: "审核候选结果"
            color: Theme.accentPrimary
            font.pixelSize: 15
            font.bold: true
        }
    }

    contentItem: Rectangle {
        color: Theme.bgPrimary

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            // Image placeholder area
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                color: Theme.bgInput
                radius: 6
                border.color: Theme.bgHover
                border.width: 1

                Label {
                    anchors.centerIn: parent
                    text: "图片预览"
                    color: Theme.textDisabled
                    font.pixelSize: 14
                }
            }

            // Candidate info
            Rectangle {
                Layout.fillWidth: true
                height: 80
                color: Theme.bgInput
                radius: 6

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: "类别："
                            color: Theme.textMuted
                            font.pixelSize: 13
                        }

                        Label {
                            text: root.candidate ? (root.candidate.className || ("Class " + root.candidate.classIndex)) : "N/A"
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            font.bold: true
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            text: "置信度："
                            color: Theme.textMuted
                            font.pixelSize: 13
                        }

                        Label {
                            text: root.candidate ? (parseFloat(root.candidate.confidence) || 0).toFixed(3) : "N/A"
                            color: root.candidate && (parseFloat(root.candidate.confidence) || 0) >= 0.5 ? Theme.accentSuccess : Theme.accentWarning
                            font.pixelSize: 13
                            font.bold: true
                            font.family: "monospace"
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: "边界框："
                            color: Theme.textMuted
                            font.pixelSize: 13
                        }

                        Label {
                            text: root.candidate ?
                                "cx=" + (parseFloat(root.candidate.cx) || 0).toFixed(3) +
                                " cy=" + (parseFloat(root.candidate.cy) || 0).toFixed(3) +
                                " w=" + (parseFloat(root.candidate.w) || 0).toFixed(3) +
                                " h=" + (parseFloat(root.candidate.h) || 0).toFixed(3) : "N/A"
                            color: Theme.textDisabled
                            font.pixelSize: 12
                            font.family: "monospace"
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: "状态："
                            color: Theme.textMuted
                            font.pixelSize: 13
                        }

                        Rectangle {
                            height: 20
                            width: stateLabel.implicitWidth + 12
                            radius: 3
                            color: {
                                switch (root.candidate ? root.candidate.state : "") {
                                case "confirmed": return Theme.accentSuccess
                                case "rejected": return Theme.accentError
                                case "edited": return Theme.accentWarning
                                default: return Theme.accentPrimary
                                }
                            }
                            border.color: {
                                switch (root.candidate ? root.candidate.state : "") {
                                case "confirmed": return Theme.accentSuccess
                                case "rejected": return Theme.accentError
                                case "edited": return Theme.accentWarning
                                default: return Theme.accentPrimary
                                }
                            }
                            border.width: 1

                            Label {
                                id: stateLabel
                                anchors.centerIn: parent
                                text: root.candidate ? root.candidate.state : "pending"
                                color: {
                                    switch (root.candidate ? root.candidate.state : "") {
                                    case "confirmed": return Theme.accentSuccess
                                    case "rejected": return Theme.accentError
                                    case "edited": return Theme.accentWarning
                                    default: return Theme.accentPrimary
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
        color: Theme.bgInput
        height: 52
        radius: 8

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 8

            // Confirm button
            Button {
                text: "确认"
                Layout.fillWidth: true
                Layout.preferredHeight: 34

                background: Rectangle {
                    color: parent.pressed ? Qt.darker(Theme.accentSuccess, 1.2) : Theme.accentSuccess
                    radius: 6
                }

                contentItem: Label {
                    text: parent.text
                    color: Theme.bgPrimary
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
                text: "拒绝"
                Layout.fillWidth: true
                Layout.preferredHeight: 34

                background: Rectangle {
                    color: parent.pressed ? Qt.darker(Theme.accentError, 1.2) : Theme.accentError
                    radius: 6
                }

                contentItem: Label {
                    text: parent.text
                    color: Theme.bgPrimary
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
                text: "编辑"
                Layout.fillWidth: true
                Layout.preferredHeight: 34

                background: Rectangle {
                    color: parent.pressed ? Qt.darker(Theme.accentWarning, 1.2) : Theme.accentWarning
                    radius: 6
                }

                contentItem: Label {
                    text: parent.text
                    color: Theme.bgPrimary
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
