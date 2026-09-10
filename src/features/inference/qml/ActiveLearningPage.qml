// ActiveLearningPage.qml - 主动学习主页面
// 提供低置信样本收集、队列管理、优先级排序功能
// 对接 ActiveLearningService（C++）和 active_learning handler（Python）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LabelTorch.Theme

/// 主动学习主页面
Rectangle {
    id: root

    // 外部属性
    property string currentProjectId: ""
    property string currentQueueType: "low-confidence"
    property var queueStats: null
    property var sampleList: []

    // 内部状态
    color: Theme.bgMain

    ListModel {
        id: queueTypeModel
        ListElement { text: "低置信队列"; value: "low-confidence" }
        ListElement { text: "误检队列"; value: "false-positive" }
        ListElement { text: "漏检队列"; value: "false-negative" }
        ListElement { text: "难例队列"; value: "hard-case" }
    }

    // 样本列表模型
    ListModel {
        id: sampleListModel
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacingNormal

        // 顶部标题栏
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: Theme.bgSide
            radius: Theme.radiusNormal
            border.color: Theme.borderColor
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLarge
                anchors.rightMargin: Theme.spacingLarge
                spacing: Theme.spacingNormal

                Label {
                    text: "主动学习中心"
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeSubheading
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                // 队列类型选择器
                Label {
                    text: "队列类型："
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeNormal
                }

                ComboBox {
                    id: queueSelector
                    Layout.preferredWidth: 140
                    model: queueTypeModel
                    textRole: "text"
                    valueRole: "value"
                    onActivated: {
                        root.currentQueueType = queueTypeModel.get(index).value
                        refreshSamples()
                    }

                    contentItem: Label {
                        text: queueSelector.displayText
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: Theme.spacingSmall
                    }

                    background: Rectangle {
                        color: Theme.bgCard
                        radius: Theme.radiusSmall
                        border.color: queueSelector.activeFocus ? Theme.primary : Theme.borderColor
                        border.width: 1
                        implicitHeight: 32
                    }

                    popup: Popup {
                        y: queueSelector.height
                        width: queueSelector.width
                        implicitHeight: Math.min(contentItem.implicitHeight, 200)
                        padding: 1

                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: queueSelector.popup.visible ? queueSelector.delegateModel : null
                            currentIndex: queueSelector.highlightedIndex
                        }

                        background: Rectangle {
                            color: Theme.bgMain
                            border.color: Theme.borderColor
                            radius: Theme.radiusSmall
                        }
                    }

                    delegate: ItemDelegate {
                        width: queueSelector.width
                        contentItem: Label {
                            text: model.text
                            color: highlighted ? Theme.primary : Theme.textMain
                            font.pixelSize: Theme.fontSizeNormal
                            verticalAlignment: Text.AlignVCenter
                        }
                        highlighted: queueSelector.highlightedIndex === index
                        background: Rectangle {
                            color: highlighted ? Theme.bgHover : Theme.bgMain
                        }
                    }
                }

                Button {
                    text: "收集样本"
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(Theme.primary, 1.2) : Theme.primary
                        radius: Theme.radiusSmall
                    }
                    contentItem: Label {
                        text: parent.text
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: collectSamplesDialog.open()
                }

                Button {
                    text: "优先级排序"
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(Theme.primaryGlow, 1.2) : Theme.primaryGlow
                        radius: Theme.radiusSmall
                    }
                    contentItem: Label {
                        text: parent.text
                        color: Theme.bgMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (typeof activeLearningService !== "undefined") {
                            activeLearningService.prioritizeQueue(
                                root.sampleList,
                                root.currentQueueType,
                                [],
                                "default"
                            )
                        }
                    }
                }
            }
        }

        // 主内容区：左右分栏
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingNormal

            // 左侧：队列样本列表
            Rectangle {
                Layout.preferredWidth: 400
                Layout.fillHeight: true
                color: Theme.bgSide
                radius: Theme.radiusNormal
                border.color: Theme.borderColor
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingNormal
                    spacing: Theme.spacingSmall

                    // 列表标题
                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: "队列样本"
                            color: Theme.primary
                            font.pixelSize: Theme.fontSizeNormal
                            font.bold: true
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            text: sampleListModel.count + " 条"
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeCaption
                        }
                    }

                    // 样本列表
                    ListView {
                        id: sampleListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: Theme.spacingSmall
                        model: sampleListModel

                        // 空态提示
                        Label {
                            anchors.centerIn: parent
                            visible: sampleListView.count === 0
                            text: qsTr("暂无样本，点击“收集样本”开始")
                            color: Theme.textDisabled
                            font.pixelSize: Theme.fontSizeNormal
                        }

                        delegate: Rectangle {
                            width: sampleListView.width
                            height: 56
                            radius: Theme.radiusSmall
                            color: sampleMouseArea.containsMouse ? Theme.bgHover : Theme.bgCard
                            border.color: model.priority === "high" ? Theme.danger : Theme.borderColor
                            border.width: model.priority === "high" ? 1 : 0

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.spacingNormal
                                anchors.rightMargin: Theme.spacingNormal
                                spacing: Theme.spacingSmall

                                // 优先级指示器
                                Rectangle {
                                    Layout.preferredWidth: 8
                                    Layout.preferredHeight: 8
                                    radius: 4
                                    color: {
                                        if (model.priority === "high") return Theme.danger
                                        if (model.priority === "medium") return Theme.warning
                                        return Theme.textDisabled
                                    }
                                }

                                // 样本信息
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    Label {
                                        text: model.fileName || ("样本 #" + model.index)
                                        color: Theme.textMain
                                        font.pixelSize: Theme.fontSizeSmall
                                        elide: Text.ElideMiddle
                                    }

                                    Label {
                                        text: model.className ? model.className : "未知类别"
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSizeCaption
                                    }
                                }

                                // 置信度
                                Label {
                                    text: (parseFloat(model.confidence) || 0).toFixed(3)
                                    color: (parseFloat(model.confidence) || 0) < 0.3 ? Theme.danger : Theme.warning
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.family: Theme.fontFamilyMono
                                }

                                // 操作按钮
                                Button {
                                    text: "审核"
                                    Layout.preferredHeight: 24
                                    Layout.preferredWidth: 50
                                    background: Rectangle {
                                        color: parent.pressed ? Qt.darker(Theme.primaryGlow, 1.2) : Theme.primaryGlow
                                        radius: Theme.radiusSmall
                                    }
                                    contentItem: Label {
                                        text: parent.text
                                        color: Theme.bgMain
                                        font.pixelSize: Theme.fontSizeCaption
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    onClicked: reviewSample(model)
                                }
                            }

                            MouseArea {
                                id: sampleMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                        }
                    }
                }
            }

            // 右侧：统计和操作面板
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacingNormal

                // 队列统计面板
                QueueStatsPanel {
                    Layout.fillWidth: true
                    stats: root.queueStats
                }

                // 操作面板
                Rectangle {
                    Layout.fillWidth: true
                    color: Theme.bgSide
                    radius: Theme.radiusNormal
                    border.color: Theme.borderColor
                    border.width: 1
                    implicitHeight: operationLayout.implicitHeight + Theme.spacingLarge * 2

                    ColumnLayout {
                        id: operationLayout
                        anchors.fill: parent
                        anchors.margins: Theme.spacingNormal
                        spacing: Theme.spacingSmall

                        Label {
                            text: "操作"
                            color: Theme.primary
                            font.pixelSize: Theme.fontSizeNormal
                            font.bold: true
                        }

                        Button {
                            text: "批量确认"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            background: Rectangle {
                                color: parent.pressed ? Qt.darker(Theme.success, 1.2) : Theme.success
                                radius: Theme.radiusSmall
                            }
                            contentItem: Label {
                                text: parent.text
                                color: Theme.bgMain
                                font.pixelSize: Theme.fontSizeNormal
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: batchConfirmSelected()
                        }

                        Button {
                            text: "批量拒绝"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            background: Rectangle {
                                color: parent.pressed ? Qt.darker(Theme.danger, 1.2) : Theme.danger
                                radius: Theme.radiusSmall
                            }
                            contentItem: Label {
                                text: parent.text
                                color: Theme.bgMain
                                font.pixelSize: Theme.fontSizeNormal
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: batchRejectSelected()
                        }

                        Button {
                            text: "清空队列"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            background: Rectangle {
                                color: parent.pressed ? Qt.darker(Theme.bgCard, 1.2) : Theme.bgCard
                                radius: Theme.radiusSmall
                                border.color: Theme.danger
                                border.width: 1
                            }
                            contentItem: Label {
                                text: parent.text
                                color: Theme.danger
                                font.pixelSize: Theme.fontSizeNormal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: clearConfirmDialog.open()
                        }

                        Item { Layout.fillHeight: true }

                        Button {
                            text: "生成训练快照"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 36
                            enabled: sampleListModel.count > 0
                            background: Rectangle {
                                color: parent.enabled ? (parent.pressed ? Qt.darker(Theme.primary, 1.2) : Theme.primary) : Theme.bgCard
                                radius: Theme.radiusSmall
                            }
                            contentItem: Label {
                                text: parent.text
                                color: parent.enabled ? Theme.textMain : Theme.textDisabled
                                font.pixelSize: Theme.fontSizeNormal
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: createTrainingSnapshot()
                        }
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
        anchors.centerIn: parent
        width: 420
        height: 280

        palette.window: Theme.bgMain
        palette.windowText: Theme.textMain
        palette.base: Theme.bgInput
        palette.text: Theme.textMain
        palette.button: Theme.bgCard
        palette.buttonText: Theme.textMain

        background: Rectangle {
            color: Theme.bgMain
            radius: Theme.radiusLarge
            border.color: Theme.borderColor
            border.width: 1
        }

        header: Rectangle {
            color: Theme.bgInput
            height: 44
            radius: Theme.radiusLarge

            Label {
                anchors.centerIn: parent
                text: "收集低置信样本"
                color: Theme.primary
                font.pixelSize: Theme.fontSizeSubheading
                font.bold: true
            }
        }

        contentItem: Rectangle {
            color: Theme.bgMain

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacingNormal

                Label {
                    text: "模型权重路径："
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeNormal
                }

                TextField {
                    id: weightPathField
                    Layout.fillWidth: true
                    placeholderText: "模型权重路径"
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeNormal
                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: weightPathField.activeFocus ? Theme.primary : Theme.borderColor
                        border.width: 1
                        implicitHeight: 32
                    }
                }

                Label {
                    text: "图片路径或目录："
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeNormal
                }

                TextField {
                    id: sourcePathField
                    Layout.fillWidth: true
                    placeholderText: "图片路径或目录"
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeNormal
                    background: Rectangle {
                        color: Theme.bgInput
                        radius: Theme.radiusSmall
                        border.color: sourcePathField.activeFocus ? Theme.primary : Theme.borderColor
                        border.width: 1
                        implicitHeight: 32
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingNormal

                    Label {
                        text: "置信度阈值:"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeNormal
                    }

                    Slider {
                        id: confSlider
                        Layout.fillWidth: true
                        from: 0.1
                        to: 0.8
                        value: 0.3
                        stepSize: 0.05

                        background: Rectangle {
                            x: confSlider.leftPadding
                            y: confSlider.topPadding + confSlider.availableHeight / 2 - height / 2
                            width: confSlider.availableWidth
                            height: 4
                            radius: 2
                            color: Theme.bgCard

                            Rectangle {
                                width: confSlider.visualPosition * parent.width
                                height: parent.height
                                color: Theme.primary
                                radius: 2
                            }
                        }

                        handle: Rectangle {
                            x: confSlider.leftPadding + confSlider.visualPosition * (confSlider.availableWidth - width)
                            y: confSlider.topPadding + confSlider.availableHeight / 2 - height / 2
                            width: 16
                            height: 16
                            radius: 8
                            color: confSlider.pressed ? Theme.primaryGlow : Theme.primary
                        }
                    }

                    Label {
                        text: confSlider.value.toFixed(2)
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeSmall
                        font.family: Theme.fontFamilyMono
                        Layout.preferredWidth: 40
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingNormal

                    Label {
                        text: "推理设备:"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeNormal
                    }

                    ComboBox {
                        id: deviceSelector
                        Layout.fillWidth: true
                        model: ["auto", "cpu", "0"]

                        contentItem: Label {
                            text: deviceSelector.currentText
                            color: Theme.textMain
                            font.pixelSize: Theme.fontSizeNormal
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: Theme.spacingSmall
                        }

                        background: Rectangle {
                            color: Theme.bgCard
                            radius: Theme.radiusSmall
                            border.color: deviceSelector.activeFocus ? Theme.primary : Theme.borderColor
                            border.width: 1
                            implicitHeight: 32
                        }
                    }
                }
            }
        }

        footer: Rectangle {
            color: Theme.bgInput
            height: 52
            radius: Theme.radiusLarge

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLarge
                anchors.rightMargin: Theme.spacingLarge
                spacing: Theme.spacingNormal

                Button {
                    text: "取消"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(Theme.bgCard, 1.2) : Theme.bgCard
                        radius: Theme.radiusSmall
                        border.color: Theme.borderColor
                        border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: collectSamplesDialog.reject()
                }

                Button {
                    text: "开始收集"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(Theme.primary, 1.2) : Theme.primary
                        radius: Theme.radiusSmall
                    }
                    contentItem: Label {
                        text: parent.text
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (typeof activeLearningService !== "undefined") {
                            activeLearningService.collectLowConfSamples(
                                weightPathField.text,
                                sourcePathField.text,
                                confSlider.value,
                                0.45,
                                640,
                                deviceSelector.currentText
                            )
                        }
                        collectSamplesDialog.accept()
                    }
                }
            }
        }
    }

    // 清空确认对话框
    Dialog {
        id: clearConfirmDialog
        title: "确认清空队列"
        modal: true
        anchors.centerIn: parent
        width: 360
        height: 160

        palette.window: Theme.bgMain
        palette.windowText: Theme.textMain

        background: Rectangle {
            color: Theme.bgMain
            radius: Theme.radiusLarge
            border.color: Theme.borderColor
            border.width: 1
        }

        header: Rectangle {
            color: Theme.bgInput
            height: 44
            radius: Theme.radiusLarge

            Label {
                anchors.centerIn: parent
                text: "确认清空队列"
                color: Theme.danger
                font.pixelSize: Theme.fontSizeSubheading
                font.bold: true
            }
        }

        contentItem: Rectangle {
            color: Theme.bgMain

            Label {
                anchors.centerIn: parent
                text: "确定要清空当前队列吗？此操作不可撤销。"
                color: Theme.textMain
                font.pixelSize: Theme.fontSizeNormal
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        footer: Rectangle {
            color: Theme.bgInput
            height: 52
            radius: Theme.radiusLarge

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLarge
                anchors.rightMargin: Theme.spacingLarge
                spacing: Theme.spacingNormal

                Button {
                    text: "取消"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(Theme.bgCard, 1.2) : Theme.bgCard
                        radius: Theme.radiusSmall
                        border.color: Theme.borderColor
                        border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: clearConfirmDialog.reject()
                }

                Button {
                    text: "确认清空"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(Theme.danger, 1.2) : Theme.danger
                        radius: Theme.radiusSmall
                    }
                    contentItem: Label {
                        text: parent.text
                        color: Theme.bgMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (typeof activeLearningService !== "undefined") {
                            activeLearningService.clearQueue(root.currentQueueType)
                        }
                        refreshSamples()
                        clearConfirmDialog.accept()
                    }
                }
            }
        }
    }

    // 样本审核详情对话框
    Dialog {
        id: reviewDialog
        title: "样本审核"
        modal: true
        anchors.centerIn: parent
        width: 420
        height: 320

        property var sampleData: null

        palette.window: Theme.bgMain
        palette.windowText: Theme.textMain
        palette.base: Theme.bgInput
        palette.text: Theme.textMain
        palette.button: Theme.bgCard
        palette.buttonText: Theme.textMain

        background: Rectangle {
            color: Theme.bgMain
            radius: Theme.radiusLarge
            border.color: Theme.borderColor
            border.width: 1
        }

        header: Rectangle {
            color: Theme.bgInput
            height: 44
            radius: Theme.radiusLarge

            Label {
                anchors.centerIn: parent
                text: "样本审核"
                color: Theme.primary
                font.pixelSize: Theme.fontSizeSubheading
                font.bold: true
            }
        }

        contentItem: Rectangle {
            color: Theme.bgMain

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacingNormal

                // 样本信息展示
                Repeater {
                    model: [
                        { label: "文件名", value: reviewDialog.sampleData ? reviewDialog.sampleData.fileName : "" },
                        { label: "类别", value: reviewDialog.sampleData ? reviewDialog.sampleData.className : "" },
                        { label: "置信度", value: reviewDialog.sampleData ? (parseFloat(reviewDialog.sampleData.confidence) || 0).toFixed(4) : "" },
                        { label: "优先级", value: reviewDialog.sampleData ? reviewDialog.sampleData.priority : "" }
                    ]

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingNormal

                        Label {
                            Layout.preferredWidth: 80
                            text: modelData.label + "："
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeNormal
                        }

                        Label {
                            Layout.fillWidth: true
                            text: modelData.value
                            color: Theme.textMain
                            font.pixelSize: Theme.fontSizeNormal
                            font.family: Theme.fontFamilyMono
                            elide: Text.ElideRight
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Label {
                    Layout.fillWidth: true
                    text: "选择操作：确认将样本标记为正确并从队列移除；拒绝将样本标记为无效并从队列移除。"
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeCaption
                    wrapMode: Text.WordWrap
                }
            }
        }

        footer: Rectangle {
            color: Theme.bgInput
            height: 52
            radius: Theme.radiusLarge

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLarge
                anchors.rightMargin: Theme.spacingLarge
                spacing: Theme.spacingNormal

                Button {
                    text: "关闭"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(Theme.bgCard, 1.2) : Theme.bgCard
                        radius: Theme.radiusSmall
                        border.color: Theme.borderColor
                        border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: reviewDialog.reject()
                }

                Button {
                    text: "拒绝"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(Theme.danger, 1.2) : Theme.danger
                        radius: Theme.radiusSmall
                    }
                    contentItem: Label {
                        text: parent.text
                        color: Theme.bgMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (reviewDialog.sampleData && typeof activeLearningService !== "undefined") {
                            activeLearningService.removeSampleFromQueue(
                                root.currentQueueType,
                                reviewDialog.sampleData.fileName || ""
                            )
                        }
                        refreshSamples()
                        reviewDialog.accept()
                    }
                }

                Button {
                    text: "确认"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(Theme.success, 1.2) : Theme.success
                        radius: Theme.radiusSmall
                    }
                    contentItem: Label {
                        text: parent.text
                        color: Theme.bgMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (reviewDialog.sampleData && typeof activeLearningService !== "undefined") {
                            activeLearningService.removeSampleFromQueue(
                                root.currentQueueType,
                                reviewDialog.sampleData.fileName || ""
                            )
                        }
                        refreshSamples()
                        reviewDialog.accept()
                    }
                }
            }
        }
    }

    // 批量操作确认对话框
    Dialog {
        id: batchOperationDialog
        title: "批量操作确认"
        modal: true
        anchors.centerIn: parent
        width: 400
        height: 200

        property string operationType: "confirm"
        property string message: ""

        palette.window: Theme.bgMain
        palette.windowText: Theme.textMain
        palette.base: Theme.bgInput
        palette.text: Theme.textMain
        palette.button: Theme.bgCard
        palette.buttonText: Theme.textMain

        background: Rectangle {
            color: Theme.bgMain
            radius: Theme.radiusLarge
            border.color: Theme.borderColor
            border.width: 1
        }

        header: Rectangle {
            color: Theme.bgInput
            height: 44
            radius: Theme.radiusLarge

            Label {
                anchors.centerIn: parent
                text: batchOperationDialog.operationType === "confirm" ? "批量确认" : "批量拒绝"
                color: batchOperationDialog.operationType === "confirm" ? Theme.success : Theme.danger
                font.pixelSize: Theme.fontSizeSubheading
                font.bold: true
            }
        }

        contentItem: Rectangle {
            color: Theme.bgMain

            Label {
                anchors.centerIn: parent
                text: batchOperationDialog.message
                color: Theme.textMain
                font.pixelSize: Theme.fontSizeNormal
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        footer: Rectangle {
            color: Theme.bgInput
            height: 52
            radius: Theme.radiusLarge

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLarge
                anchors.rightMargin: Theme.spacingLarge
                spacing: Theme.spacingNormal

                Button {
                    text: "取消"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(Theme.bgCard, 1.2) : Theme.bgCard
                        radius: Theme.radiusSmall
                        border.color: Theme.borderColor
                        border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: batchOperationDialog.reject()
                }

                Button {
                    text: "确认"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(Theme.primary, 1.2) : Theme.primary
                        radius: Theme.radiusSmall
                    }
                    contentItem: Label {
                        text: parent.text
                        color: Theme.bgMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (typeof activeLearningService !== "undefined") {
                            activeLearningService.clearQueue(root.currentQueueType)
                        }
                        refreshSamples()
                        batchOperationDialog.accept()
                    }
                }
            }
        }
    }

    // 创建训练快照对话框
    Dialog {
        id: createSnapshotDialog
        title: "生成训练快照"
        modal: true
        anchors.centerIn: parent
        width: 420
        height: 260

        palette.window: Theme.bgMain
        palette.windowText: Theme.textMain
        palette.base: Theme.bgInput
        palette.text: Theme.textMain
        palette.button: Theme.bgCard
        palette.buttonText: Theme.textMain

        background: Rectangle {
            color: Theme.bgMain
            radius: Theme.radiusLarge
            border.color: Theme.borderColor
            border.width: 1
        }

        header: Rectangle {
            color: Theme.bgInput
            height: 44
            radius: Theme.radiusLarge

            Label {
                anchors.centerIn: parent
                text: "生成训练快照"
                color: Theme.primary
                font.pixelSize: Theme.fontSizeSubheading
                font.bold: true
            }
        }

        contentItem: Rectangle {
            color: Theme.bgMain

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                spacing: Theme.spacingNormal

                Label {
                    text: "选择数据集："
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeNormal
                }

                ComboBox {
                    id: snapshotDatasetCombo
                    Layout.fillWidth: true
                    model: typeof datasetModel !== "undefined" ? datasetModel : null
                    textRole: "name"
                    valueRole: "id"

                    contentItem: Label {
                        text: snapshotDatasetCombo.displayText
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: Theme.spacingSmall
                    }

                    background: Rectangle {
                        color: Theme.bgCard
                        radius: Theme.radiusSmall
                        border.color: snapshotDatasetCombo.activeFocus ? Theme.primary : Theme.borderColor
                        border.width: 1
                        implicitHeight: 32
                    }
                }

                Label {
                    text: "训练集比例："
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSizeNormal
                }

                SpinBox {
                    id: trainRatioSpin
                    Layout.fillWidth: true
                    from: 10
                    to: 90
                    value: 80
                    stepSize: 5
                    suffix: "%"

                    palette.base: Theme.bgInput
                    palette.text: Theme.textMain
                    palette.button: Theme.bgCard
                    palette.buttonText: Theme.textMain
                    palette.highlight: Theme.primary
                }
            }
        }

        footer: Rectangle {
            color: Theme.bgInput
            height: 52
            radius: Theme.radiusLarge

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLarge
                anchors.rightMargin: Theme.spacingLarge
                spacing: Theme.spacingNormal

                Button {
                    text: "取消"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    background: Rectangle {
                        color: parent.pressed ? Qt.darker(Theme.bgCard, 1.2) : Theme.bgCard
                        radius: Theme.radiusSmall
                        border.color: Theme.borderColor
                        border.width: 1
                    }
                    contentItem: Label {
                        text: parent.text
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: createSnapshotDialog.reject()
                }

                Button {
                    text: "创建快照"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    enabled: snapshotDatasetCombo.currentValue !== undefined && snapshotDatasetCombo.currentValue !== ""
                    background: Rectangle {
                        color: parent.enabled ? (parent.pressed ? Qt.darker(Theme.primary, 1.2) : Theme.primary) : Theme.bgCard
                        radius: Theme.radiusSmall
                    }
                    contentItem: Label {
                        text: parent.text
                        color: parent.enabled ? Theme.bgMain : Theme.textDisabled
                        font.pixelSize: Theme.fontSizeNormal
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (typeof snapshotService !== "undefined" && snapshotDatasetCombo.currentValue) {
                            var ratio = trainRatioSpin.value / 100.0
                            var snapshotId = snapshotService.createSnapshot(
                                snapshotDatasetCombo.currentValue,
                                ratio,
                                "random"
                            )
                            if (snapshotId && snapshotId !== "") {
                                createSnapshotDialog.accept()
                            }
                        }
                    }
                }
            }
        }
    }

    // 刷新样本列表
    function refreshSamples() {
        sampleListModel.clear()
        for (var i = 0; i < root.sampleList.length; i++) {
            var s = root.sampleList[i]
            sampleListModel.append({
                "index": i,
                "fileName": s.fileName || "",
                "className": s.className || "",
                "confidence": s.confidence || 0,
                "priority": s.priority || "low"
            })
        }
    }

    // 审核单个样本 - 打开样本详情对话框
    function reviewSample(sampleData) {
        reviewDialog.sampleData = sampleData
        reviewDialog.open()
    }

    // 批量确认选中样本 - 将当前队列所有样本标记为已确认并清空队列
    function batchConfirmSelected() {
        if (sampleListModel.count === 0) {
            return
        }
        batchOperationDialog.operationType = "confirm"
        batchOperationDialog.message = "确认将当前队列中的 " + sampleListModel.count + " 个样本标记为已审核（确认）？\n确认后样本将从队列移除。"
        batchOperationDialog.open()
    }

    // 批量拒绝选中样本 - 将当前队列所有样本标记为已拒绝并清空队列
    function batchRejectSelected() {
        if (sampleListModel.count === 0) {
            return
        }
        batchOperationDialog.operationType = "reject"
        batchOperationDialog.message = "确认将当前队列中的 " + sampleListModel.count + " 个样本标记为已拒绝？\n拒绝后样本将从队列移除。"
        batchOperationDialog.open()
    }

    // 从主动学习队列生成训练快照 - 打开创建快照对话框
    function createTrainingSnapshot() {
        if (root.currentProjectId === "") {
            return
        }
        // 刷新数据集模型以获取当前项目的数据集列表
        if (typeof datasetModel !== "undefined") {
            datasetModel.setProjectId(root.currentProjectId)
            datasetModel.refresh()
        }
        createSnapshotDialog.open()
    }

    // 信号连接：监听 ActiveLearningService 事件
    Connections {
        target: typeof activeLearningService !== "undefined" ? activeLearningService : null

        function onSamplesCollected(samples, totalSamples) {
            root.sampleList = samples || []
            refreshSamples()
        }

        function onQueuePrioritized(sortedSamples, total) {
            root.sampleList = sortedSamples || []
            refreshSamples()
        }

        function onQueueStatsReady(stats) {
            root.queueStats = stats || ({})
        }

        function onError(message) {
            console.error("[ActiveLearning] Error:", message)
        }
    }
}
