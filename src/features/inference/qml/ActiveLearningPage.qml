import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/// <summary>
/// 主动学习主页面
/// 提供低置信样本收集、队列管理、优先级排序功能
/// </summary>
Page {
    id: root

    title: "主动学习"

    // 属性
    property string currentQueueType: "low-confidence"
    property var queueStats: ({})

    header: ToolBar {
        Material.background: Theme.surface
        Material.foreground: Theme.onSurface

        RowLayout {
            anchors.fill: parent
            spacing: Theme.spacingMd

            Label {
                text: "主动学习中心"
                font: Theme.fontH6
                color: Theme.onSurface
            }

            Item { Layout.fillWidth: true }

            // 队列切换按钮
            ComboBox {
                id: queueSelector
                model: [
                    { text: "低置信队列", value: "low-confidence" },
                    { text: "误检队列", value: "false-positive" },
                    { text: "漏检队列", value: "false-negative" },
                    { text: "难例队列", value: "hard-case" }
                ]
                textRole: "text"
                valueRole: "value"
                onActivated: {
                    currentQueueType = model[index].value
                }
            }

            Button {
                text: "收集样本"
                highlighted: true
                onClicked: collectSamplesDialog.open()
            }

            Button {
                text: "优先级排序"
                onClicked: activeLearningService.prioritizeQueue(
                    sampleModel.samples,
                    currentQueueType,
                    classWeightsModel.weights,
                    "default"
                )
            }
        }
    }

    content: SplitView {
        anchors.fill: parent

        // 左侧：队列样本列表
        ListView {
            id: sampleListView
            SplitView.preferredWidth: 400
            SplitView.minimumWidth: 300

            model: SampleModel {
                id: sampleModel
                queueType: currentQueueType
            }

            delegate: SampleCard {
                width: sampleListView.width
                sampleData: modelData
                onRemove: activeLearningService.removeSampleFromQueue(currentQueueType, modelData.path)
                onReview: reviewSample(modelData)
            }

            section.property: "priority_level"
            section.delegate: SectionHeader {
                text: section === "high" ? "高优先级" :
                      section === "medium" ? "中优先级" : "低优先级"
            }
        }

        // 右侧：统计和操作面板
        ColumnLayout {
            SplitView.fillWidth: true
            spacing: Theme.spacingMd

            // 队列统计面板
            QueueStatsPanel {
                id: statsPanel
                Layout.fillWidth: true
                stats: queueStats
            }

            // 操作面板
            GroupBox {
                Layout.fillWidth: true
                title: "操作"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingSm

                    Button {
                        text: "批量确认"
                        Layout.fillWidth: true
                        onClicked: batchConfirmSelected()
                    }

                    Button {
                        text: "批量拒绝"
                        Layout.fillWidth: true
                        onClicked: batchRejectSelected()
                    }

                    Button {
                        text: "清空队列"
                        Layout.fillWidth: true
                        highlighted: false
                        onClicked: clearConfirmDialog.open()
                    }

                    Item { Layout.fillHeight: true }

                    Button {
                        text: "生成训练快照"
                        Layout.fillWidth: true
                        highlighted: true
                        enabled: sampleModel.count > 0
                        onClicked: createTrainingSnapshot()
                    }
                }
            }
        }
    }

    // 收集样本对话框
    Dialog {
        id: collectSamplesDialog
        title: "收集低置信样本"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        ColumnLayout {
            spacing: Theme.spacingMd

            TextField {
                id: weightPathField
                Layout.fillWidth: true
                placeholderText: "模型权重路径"
            }

            TextField {
                id: sourcePathField
                Layout.fillWidth: true
                placeholderText: "图片路径或目录"
            }

            RowLayout {
                spacing: Theme.spacingMd

                Label { text: "置信度阈值:" }
                Slider {
                    id: confSlider
                    from: 0.1
                    to: 0.8
                    value: 0.3
                    stepSize: 0.05
                }
                Label { text: confSlider.value.toFixed(2) }
            }

            RowLayout {
                spacing: Theme.spacingMd

                Label { text: "推理设备:" }
                ComboBox {
                    id: deviceSelector
                    model: ["auto", "cpu", "0"]
                }
            }
        }

        onAccepted: {
            activeLearningService.collectLowConfSamples(
                weightPathField.text,
                sourcePathField.text,
                confSlider.value,
                0.45,
                640,
                deviceSelector.currentText
            )
        }
    }

    // 清空确认对话框
    Dialog {
        id: clearConfirmDialog
        title: "确认清空队列"
        text: "确定要清空当前队列吗？此操作不可撤销。"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: {
            activeLearningService.clearQueue(currentQueueType)
            sampleModel.refresh()
        }
    }

    // 方法
    function reviewSample(sampleData) {
        // 打开样本审核界面
        console.log("Review sample:", sampleData.path)
    }

    function batchConfirmSelected() {
        // 批量确认选中的样本
        console.log("Batch confirm selected samples")
    }

    function batchRejectSelected() {
        // 批量拒绝选中的样本
        console.log("Batch reject selected samples")
    }

    function createTrainingSnapshot() {
        // 创建训练快照
        console.log("Create training snapshot from active learning queue")
    }

    // 信号连接
    Connections {
        target: activeLearningService

        function onSamplesCollected(samples, totalSamples) {
            sampleModel.refresh()
            statsPanel.updateStats()
        }

        function onQueuePrioritized(sortedSamples, total) {
            sampleModel.refresh()
        }

        function onQueueStatsReady(stats) {
            queueStats = stats
        }

        function onError(message) {
            errorBanner.show(message)
        }
    }
}
