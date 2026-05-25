import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import LabelTorch.Theme 1.0

/**
 * 异常检测推理面板
 *
 * 提供异常检测模型的推理功能，支持单张/批量图片推理，
 * 展示异常分数和热力图叠加显示
 */
Rectangle {
    id: root

    color: Theme.bgPrimary

    // 外部属性
    property string currentProjectId: ""
    property string currentWeightPath: ""
    property var anomalyModels: anomalyService ? anomalyService.listModels() : []

    // 内部状态
    property string selectedModel: "patchcore"
    property string selectedDevice: "auto"
    property int selectedImgSize: 256
    property var inferenceResults: []
    property bool isInfering: false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMd
        spacing: Theme.spacingMd

        // 标题
        Label {
            text: "异常检测推理"
            font.pixelSize: Theme.fontH3
            color: Theme.textPrimary
            Layout.fillWidth: true
        }

        // 配置区域
        GroupBox {
            Layout.fillWidth: true
            title: "推理配置"

            label: Label {
                text: parent.title
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
            }

            background: Rectangle {
                color: Theme.bgSecondary
                radius: Theme.radiusMd
                border.color: Theme.borderLight
            }

            GridLayout {
                anchors.fill: parent
                columns: 2
                rowSpacing: Theme.spacingSm
                columnSpacing: Theme.spacingMd

                Label {
                    text: "模型算法："
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                }
                ComboBox {
                    id: modelCombo
                    Layout.fillWidth: true
                    model: root.anomalyModels
                    currentIndex: 0
                    onCurrentTextChanged: root.selectedModel = currentText
                    background: Rectangle {
                        color: Theme.bgTertiary
                        radius: Theme.radiusSm
                    }
                }

                Label {
                    text: "推理设备："
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                }
                ComboBox {
                    id: deviceCombo
                    Layout.fillWidth: true
                    model: ["auto", "cpu", "0", "1"]
                    currentIndex: 0
                    onCurrentTextChanged: root.selectedDevice = currentText
                    background: Rectangle {
                        color: Theme.bgTertiary
                        radius: Theme.radiusSm
                    }
                }

                Label {
                    text: "图片尺寸："
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                }
                SpinBox {
                    id: imgSizeSpin
                    Layout.fillWidth: true
                    from: 64
                    to: 2048
                    stepSize: 64
                    value: 256
                    onValueChanged: root.selectedImgSize = value
                    background: Rectangle {
                        color: Theme.bgTertiary
                        radius: Theme.radiusSm
                    }
                }

                Label {
                    text: "权重文件："
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                }
                Label {
                    Layout.fillWidth: true
                    text: root.currentWeightPath ? root.currentWeightPath.split("/").pop() : "未选择"
                    color: root.currentWeightPath ? Theme.textPrimary : Theme.textMuted
                    font.pixelSize: Theme.fontBody
                    elide: Text.ElideMiddle
                }
            }
        }

        // 操作按钮
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm

            Button {
                text: root.isInfering ? "推理中..." : "开始推理"
                enabled: !root.isInfering && root.currentWeightPath !== ""
                onClicked: startInference()
                background: Rectangle {
                    implicitWidth: 120
                    implicitHeight: 36
                    color: parent.enabled ? Theme.accentPrimary : Theme.bgTertiary
                    radius: Theme.radiusSm
                }
                contentItem: Label {
                    text: parent.text
                    color: parent.enabled ? Theme.textOnAccent : Theme.textMuted
                    font.pixelSize: Theme.fontBody
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                text: "清空结果"
                onClicked: {
                    root.inferenceResults = [];
                    resultListView.model = [];
                }
                background: Rectangle {
                    implicitWidth: 80
                    implicitHeight: 36
                    color: Theme.bgTertiary
                    radius: Theme.radiusSm
                }
                contentItem: Label {
                    text: parent.text
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontBody
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // 结果列表
        GroupBox {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "推理结果"

            label: Label {
                text: parent.title
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSmall
            }

            background: Rectangle {
                color: Theme.bgSecondary
                radius: Theme.radiusMd
                border.color: Theme.borderLight
            }

            ListView {
                id: resultListView
                anchors.fill: parent
                clip: true
                spacing: Theme.spacingXs

                delegate: Rectangle {
                    width: resultListView.width
                    height: 72
                    color: index % 2 === 0 ? Theme.bgSecondary : Theme.bgTertiary
                    radius: Theme.radiusSm

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingSm
                        spacing: Theme.spacingMd

                        // 异常分数指示
                        Rectangle {
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            radius: 24
                            color: {
                                var score = parseFloat(modelData.anomaly_score) || 0;
                                if (score > 0.7) return "#e74c3c";
                                if (score > 0.3) return "#f39c12";
                                return "#27ae60";
                            }

                            Label {
                                anchors.centerIn: parent
                                text: ((parseFloat(modelData.anomaly_score) || 0) * 100).toFixed(1) + "%"
                                color: "white"
                                font.pixelSize: Theme.fontSmall
                                font.bold: true
                            }
                        }

                        // 图片信息
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                text: modelData.image_path ? modelData.image_path.split("/").pop() : ""
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontBody
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            Label {
                                text: "标签: " + (modelData.pred_label === "anomalous" ? "异常" : "正常")
                                color: modelData.pred_label === "anomalous" ? "#e74c3c" : "#27ae60"
                                font.pixelSize: Theme.fontSmall
                            }

                            Label {
                                text: modelData.error ? "错误: " + modelData.error : ""
                                color: "#e74c3c"
                                font.pixelSize: Theme.fontSmall
                                visible: modelData.error !== undefined && modelData.error !== ""
                            }
                        }

                        // 热力图按钮
                        Button {
                            text: "查看热力图"
                            visible: modelData.anomaly_map_path && modelData.anomaly_map_path !== ""
                            onClicked: Qt.openUrlExternally("file:///" + modelData.anomaly_map_path)
                            background: Rectangle {
                                implicitWidth: 80
                                implicitHeight: 28
                                color: Theme.accentPrimary
                                radius: Theme.radiusSm
                            }
                            contentItem: Label {
                                text: parent.text
                                color: Theme.textOnAccent
                                font.pixelSize: Theme.fontSmall
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
            }
        }
    }

    // 推理函数
    function startInference() {
        if (!anomalyService || !root.currentWeightPath) return;

        root.isInfering = true;

        // 构建图片路径JSON数组
        var paths = [];
        // TODO: 从当前数据集获取图片路径
        // 这里需要与DatasetService集成

        var pathsJson = JSON.stringify(paths);
        anomalyService.runInference(
            root.currentWeightPath,
            pathsJson,
            root.selectedModel,
            root.selectedDevice,
            root.selectedImgSize
        );
    }

    // 连接AnomalyService信号
    Connections {
        target: anomalyService
        function onInferenceResult(predictions) {
            root.isInfering = false;
            root.inferenceResults = predictions;
            resultListView.model = predictions;
        }
        function onInferenceFailed(error) {
            root.isInfering = false;
        }
    }
}
