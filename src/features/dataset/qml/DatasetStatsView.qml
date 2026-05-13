// DatasetStatsView.qml - 数据统计视图
import QtQuick
import QtQuick.Controls
import LabelTorch.Shell
import QtQuick.Layouts

Item {
    id: root

    property var currentStats: null
    property var currentAnomalies: null
    property var classDistList: []

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // 标题栏
        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "数据统计"
                font.pixelSize: 24
                font.bold: true
                color: Theme.textPrimary
            }

            Item { Layout.fillWidth: true }

            Label {
                visible: !appController.projectOpen
                text: "请先打开项目"
                color: Theme.accentError
                font.pixelSize: 14
            }

            ComboBox {
                id: datasetCombo
                visible: appController.projectOpen
                Layout.preferredWidth: 220
                model: datasetModel
                textRole: "name"
                valueRole: "datasetId"

                background: Rectangle { color: Theme.bgInput; radius: 4; border.color: Theme.borderNormal }
                contentItem: Label {
                    text: datasetCombo.displayText
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }
            }

            Button {
                visible: appController.projectOpen
                text: "刷新统计"
                highlighted: true
                enabled: datasetCombo.currentValue
                onClicked: refreshStats()
            }
        }

        // 主内容区
        RowLayout {
            visible: appController.projectOpen
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            // 左面板 - 统计概览
            Rectangle {
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                color: Theme.bgCard
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Label {
                        text: "样本统计"
                        font.pixelSize: 16
                        font.bold: true
                        color: Theme.textPrimary
                    }

                    // 统计卡片
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 8
                        columnSpacing: 8

                        StatCard {
                            cardTitle: "总数"
                            cardValue: currentStats ? currentStats.totalSamples : "-"
                            valueColor: Theme.accentPrimary
                        }
                        StatCard {
                            cardTitle: "有效"
                            cardValue: currentStats ? currentStats.validSamples : "-"
                            valueColor: Theme.accentSuccess
                        }
                        StatCard {
                            cardTitle: "无效"
                            cardValue: currentStats ? currentStats.invalidSamples : "-"
                            valueColor: Theme.accentError
                        }
                        StatCard {
                            cardTitle: "未标注"
                            cardValue: currentStats ? currentStats.unlabeledSamples : "-"
                            valueColor: Theme.accentWarning
                        }
                    }

                    // 标注密度
                    Label {
                        text: "标注密度"
                        font.pixelSize: 14
                        font.bold: true
                        color: Theme.textSecondary
                        Layout.topMargin: 8
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 4
                        rowSpacing: 6
                        columnSpacing: 6

                        MiniStat { statLabel: "最小"; statValue: currentStats ? currentStats.annotationDensity.min : "-" }
                        MiniStat { statLabel: "最大"; statValue: currentStats ? currentStats.annotationDensity.max : "-" }
                        MiniStat { statLabel: "平均"; statValue: currentStats ? currentStats.annotationDensity.avg : "-" }
                        MiniStat { statLabel: "中位"; statValue: currentStats ? currentStats.annotationDensity.median : "-" }
                    }

                    Item { Layout.fillHeight: true }

                    // 占位提示
                    Label {
                        visible: !currentStats
                        text: "选择数据集并点击\"刷新统计\"查看数据"
                        color: Theme.textMuted
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            // 右面板 - 类别分布
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.bgCard
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    Label {
                        text: "类别分布"
                        font.pixelSize: 16
                        font.bold: true
                        color: Theme.textPrimary
                    }

                    ListView {
                        id: classDistView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 4
                        model: classDistList

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 32
                            color: "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 4
                                anchors.rightMargin: 4
                                spacing: 8

                                Label {
                                    text: modelData.className
                                    color: Theme.textPrimary
                                    font.pixelSize: 12
                                    Layout.preferredWidth: 80
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 16
                                    radius: 3
                                    color: Theme.bgInput

                                    Rectangle {
                                        width: parent.width * (modelData.count / maxCount())
                                        height: parent.height
                                        radius: 3
                                        color: barColor(index)

                                        Behavior on width {
                                            NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                                        }
                                    }
                                }

                                Label {
                                    text: modelData.count
                                    color: Theme.textSecondary
                                    font.pixelSize: 12
                                    Layout.preferredWidth: 50
                                    horizontalAlignment: Text.AlignRight
                                }
                            }
                        }

                        Label {
                            visible: classDistList.length === 0
                            anchors.centerIn: parent
                            text: "暂无类别分布数据"
                            color: Theme.textMuted
                            font.pixelSize: 13
                        }
                    }
                }
            }
        }

        // 底部面板 - 异常检测
        Rectangle {
            visible: appController.projectOpen
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            color: Theme.bgCard
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: "异常检测"
                        font.pixelSize: 16
                        font.bold: true
                        color: Theme.textPrimary
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "检测异常"
                        enabled: datasetCombo.currentValue
                        onClicked: detectAnomalies()
                    }
                }

                // 异常结果列表
                ListView {
                    id: anomalyView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    model: anomalyModel

                    delegate: Rectangle {
                        property bool isExpanded: false

                        width: ListView.view.width
                        height: anomalyContent.height + 16 + (isExpanded && model.count > 0 ? detailText.height + 8 : 0)
                        color: Theme.bgPrimary
                        radius: 4

                        Column {
                            id: anomalyContent
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 4

                            Row {
                                width: parent.width
                                spacing: 8

                                Rectangle {
                                    width: 8
                                    height: 8
                                    radius: 4
                                    color: model.count > 0 ? Theme.accentError : Theme.accentSuccess
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Label {
                                    text: model.label
                                    color: Theme.textPrimary
                                    font.pixelSize: 13
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Item { width: parent.width - 200; height: 1 }

                                Label {
                                    text: model.count + " 项"
                                    color: model.count > 0 ? Theme.accentError : Theme.accentSuccess
                                    font.pixelSize: 13
                                    font.bold: true
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            Label {
                                id: detailText
                                visible: isExpanded && model.count > 0
                                text: model.count > 0 ? model.ids.join(", ") : ""
                                color: Theme.textMuted
                                font.pixelSize: 11
                                font.family: "Consolas"
                                wrapMode: Text.WordWrap
                                maximumLineCount: 3
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: parent.isExpanded = !parent.isExpanded
                        }
                    }
                }

                Label {
                    visible: !currentAnomalies
                    text: "点击\"检测异常\"扫描数据集问题"
                    color: Theme.textMuted
                    font.pixelSize: 12
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }

    // 组件定义 - 统计卡片
    component StatCard: Rectangle {
        property string cardTitle: ""
        property var cardValue: "-"
        property color valueColor: Theme.accentPrimary

        Layout.fillWidth: true
        height: 64
        color: Theme.bgPrimary
        radius: 6

        Column {
            anchors.centerIn: parent
            spacing: 2

            Label {
                text: cardTitle
                color: Theme.textMuted
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                text: cardValue
                color: valueColor
                font.pixelSize: 20
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    // 组件定义 - 迷你统计
    component MiniStat: Column {
        property string statLabel: ""
        property var statValue: "-"

        Layout.fillWidth: true

        Label {
            text: statLabel
            color: Theme.textMuted
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
        }

        Label {
            text: statValue
            color: Theme.textPrimary
            font.pixelSize: 14
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // 数据模型
    ListModel {
        id: anomalyModel

        ListElement {
            label: "空标签文件"
            count: 0
            ids: []
            expanded: false
        }
        ListElement {
            label: "类别ID越界"
            count: 0
            ids: []
            expanded: false
        }
        ListElement {
            label: "尺寸异常"
            count: 0
            ids: []
            expanded: false
        }
        ListElement {
            label: "重复图片"
            count: 0
            ids: []
            expanded: false
        }
    }

    // 辅助函数
    function maxCount() {
        var mx = 0
        for (var i = 0; i < classDistList.length; i++) {
            if (classDistList[i].count > mx) mx = classDistList[i].count
        }
        return mx || 1
    }

    function barColor(idx) {
        var colors = [Theme.accentPrimary, Theme.accentSuccess, Theme.accentWarning, Theme.accentError, "#cba6f7", "#94e2d5", "#fab387", "#74c7ec"]
        return colors[idx % colors.length]
    }

    function refreshStats() {
        var dsId = datasetCombo.currentValue
        if (!dsId) return

        currentStats = datasetService.getSampleStats(dsId)
        classDistList = datasetService.getClassDistribution(dsId)
    }

    function detectAnomalies() {
        var dsId = datasetCombo.currentValue
        if (!dsId) return

        currentAnomalies = datasetService.detectAnomalies(dsId)

        // 更新异常模型
        anomalyModel.get(0).count = currentAnomalies.emptyLabels.length
        anomalyModel.get(0).ids = currentAnomalies.emptyLabels
        anomalyModel.get(0).expanded = false

        anomalyModel.get(1).count = currentAnomalies.classErrors.length
        anomalyModel.get(1).ids = currentAnomalies.classErrors
        anomalyModel.get(1).expanded = false

        anomalyModel.get(2).count = currentAnomalies.sizeAnomalies.length
        anomalyModel.get(2).ids = currentAnomalies.sizeAnomalies
        anomalyModel.get(2).expanded = false

        anomalyModel.get(3).count = currentAnomalies.duplicateImages.length
        anomalyModel.get(3).ids = currentAnomalies.duplicateImages
        anomalyModel.get(3).expanded = false
    }
}
