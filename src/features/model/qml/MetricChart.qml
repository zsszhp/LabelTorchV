// MetricChart.qml - Metrics display (text-based, chart rendering deferred)
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme
import QtQuick.Layouts

Item {
    id: root

    property string versionId: ""
    property string metricsJson: "{}"

    Rectangle {
        anchors.fill: parent
        color: Theme.bgPrimary
        radius: 6

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // Header
            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: "指标"
                    color: Theme.accentPrimary
                    font.pixelSize: 14
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: "文本视图（图表渲染延迟）"
                    color: Theme.textMuted
                    font.pixelSize: 11
                    font.italic: true
                }
            }

            // Key metrics grid
            GridLayout {
                Layout.fillWidth: true
                columns: 5
                columnSpacing: 8
                rowSpacing: 6
                visible: keyMetricsRepeater.count > 0

                Repeater {
                    id: keyMetricsRepeater
                    model: {
                        var metrics = ["mAP50", "mAP50-95", "precision", "recall", "fitness"]
                        var result = []
                        try {
                            var obj = JSON.parse(root.metricsJson)
                            for (var i = 0; i < metrics.length; i++) {
                                var key = metrics[i]
                                if (obj.hasOwnProperty(key) && key !== "tags") {
                                    result.push({"name": key, "value": obj[key]})
                                }
                            }
                        } catch(e) {}
                        return result
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 60
                        color: Theme.bgCard
                        radius: 6

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 2

                            Label {
                                text: modelData.name
                                color: Theme.textMuted
                                font.pixelSize: 10
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Label {
                                text: {
                                    var val = modelData.value
                                    if (typeof val === 'number' && isFinite(val)) {
                                        return val.toFixed(4)
                                    }
                                    return String(val)
                                }
                                color: Theme.accentSuccess
                                font.pixelSize: 16
                                font.bold: true
                                font.family: "monospace"
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                    }
                }
            }

            // No key metrics message
            Label {
                Layout.fillWidth: true
                visible: keyMetricsRepeater.count === 0
                text: "指标快照中未找到关键指标（mAP50, mAP50-95, precision, recall, fitness）"
                color: Theme.textMuted
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.bgInput
                visible: keyMetricsRepeater.count > 0
            }

            // Raw metrics JSON display
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.metricsJson !== "{}" && root.metricsJson !== ""

                clip: true

                TextArea {
                    readOnly: true
                    text: {
                        try {
                            var obj = JSON.parse(root.metricsJson)
                            // Remove tags from raw display (shown separately)
                            var display = Object.assign({}, obj)
                            return JSON.stringify(display, null, 2)
                        } catch(e) {
                            return root.metricsJson
                        }
                    }
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    font.family: "monospace"
                    selectByMouse: true
                    wrapMode: Text.WordWrap
                    background: Rectangle {
                        color: "transparent"
                    }
                }
            }

            // Epoch history section (if available via metricService)
            Label {
                Layout.fillWidth: true
                visible: root.versionId !== ""
                text: "训练曲线摘要：需要epoch数据（延迟实现）"
                color: Theme.textMuted
                font.pixelSize: 11
                font.italic: true
            }
        }
    }
}
