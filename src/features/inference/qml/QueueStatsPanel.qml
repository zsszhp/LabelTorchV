import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/// <summary>
/// 队列统计面板
/// 显示主动学习队列的统计信息
/// </summary>
GroupBox {
    id: root

    title: "队列统计"
    Layout.fillWidth: true

    property var stats: ({})

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacingSm

        RowLayout {
            spacing: Theme.spacingMd

            StatCard {
                Layout.fillWidth: true
                title: "总样本数"
                value: stats.total_samples || 0
                icon: "📊"
            }

            StatCard {
                Layout.fillWidth: true
                title: "总框数"
                value: stats.total_boxes || 0
                icon: "📦"
            }
        }

        RowLayout {
            spacing: Theme.spacingMd

            StatCard {
                Layout.fillWidth: true
                title: "平均置信度"
                value: stats.avg_confidence ? (stats.avg_confidence * 100).toFixed(1) + "%" : "N/A"
                icon: "🎯"
            }

            StatCard {
                Layout.fillWidth: true
                title: "每样本框数"
                value: stats.avg_boxes_per_sample || 0
                icon: "📐"
            }
        }

        // 类别分布
        Label {
            text: "类别分布"
            font: Theme.fontSubtitle
            Layout.topMargin: Theme.spacingSm
        }

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 200)
            clip: true

            model: stats.class_distribution ? Object.keys(stats.class_distribution) : []

            delegate: RowLayout {
                width: parent.width
                spacing: Theme.spacingSm

                Label {
                    text: "类别 " + modelData
                    Layout.fillWidth: true
                }

                Label {
                    text: stats.class_distribution[modelData] + " 个"
                    font: Theme.fontBody
                }
            }
        }
    }
}

/// <summary>
/// 统计卡片组件
/// </summary>
Component {
    id: statCardComponent

    Rectangle {
        id: card
        implicitHeight: 80
        radius: Theme.radiusMd
        color: Theme.surfaceVariant

        property string title: ""
        property var value: 0
        property string icon: ""

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 4

            Label {
                text: icon + " " + title
                font: Theme.fontCaption
                color: Theme.onSurfaceVariant
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: value
                font: Theme.fontH4
                color: Theme.primary
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
}
