// QueueStatsPanel.qml - 队列统计面板
// 显示主动学习队列的统计信息（总样本数/总框数/平均置信度/类别分布）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme

/// 队列统计面板：显示主动学习队列的统计信息
Rectangle {
    id: root

    // 外部属性：统计数据
    property var stats: null

    // 外观
    color: Theme.bgSide
    radius: Theme.radiusNormal
    border.color: Theme.borderColor
    border.width: 1
    implicitHeight: contentLayout.implicitHeight + Theme.spacingLarge * 2

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: Theme.spacingNormal
        spacing: Theme.spacingSmall

        // 区域标题
        Label {
            text: "队列统计"
            color: Theme.primary
            font.pixelSize: Theme.fontSizeSubheading
            font.bold: true
        }

        // 第一行：总样本数 + 总框数
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingNormal

            // 总样本数卡片
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                radius: Theme.radiusSmall
                color: Theme.bgCard
                border.color: Theme.borderColor
                border.width: 1

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 2

                    Label {
                        text: "总样本数"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeCaption
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Label {
                        text: stats.total_samples || 0
                        color: Theme.primary
                        font.pixelSize: Theme.fontSizeLarge
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            // 总框数卡片
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                radius: Theme.radiusSmall
                color: Theme.bgCard
                border.color: Theme.borderColor
                border.width: 1

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 2

                    Label {
                        text: "总框数"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeCaption
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Label {
                        text: stats.total_boxes || 0
                        color: Theme.primary
                        font.pixelSize: Theme.fontSizeLarge
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
        }

        // 第二行：平均置信度 + 每样本框数
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingNormal

            // 平均置信度卡片
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                radius: Theme.radiusSmall
                color: Theme.bgCard
                border.color: Theme.borderColor
                border.width: 1

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 2

                    Label {
                        text: "平均置信度"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeCaption
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Label {
                        text: stats.avg_confidence !== undefined && stats.avg_confidence !== null ?
                            ((parseFloat(stats.avg_confidence) || 0) * 100).toFixed(1) + "%" : "N/A"
                        color: Theme.primary
                        font.pixelSize: Theme.fontSizeLarge
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            // 每样本框数卡片
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                radius: Theme.radiusSmall
                color: Theme.bgCard
                border.color: Theme.borderColor
                border.width: 1

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 2

                    Label {
                        text: "每样本框数"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeCaption
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Label {
                        text: stats.avg_boxes_per_sample || 0
                        color: Theme.primary
                        font.pixelSize: Theme.fontSizeLarge
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
        }

        // 类别分布标题
        Label {
            text: "类别分布"
            color: Theme.textMain
            font.pixelSize: Theme.fontSizeNormal
            font.bold: true
            Layout.topMargin: Theme.spacingSmall
        }

        // 类别分布列表
        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 200)
            clip: true
            spacing: 2

            model: stats.class_distribution ? Object.keys(stats.class_distribution) : []

            delegate: Rectangle {
                width: parent ? parent.width : 0
                height: 28
                radius: Theme.radiusSmall
                color: Theme.bgCard

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingNormal
                    anchors.rightMargin: Theme.spacingNormal
                    spacing: Theme.spacingSmall

                    Label {
                        text: "类别 " + modelData
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeSmall
                        Layout.fillWidth: true
                    }

                    Label {
                        text: stats.class_distribution[modelData] + " 个"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }
        }
    }
}
