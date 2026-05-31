// LowConfQueue.qml - 低置信度样本队列浏览器
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme

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
        color: Theme.bgPrimary
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
                    text: "低置信度队列"
                    color: Theme.accentWarning
                    font.pixelSize: 16
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: "阈值："
                    color: Theme.textPrimary
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
                        color: Theme.bgHover

                        Rectangle {
                            width: thresholdSlider.visualPosition * parent.width
                            height: parent.height
                            color: Theme.accentWarning
                            radius: 2
                        }
                    }

                    handle: Rectangle {
                        x: thresholdSlider.leftPadding + thresholdSlider.visualPosition * (thresholdSlider.availableWidth - width)
                        y: thresholdSlider.topPadding + thresholdSlider.availableHeight / 2 - height / 2
                        width: 14
                        height: 14
                        radius: 7
                        color: thresholdSlider.pressed ? Qt.darker(Theme.accentWarning, 1.15) : Theme.accentWarning
                    }
                }

                Label {
                    text: thresholdSlider.value.toFixed(2)
                    color: Theme.textMuted
                    font.pixelSize: 12
                    font.family: "monospace"
                    Layout.preferredWidth: 36
                }

                Button {
                    text: "刷新"
                    Layout.preferredHeight: 28

                    background: Rectangle {
                        color: parent.pressed ? Theme.textDisabled : Theme.bgHover
                        radius: 4
                        border.color: Theme.accentWarning
                        border.width: 1
                    }

                    contentItem: Label {
                        text: parent.text
                        color: Theme.accentWarning
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
                text: qsTr("找到 %1 个低置信度候选").arg(queueListModel.count)
                color: queueListModel.count > 0 ? Theme.accentWarning : Theme.textDisabled
                font.pixelSize: 12
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.bgHover
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
                    text: batchId === "" ? "请选择批次以查看低置信度样本" : "当前阈值下无低置信度样本"
                    color: Theme.textDisabled
                    font.pixelSize: 14
                }

                delegate: Rectangle {
                    width: queueList.width
                    height: 52
                    radius: 6
                    color: delegateMouseArea.containsMouse ? Theme.bgHover : Theme.bgSecondary
                    border.color: Theme.bgHover
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
                                var conf = parseFloat(model.confidence) || 0
                                if (conf < 0.2) return Theme.accentError
                                return Theme.accentWarning
                            }
                        }

                        // Candidate index (truncated ID)
                        Label {
                            text: "#" + model.candidateIndex
                            color: Theme.accentPrimary
                            font.pixelSize: 12
                            font.family: "monospace"
                            font.bold: true
                            Layout.preferredWidth: 48
                        }

                        // Class name
                        Label {
                            text: model.className || ("类别 " + model.classIndex)
                            color: Theme.textPrimary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }

                        // Confidence score
                        Label {
                            text: "置信度: " + (parseFloat(model.confidence) || 0).toFixed(3)
                            color: (parseFloat(model.confidence) || 0) < 0.2 ? Theme.accentError : Theme.accentWarning
                            font.pixelSize: 11
                            font.family: "monospace"
                        }

                        // Box coordinates
                        Label {
                            text: "[" + (parseFloat(model.cx) || 0).toFixed(2) + ", " +
                                  (parseFloat(model.cy) || 0).toFixed(2) + ", " +
                                  (parseFloat(model.w) || 0).toFixed(2) + ", " +
                                  (parseFloat(model.h) || 0).toFixed(2) + "]"
                            color: Theme.textDisabled
                            font.pixelSize: 10
                            font.family: "monospace"
                        }

                        Item { Layout.fillWidth: true }

                        // Send to Annotation button
                        Button {
                            text: "标注"
                            Layout.preferredHeight: 26
                            Layout.preferredWidth: 70

                            background: Rectangle {
                                color: parent.pressed ? Qt.darker(Theme.accentSuccess, 1.2) : Theme.accentSuccess
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
                                // Navigate to annotation page for this candidate
                                if (typeof annotationPage !== "undefined") {
                                    annotationPage.loadCandidate(root.batchId, model.candidateIndex)
                                }
                            }
                        }

                        // Dismiss button
                        Button {
                            text: "忽略"
                            Layout.preferredHeight: 26
                            Layout.preferredWidth: 64

                            background: Rectangle {
                                color: parent.pressed ? Theme.textDisabled : Theme.bgHover
                                radius: 4
                                border.color: Theme.accentError
                                border.width: 1
                            }

                            contentItem: Label {
                                text: parent.text
                                color: Theme.accentError
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
