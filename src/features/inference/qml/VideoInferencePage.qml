// VideoInferencePage.qml - 视频流推理页面（P2-6，supervision 集成）
// 使用 sv.VideoInfo + sv.VideoSink + sv.get_video_frames_generator 逐帧推理
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import LabelTorch.Theme
import LabelTorch.Components

Item {
    id: root

    property string currentProjectId: appController.currentProjectId
    property string selectedModelVersionId: ""
    property string selectedVideoPath: ""
    property bool processing: false
    property string outputPath: ""
    property string errorMessage: ""
    property int totalFrames: 0
    property int totalDetections: 0

    // 模型版本列表（仅显示当前项目的版本）
    ListModel {
        id: modelVersionListModel
    }

    function refreshModelVersions() {
        modelVersionListModel.clear()
        if (!appController.projectOpen) return
        var versions = modelRegistry.listModelVersions(currentProjectId)
        for (var i = 0; i < versions.length; i++) {
            var v = versions[i]
            modelVersionListModel.append({
                "versionId": v.id,
                "versionLabel": (v.id || "").substring(0, 8) + "..."
                                 + (v.bestWeightPath ? " | best" : "")
                                 + " | " + (v.createdAt || "")
            })
        }
    }

    function formatDuration(seconds) {
        if (!seconds || seconds <= 0) return "—"
        var m = Math.floor(seconds / 60)
        var s = Math.floor(seconds % 60)
        return m + ":" + (s < 10 ? "0" + s : s)
    }

    onCurrentProjectIdChanged: refreshModelVersions()
    Component.onCompleted: refreshModelVersions()

    Connections {
        target: appController
        function onCurrentProjectIdChanged() {
            refreshModelVersions()
        }
    }

    // P2-6 监听视频推理完成信号
    Connections {
        target: inferenceService

        function onVideoInferenceFinished(outPath, success, frames, dets, error) {
            processing = false
            if (success) {
                outputPath = outPath
                totalFrames = frames
                totalDetections = dets
                errorMessage = ""
            } else {
                errorMessage = error || "视频推理失败"
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // 标题栏
        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "视频流推理 (P2-6)"
                font.pixelSize: 24
                font.bold: true
                color: Theme.textMain
            }

            Item { Layout.fillWidth: true }

            Label {
                visible: !appController.projectOpen
                text: "请先打开项目"
                color: Theme.danger
                font.pixelSize: 14
            }
        }

        // 主内容区
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgCard
            radius: 8
            visible: appController.projectOpen

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 16

                // 模型版本选择
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Label {
                        text: "模型版本:"
                        color: Theme.textMuted
                        font.pixelSize: 14
                        Layout.preferredWidth: 100
                    }

                    ComboBox {
                        id: modelVersionCombo
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        model: modelVersionListModel
                        textRole: "versionLabel"
                        valueRole: "versionId"
                        enabled: !root.processing

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: modelVersionCombo.activeFocus ? Theme.primaryGlow : Theme.borderColor
                            border.width: 1
                        }

                        contentItem: Text {
                            text: modelVersionCombo.displayText
                            color: Theme.textMain
                            font.pixelSize: 13
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 8
                        }

                        onActivated: {
                            selectedModelVersionId = currentValue || ""
                        }
                    }

                    Button {
                        text: "刷新"
                        enabled: !root.processing
                        implicitHeight: 32
                        onClicked: refreshModelVersions()
                    }
                }

                // 视频文件选择
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Label {
                        text: "视频文件:"
                        color: Theme.textMuted
                        font.pixelSize: 14
                        Layout.preferredWidth: 100
                    }

                    TextField {
                        id: videoPathField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        readOnly: true
                        placeholderText: "请选择视频文件 (mp4/avi/mov)"
                        text: selectedVideoPath
                        color: Theme.textMain
                        font.pixelSize: 13
                        enabled: !root.processing

                        background: Rectangle {
                            color: Theme.bgInput
                            radius: Theme.radiusSmall
                            border.color: Theme.borderColor
                            border.width: 1
                        }
                    }

                    Button {
                        text: "浏览..."
                        enabled: !root.processing
                        implicitHeight: 32
                        onClicked: videoFileDialog.open()
                    }
                }

                // 阈值配置
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 24

                    Label {
                        text: "置信度阈值:"
                        color: Theme.textMuted
                        font.pixelSize: 14
                    }

                    SpinBox {
                        id: confSpin
                        from: 1
                        to: 100
                        value: 25
                        stepSize: 5
                        implicitHeight: 32
                        enabled: !root.processing
                        // 显示为 0.00-1.00
                        textFromValue: function(value) {
                            return (value / 100).toFixed(2)
                        }
                        valueFromText: function(text) {
                            return Math.round(parseFloat(text) * 100)
                        }
                    }

                    Label {
                        text: "IoU 阈值:"
                        color: Theme.textMuted
                        font.pixelSize: 14
                    }

                    SpinBox {
                        id: iouSpin
                        from: 1
                        to: 100
                        value: 45
                        stepSize: 5
                        implicitHeight: 32
                        enabled: !root.processing
                        textFromValue: function(value) {
                            return (value / 100).toFixed(2)
                        }
                        valueFromText: function(text) {
                            return Math.round(parseFloat(text) * 100)
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // 开始推理按钮
                RowLayout {
                    Layout.fillWidth: true

                    Button {
                        text: root.processing ? "推理中..." : "开始视频推理"
                        enabled: !root.processing
                                && selectedModelVersionId !== ""
                                && selectedVideoPath !== ""
                        highlighted: true
                        implicitHeight: 40
                        Layout.preferredWidth: 200

                        background: Rectangle {
                            color: parent.enabled
                                ? (parent.hovered ? Theme.primaryGlow : Theme.primary)
                                : Theme.bgInput
                            radius: Theme.radiusSmall
                        }

                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? "#FFFFFF" : Theme.textDisabled
                            font.pixelSize: 14
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            errorMessage = ""
                            outputPath = ""
                            totalFrames = 0
                            totalDetections = 0
                            processing = true
                            inferenceService.runVideoInference(
                                selectedModelVersionId,
                                selectedVideoPath,
                                "",  // 让 Service 自动派生输出路径
                                confSpin.value / 100,
                                iouSpin.value / 100
                            )
                        }
                    }

                    Label {
                        visible: root.processing
                        text: "正在逐帧推理，可能需要较长时间..."
                        color: Theme.warning
                        font.pixelSize: 12
                    }
                }

                // 错误提示
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: errorMessage ? 40 : 0
                    visible: errorMessage !== ""
                    color: Qt.alpha(Theme.danger, 0.1)
                    radius: Theme.radiusSmall
                    border.color: Theme.danger
                    border.width: 1

                    Label {
                        anchors.centerIn: parent
                        text: "错误: " + errorMessage
                        color: Theme.danger
                        font.pixelSize: 13
                    }
                }

                // 结果显示
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.bgMain
                    radius: Theme.radiusNormal
                    border.color: Theme.borderColor
                    border.width: 1
                    visible: outputPath !== "" || totalFrames > 0

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 12

                        Label {
                            text: "推理结果"
                            font.pixelSize: 16
                            font.bold: true
                            color: Theme.textMain
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 8
                            columnSpacing: 16

                            Label {
                                text: "总帧数:"
                                color: Theme.textMuted
                                font.pixelSize: 13
                            }
                            Label {
                                text: totalFrames
                                color: Theme.textMain
                                font.pixelSize: 14
                                font.bold: true
                            }

                            Label {
                                text: "总检测框数:"
                                color: Theme.textMuted
                                font.pixelSize: 13
                            }
                            Label {
                                text: totalDetections
                                color: Theme.primary
                                font.pixelSize: 14
                                font.bold: true
                            }

                            Label {
                                text: "平均每帧检测:"
                                color: Theme.textMuted
                                font.pixelSize: 13
                            }
                            Label {
                                text: totalFrames > 0
                                    ? (totalDetections / totalFrames).toFixed(2)
                                    : "—"
                                color: Theme.textMain
                                font.pixelSize: 14
                            }
                        }

                        Label {
                            text: "输出视频路径:"
                            color: Theme.textMuted
                            font.pixelSize: 13
                        }

                        TextField {
                            Layout.fillWidth: true
                            readOnly: true
                            text: outputPath
                            color: Theme.textMain
                            font.pixelSize: 12
                            font.family: Theme.fontFamilyMono

                            background: Rectangle {
                                color: Theme.bgInput
                                radius: Theme.radiusSmall
                                border.color: Theme.borderColor
                                border.width: 1
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Button {
                                text: "打开输出目录"
                                implicitHeight: 30
                                enabled: outputPath !== ""
                                onClicked: {
                                    // 调用 Qt.openUrlExternally 打开父目录
                                    var folderUrl = "file:///" + outputPath.substring(0, outputPath.lastIndexOf('/'))
                                    Qt.openUrlExternally(folderUrl)
                                }
                            }

                            Button {
                                text: "复制路径"
                                implicitHeight: 30
                                enabled: outputPath !== ""
                                onClicked: {
                                    // 复制到剪贴板（通过隐藏 TextField）
                                    outputPathField.text = outputPath
                                    outputPathField.selectAll()
                                    outputPathField.copy()
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        // 隐藏的 TextField 用于复制到剪贴板
                        TextField {
                            id: outputPathField
                            visible: false
                            text: outputPath
                        }
                    }
                }
            }
        }
    }

    // 文件选择对话框
    FileDialog {
        id: videoFileDialog
        title: "选择视频文件"
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "视频文件 (*.mp4 *.avi *.mov *.mkv *.wmv *.flv)",
            "所有文件 (*)"
        ]
        onAccepted: {
            selectedVideoPath = selectedFile.toString().replace("file:///", "")
            videoPathField.text = selectedVideoPath
        }
    }
}
