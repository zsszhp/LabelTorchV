// SnapshotPage.qml - 数据快照管理
import QtQuick
import QtQuick.Controls
import LabelTorch.Shell
import QtQuick.Layouts
import QtQuick.Dialogs

Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // 顶部工具栏
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Label {
                text: "数据集:"
                color: Theme.textPrimary
                font.pixelSize: 13
            }

            ComboBox {
                id: datasetCombo
                Layout.preferredWidth: 240
                model: datasetModel
                textRole: "name"
                valueRole: "id"
                onActivated: {
                    snapshotModel.setDatasetId(currentValue)
                }

                Component.onCompleted: {
                    if (count > 0) {
                        currentIndex = 0
                        snapshotModel.setDatasetId(currentValue)
                    }
                }
            }

            Label {
                text: "训练比例:"
                color: Theme.textPrimary
                font.pixelSize: 13
            }

            SpinBox {
                id: trainRatioSpin
                from: 50
                to: 95
                value: 80
                stepSize: 5
                editable: true

                property int realValue: value

                textFromValue: function(value) {
                    return (value / 100).toFixed(2)
                }

                valueFromText: function(text) {
                    return Math.round(parseFloat(text) * 100)
                }
            }

            Label {
                text: "%"
                color: Theme.textPrimary
                font.pixelSize: 13
            }

            Label {
                text: "划分策略:"
                color: Theme.textPrimary
                font.pixelSize: 13
            }

            ComboBox {
                id: splitStrategyCombo
                model: ["random", "sequential"]
                currentIndex: 0
                Layout.preferredWidth: 140
            }

            Button {
                text: "创建快照"
                highlighted: true
                enabled: datasetCombo.currentValue !== undefined
                onClicked: {
                    var ratio = trainRatioSpin.realValue / 100.0
                    var strategy = splitStrategyCombo.currentText
                    var datasetId = datasetCombo.currentValue
                    var snapId = snapshotService.createSnapshot(datasetId, ratio, strategy)
                    if (snapId !== "") {
                        snapshotModel.refresh()
                        statusLabel.text = "快照创建成功: " + snapId.substring(0, 8) + "..."
                        statusLabel.color = Theme.accentSuccess
                    } else {
                        statusLabel.text = "快照创建失败"
                        statusLabel.color = Theme.accentError
                    }
                }
            }

            Label {
                id: statusLabel
                text: ""
                color: Theme.accentSuccess
                font.pixelSize: 12
            }

            Item { Layout.fillWidth: true }
        }

        // 快照列表
        ListView {
            id: snapshotList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: snapshotModel
            spacing: 4

            delegate: Rectangle {
                width: snapshotList.width
                height: 64
                radius: 6
                color: mouseArea.containsMouse ? Theme.bgInput : "#252536"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 16

                    // 快照ID (缩写)
                    Label {
                        text: model.snapshotId.substring(0, 8) + "..."
                        color: Theme.accentPrimary
                        font.pixelSize: 13
                        font.family: "monospace"
                        Layout.preferredWidth: 100
                    }

                    // 样本数
                    Label {
                        text: model.sampleCount + " 样本"
                        color: Theme.textPrimary
                        font.pixelSize: 13
                    }

                    // 划分信息
                    Label {
                        text: "训练: " + model.trainCount + " / 验证: " + model.valCount
                        color: Theme.textSecondary
                        font.pixelSize: 12
                    }

                    // 类别版本
                    Label {
                        text: "类别: " + (model.taxonomyVersion || "未知")
                        color: Theme.textSecondary
                        font.pixelSize: 12
                    }

                    // 修订边界
                    Label {
                        text: "修订: " + (model.revisionBoundary || "无")
                        color: Theme.textSecondary
                        font.pixelSize: 12
                    }

                    // 创建时间
                    Label {
                        text: model.createdAt || ""
                        color: Theme.textMuted
                        font.pixelSize: 11
                    }

                    Item { Layout.fillWidth: true }

                    // 删除按钮
                    Button {
                        text: "删除"
                        flat: true
                        palette.buttonText: Theme.accentError
                        onClicked: {
                            if (snapshotService.deleteSnapshot(model.snapshotId)) {
                                snapshotModel.refresh()
                            }
                        }
                    }
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        detailPanel.snapshotId = model.snapshotId
                        detailPanel.visible = true
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: snapshotList.count === 0
                text: datasetCombo.currentValue ? "暂无快照，点击\"创建快照\"添加" : "请先选择数据集"
                color: Theme.textMuted
                font.pixelSize: 14
            }
        }

        // 快照详情面板
        Rectangle {
            id: detailPanel
            property string snapshotId: ""
            visible: false
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            color: Theme.bgCard
            radius: 6

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6

                RowLayout {
                    Label { text: "快照详情"; color: Theme.accentPrimary; font.pixelSize: 14; font.bold: true }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "关闭"
                        flat: true
                        palette.buttonText: Theme.textMuted
                        onClicked: detailPanel.visible = false
                    }
                }

                Label {
                    id: detailLabel
                    color: Theme.textPrimary
                    font.pixelSize: 12
                    font.family: "monospace"
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                }

                Label {
                    id: splitDetailLabel
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                }
            }

            onSnapshotIdChanged: {
                if (snapshotId === "") return
                var details = snapshotService.getSnapshot(snapshotId)
                detailLabel.text = "ID: " + details.id + " | 样本: " + details.sampleCount +
                                   " | 训练: " + details.trainCount + " | 验证: " + details.valCount +
                                   " | 类别版本: " + (details.taxonomyVersion || "无")
                var split = snapshotService.getSplitManifest(snapshotId)
                var trainIds = split.train || []
                var valIds = split.val || []
                splitDetailLabel.text = "训练集: " + trainIds.length + " 样本 | 验证集: " + valIds.length + " 样本"
            }
        }
    }
}
