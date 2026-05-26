import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme

/**
 * 异常检测推理面板 V2
 *
 * 对接 C++ AnomalyDetector 推理结果，
 * 支持半透明伪彩热力图叠加显示、OK/NG 状态标记、
 * anomaly_score 进度条、不透明度滑块控制
 */
Rectangle {
    id: root

    color: Theme.bgPrimary

    // 外部属性
    property string currentProjectId: ""
    property string currentWeightPath: ""

    // 内部状态
    property string selectedModel: "patchcore"
    property string selectedDevice: "auto"
    property int selectedImgSize: 256
    property bool isInfering: false
    property bool modelLoaded: anomalyDetector ? anomalyDetector.isLoaded() : false

    // 当前推理结果
    property var currentResult: null
    property real anomalyScore: 0
    property int isAnomalous: 0
    property string heatmapBase64: ""
    property real heatmapOpacity: 0.6

    // 原始图片
    property string currentImagePath: ""

    // 推理历史
    property var inferenceHistory: []

    // NG 闪烁动画
    property bool ngFlash: false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLarge
        spacing: Theme.spacingNormal

        // 标题行
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingNormal

            Label {
                text: "异常检测推理"
                font.pixelSize: Theme.fontSizeLarge
                font.bold: true
                color: Theme.textPrimary
            }

            Item { Layout.fillWidth: true }

            // 模型加载状态
            Rectangle {
                Layout.preferredHeight: 28
                Layout.preferredWidth: modelStatusText.implicitWidth + 20
                radius: Theme.radiusSmall
                color: root.modelLoaded ? Theme.accentSuccess : Theme.bgTertiary
                border.color: root.modelLoaded ? Theme.accentSuccess : Theme.border

                Label {
                    id: modelStatusText
                    anchors.centerIn: parent
                    text: root.modelLoaded ? "模型已加载" : "模型未加载"
                    color: root.modelLoaded ? "#FFFFFF" : Theme.textMuted
                    font.pixelSize: Theme.fontSizeCaption
                }
            }
        }

        // 推理配置区域
        Rectangle {
            Layout.fillWidth: true
            color: Theme.bgSecondary
            radius: Theme.radiusNormal
            border.color: Theme.border
            implicitHeight: configGrid.implicitHeight + Theme.spacingLarge * 2

            GridLayout {
                id: configGrid
                anchors.fill: parent
                anchors.margins: Theme.spacingLarge
                columns: 4
                rowSpacing: Theme.spacingSmall
                columnSpacing: Theme.spacingNormal

                Label {
                    text: "权重文件："
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeNormal
                }
                Label {
                    Layout.fillWidth: true
                    text: root.currentWeightPath ? root.currentWeightPath.split("/").pop().split("\\").pop() : "未选择"
                    color: root.currentWeightPath ? Theme.textPrimary : Theme.textMuted
                    font.pixelSize: Theme.fontSizeNormal
                    elide: Text.ElideMiddle
                }

                Label {
                    text: "推理设备："
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeNormal
                }
                ComboBox {
                    id: deviceCombo
                    Layout.fillWidth: true
                    model: ["auto", "cpu", "0", "1"]
                    currentIndex: 0
                    onCurrentTextChanged: root.selectedDevice = currentText
                    background: Rectangle {
                        color: Theme.bgTertiary
                        radius: Theme.radiusSmall
                        border.color: Theme.border
                    }
                    contentItem: Label {
                        text: deviceCombo.displayText
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeNormal
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Label {
                    text: "图片尺寸："
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeNormal
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
                        radius: Theme.radiusSmall
                        border.color: Theme.border
                    }
                }

                // 加载模型按钮
                Label { text: "" }
                Button {
                    Layout.fillWidth: true
                    text: root.modelLoaded ? "重新加载模型" : "加载 ONNX 模型"
                    enabled: root.currentWeightPath !== ""
                    onClicked: {
                        if (anomalyDetector) {
                            var ok = anomalyDetector.loadModel(root.currentWeightPath);
                            root.modelLoaded = anomalyDetector.isLoaded();
                        }
                    }
                    background: Rectangle {
                        implicitHeight: 32
                        color: parent.enabled ? Theme.accentSecondary : Theme.bgTertiary
                        radius: Theme.radiusSmall
                    }
                    contentItem: Label {
                        text: parent.text
                        color: parent.enabled ? "#FFFFFF" : Theme.textMuted
                        font.pixelSize: Theme.fontSizeNormal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        // 图片选择与推理按钮
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSmall

            Button {
                text: "选择图片"
                onClicked: imageFileDialog.open()
                background: Rectangle {
                    implicitWidth: 90
                    implicitHeight: 36
                    color: Theme.bgTertiary
                    radius: Theme.radiusSmall
                    border.color: Theme.border
                }
                contentItem: Label {
                    text: parent.text
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeNormal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                text: root.isInfering ? "推理中..." : "开始推理"
                enabled: !root.isInfering && root.modelLoaded && root.currentImagePath !== ""
                onClicked: runInference()
                background: Rectangle {
                    implicitWidth: 120
                    implicitHeight: 36
                    color: parent.enabled ? Theme.accentPrimary : Theme.bgTertiary
                    radius: Theme.radiusSmall
                }
                contentItem: Label {
                    text: parent.text
                    color: parent.enabled ? "#FFFFFF" : Theme.textMuted
                    font.pixelSize: Theme.fontSizeNormal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.currentImagePath ? root.currentImagePath.split("/").pop().split("\\").pop() : ""
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeNormal
                elide: Text.ElideMiddle
            }
        }

        // 热力图叠加显示区域
        Rectangle {
            id: imageArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 300
            color: Theme.bgSecondary
            radius: Theme.radiusNormal
            border.color: root.isAnomalous === 1 ? (root.ngFlash ? Theme.accentError : "transparent") : Theme.border
            border.width: root.isAnomalous === 1 ? 3 : 1

            // NG 闪烁动画
            SequentialAnimation on ngFlash {
                running: root.isAnomalous === 1
                loops: Animation.Infinite
                PropertyAction { value: true }
                PauseAnimation { duration: 500 }
                PropertyAction { value: false }
                PauseAnimation { duration: 500 }
            }

            // OK/NG 状态标签
            Rectangle {
                id: statusBadge
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: Theme.spacingNormal
                width: statusText.implicitWidth + 24
                height: 36
                radius: Theme.radiusSmall
                color: root.isAnomalous === 1 ? Theme.accentError : Theme.accentSuccess
                visible: root.currentResult !== null

                Label {
                    id: statusText
                    anchors.centerIn: parent
                    text: root.isAnomalous === 1 ? "NG" : "OK"
                    color: "#FFFFFF"
                    font.pixelSize: Theme.fontSizeLarge
                    font.bold: true
                }
            }

            // 异常分数显示
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: Theme.spacingNormal
                width: scoreColumn.implicitWidth + 20
                height: scoreColumn.implicitHeight + 16
                radius: Theme.radiusSmall
                color: "#CC0D0E15"
                visible: root.currentResult !== null

                ColumnLayout {
                    id: scoreColumn
                    anchors.centerIn: parent
                    spacing: 4

                    Label {
                        text: "异常评分"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeCaption
                    }

                    Label {
                        text: (root.anomalyScore * 100).toFixed(1) + "%"
                        color: root.isAnomalous === 1 ? Theme.accentError : Theme.accentSuccess
                        font.pixelSize: Theme.fontSizeDisplay
                        font.bold: true
                    }

                    // 进度条
                    ProgressBar {
                        Layout.preferredWidth: 120
                        Layout.preferredHeight: 6
                        from: 0
                        to: 1
                        value: root.anomalyScore
                        background: Rectangle {
                            radius: 3
                            color: Theme.bgTertiary
                        }
                        contentItem: Rectangle {
                            implicitWidth: 120
                            implicitHeight: 6
                            radius: 3
                            color: root.isAnomalous === 1 ? Theme.accentError : Theme.accentSuccess
                            width: parent.visualPosition * parent.width
                        }
                    }
                }
            }

            // 原始图片
            Image {
                id: originalImage
                anchors.fill: parent
                anchors.margins: Theme.spacingNormal
                fillMode: Image.PreserveAspectFit
                source: root.currentImagePath ? "file:///" + root.currentImagePath : ""
                visible: root.currentImagePath !== ""
                asynchronous: true
                cache: false
            }

            // 热力图叠加层
            Canvas {
                id: heatmapCanvas
                anchors.fill: originalImage
                visible: root.heatmapBase64 !== ""

                onPaint: {
                    if (!root.heatmapBase64) return;

                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    // 加载 base64 热力图
                    var heatImg = new Image();
                    heatImg.onload = function() {
                        ctx.globalAlpha = root.heatmapOpacity;
                        ctx.drawImage(heatImg, 0, 0, width, height);
                        ctx.globalAlpha = 1.0;
                    };
                    heatImg.src = "data:image/png;base64," + root.heatmapBase64;
                }

                onVisibleChanged: {
                    if (visible) requestPaint();
                }

                Connections {
                    target: root
                    function onHeatmapBase64Changed() {
                        heatmapCanvas.requestPaint();
                    }
                    function onHeatmapOpacityChanged() {
                        heatmapCanvas.requestPaint();
                    }
                }
            }

            // 无图片占位
            Label {
                anchors.centerIn: parent
                text: "选择图片后进行异常检测推理"
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeNormal
                visible: root.currentImagePath === ""
            }
        }

        // 热力图不透明度控制
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingNormal
            visible: root.heatmapBase64 !== ""

            Label {
                text: "热力图不透明度："
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeNormal
            }

            Slider {
                id: opacitySlider
                Layout.fillWidth: true
                from: 0
                to: 1
                value: 0.6
                stepSize: 0.05
                onValueChanged: root.heatmapOpacity = value
                background: Rectangle {
                    x: opacitySlider.leftPadding
                    y: opacitySlider.topPadding + opacitySlider.availableHeight / 2 - height / 2
                    implicitWidth: 200
                    implicitHeight: 4
                    width: opacitySlider.availableWidth
                    height: implicitHeight
                    radius: 2
                    color: Theme.bgTertiary

                    Rectangle {
                        width: opacitySlider.visualPosition * parent.width
                        height: parent.height
                        radius: 2
                        color: Theme.accentPrimary
                    }
                }
                handle: Rectangle {
                    x: opacitySlider.leftPadding + opacitySlider.visualPosition * (opacitySlider.availableWidth - width)
                    y: opacitySlider.topPadding + opacitySlider.availableHeight / 2 - height / 2
                    implicitWidth: 16
                    implicitHeight: 16
                    radius: 8
                    color: opacitySlider.pressed ? Theme.accentPrimary : Theme.textPrimary
                    border.color: Theme.accentPrimary
                }
            }

            Label {
                text: (root.heatmapOpacity * 100).toFixed(0) + "%"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeNormal
                Layout.preferredWidth: 40
            }
        }

        // 推理历史列表
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 160
            color: Theme.bgSecondary
            radius: Theme.radiusNormal
            border.color: Theme.border

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingNormal
                spacing: Theme.spacingSmall

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: "推理历史"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSubheading
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: root.inferenceHistory.length + " 条记录"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSizeCaption
                    }

                    Button {
                        text: "清空"
                        onClicked: root.inferenceHistory = []
                        background: Rectangle {
                            implicitWidth: 50
                            implicitHeight: 24
                            color: Theme.bgTertiary
                            radius: Theme.radiusSmall
                        }
                        contentItem: Label {
                            text: parent.text
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeCaption
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                ListView {
                    id: historyList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 2
                    model: root.inferenceHistory

                    delegate: Rectangle {
                        width: historyList.width
                        height: 36
                        radius: Theme.radiusSmall
                        color: modelData.isAnomalous === 1 ? "#1A0D0E15" : Theme.bgTertiary
                        border.color: modelData.isAnomalous === 1 ? Theme.accentError : "transparent"
                        border.width: modelData.isAnomalous === 1 ? 1 : 0

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingNormal
                            anchors.rightMargin: Theme.spacingNormal
                            spacing: Theme.spacingNormal

                            // OK/NG 小标签
                            Rectangle {
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 20
                                radius: Theme.radiusSmall
                                color: modelData.isAnomalous === 1 ? Theme.accentError : Theme.accentSuccess

                                Label {
                                    anchors.centerIn: parent
                                    text: modelData.isAnomalous === 1 ? "NG" : "OK"
                                    color: "#FFFFFF"
                                    font.pixelSize: Theme.fontSizeCaption
                                    font.bold: true
                                }
                            }

                            // 文件名
                            Label {
                                Layout.fillWidth: true
                                text: modelData.fileName || ""
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeNormal
                                elide: Text.ElideMiddle
                            }

                            // 异常评分
                            Label {
                                text: (modelData.anomalyScore * 100).toFixed(1) + "%"
                                color: modelData.isAnomalous === 1 ? Theme.accentError : Theme.accentSuccess
                                font.pixelSize: Theme.fontSizeNormal
                                font.bold: true
                            }

                            // 查看按钮
                            Button {
                                text: "查看"
                                onClicked: {
                                    root.currentImagePath = modelData.imagePath;
                                    root.anomalyScore = modelData.anomalyScore;
                                    root.isAnomalous = modelData.isAnomalous;
                                    root.heatmapBase64 = modelData.heatmapBase64 || "";
                                    root.currentResult = modelData;
                                }
                                background: Rectangle {
                                    implicitWidth: 40
                                    implicitHeight: 22
                                    color: Theme.accentSecondary
                                    radius: Theme.radiusSmall
                                }
                                contentItem: Label {
                                    text: parent.text
                                    color: "#FFFFFF"
                                    font.pixelSize: Theme.fontSizeCaption
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 文件选择对话框
    FileDialog {
        id: imageFileDialog
        title: "选择图片"
        nameFilters: ["图片文件 (*.png *.jpg *.jpeg *.bmp *.tiff)", "所有文件 (*)"]
        onAccepted: {
            var path = fileDialogHelper.urlToPath(imageFileDialog.selectedFile);
            root.currentImagePath = path;
        }
    }

    // 辅助对象：URL 转本地路径
    QtObject {
        id: fileDialogHelper
        function urlToPath(url) {
            var path = url.toString();
            if (path.startsWith("file:///")) {
                path = path.substring(8);
            } else if (path.startsWith("file://")) {
                path = path.substring(7);
            }
            return decodeURIComponent(path);
        }
    }

    // 执行推理
    function runInference() {
        if (!anomalyDetector || !root.modelLoaded || !root.currentImagePath) return;

        root.isInfering = true;
        root.currentResult = null;
        root.heatmapBase64 = "";
        root.anomalyScore = 0;
        root.isAnomalous = 0;

        // 调用 C++ AnomalyDetector 推理
        var result = anomalyDetector.infer(root.currentImagePath);

        root.isInfering = false;

        if (result) {
            root.currentResult = result;
            root.anomalyScore = parseFloat(result.anomalyScore) || 0;
            root.isAnomalous = parseInt(result.isAnomalous) || 0;
            root.heatmapBase64 = result.heatmapImage || "";

            // 添加到历史记录
            var entry = {
                "fileName": root.currentImagePath.split("/").pop().split("\\").pop(),
                "imagePath": root.currentImagePath,
                "anomalyScore": root.anomalyScore,
                "isAnomalous": root.isAnomalous,
                "heatmapBase64": root.heatmapBase64
            };

            var history = root.inferenceHistory.slice();
            history.unshift(entry);
            if (history.length > 50) history = history.slice(0, 50);
            root.inferenceHistory = history;
        }
    }

    // 连接 AnomalyDetector 信号
    Connections {
        target: anomalyDetector
        function onInferenceCompleted(result) {
            root.isInfering = false;
        }
        function onInferenceFailed(error) {
            root.isInfering = false;
        }
    }
}
