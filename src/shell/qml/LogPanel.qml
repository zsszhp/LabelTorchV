// LogPanel.qml - V4 日志面板（赛博蓝科技风）
// P1-25：环形缓冲保留最近 2000 行，批量刷新文本，防止长时间任务日志刷爆内存
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme

Rectangle {
    id: root
    color: Theme.bgSide
    border.color: Theme.borderColor
    border.width: 1

    property alias logText: logArea.text
    property bool autoScroll: true
    property bool collapsed: false

    // 日志级别显示阈值：0=DEBUG 1=INFO 2=WARNING 3=ERROR（与设置页下标对齐）
    // 低于阈值的行不进入展示缓冲，设置改动即时生效
    property int minLevel: 1

    // 环形缓冲：最多保留 maxLogLines 行
    property int maxLogLines: 2000
    property var logLines: []
    property bool refreshPending: false

    // 解析行首级别标记：[ERROR] / [WARN] / [INFO] / [LOG] / [DEBUG]
    // 未带标记的行按 INFO 处理
    function levelOfLine(line) {
        if (line.indexOf("[ERROR]") === 0 || line.indexOf("[CRIT]") === 0) return 3
        if (line.indexOf("[WARN]") === 0) return 2
        if (line.indexOf("[INFO]") === 0) return 1
        if (line.indexOf("[LOG]") === 0 || line.indexOf("[DEBUG]") === 0 || line.indexOf("[TRACE]") === 0) return 0
        return 1
    }

    function appendLog(line) {
        if (line === undefined || line === null) return
        var text = String(line)
        var parts = text.split("\n")
        for (var i = 0; i < parts.length; i++) {
            // 按当前 minLevel 阈值过滤，低于阈值的行直接丢弃
            if (levelOfLine(parts[i]) >= minLevel) {
                logLines.push(parts[i])
            }
        }
        if (logLines.length > maxLogLines) {
            logLines = logLines.slice(logLines.length - maxLogLines)
        }
        scheduleRefresh()
    }

    // 批量追加（后端合并推送的行数组）
    function appendLogBatch(lines) {
        if (!lines || lines.length === 0) return
        for (var i = 0; i < lines.length; i++) {
            if (levelOfLine(String(lines[i])) >= minLevel) {
                logLines.push(String(lines[i]))
            }
        }
        if (logLines.length > maxLogLines) {
            logLines = logLines.slice(logLines.length - maxLogLines)
        }
        scheduleRefresh()
    }

    function clear() {
        logLines = []
        logArea.text = ""
    }

    // 一帧内合并多次 append，避免每行都触发 TextEdit 全量重排
    function scheduleRefresh() {
        if (refreshPending) return
        refreshPending = true
        Qt.callLater(function() {
            refreshPending = false
            logArea.text = logLines.join("\n")
            if (autoScroll && !collapsed) {
                logFlickable.contentY = logFlickable.contentHeight - logFlickable.height
            }
        })
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 顶栏
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: Theme.bgCard
            border.color: Theme.borderColor
            border.width: 1

            // 双击折叠热区（置于底层，避免拦截右侧按钮点击）
            MouseArea {
                anchors.fill: parent
                z: -1
                onDoubleClicked: collapsed = !collapsed
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLarge
                anchors.rightMargin: Theme.spacingNormal
                spacing: Theme.spacingNormal

                // 日志图标
                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: Theme.primary
                    opacity: 0.8
                }

                Label {
                    text: "日志面板"
                    color: Theme.primary
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: true
                    font.family: Theme.fontFamily
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: autoScroll ? "自动滚动" : "手动滚动"
                    color: autoScroll ? Theme.success : Theme.textMuted
                    font.pixelSize: Theme.fontSizeCaption
                    font.bold: true
                    font.family: Theme.fontFamilyMono
                }

                Button {
                    text: "清空"
                    flat: true
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: Theme.fontFamily
                    palette.buttonText: Theme.textMuted
                    onClicked: root.clear()
                }

                Button {
                    text: collapsed ? "展开 ▲" : "折叠 ▼"
                    flat: true
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: Theme.fontFamily
                    palette.buttonText: Theme.primary
                    onClicked: {
                        collapsed = !collapsed
                    }
                }
            }
        }

        // 内容区
        Flickable {
            id: logFlickable
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !collapsed
            clip: true
            contentWidth: logArea.width
            contentHeight: logArea.height
            leftMargin: Theme.spacingLarge
            topMargin: Theme.spacingNormal
            rightMargin: Theme.spacingLarge
            bottomMargin: Theme.spacingNormal

            ScrollBar.vertical: ScrollBar {
                active: true
                policy: ScrollBar.AsNeeded
            }

            TextEdit {
                id: logArea
                readOnly: true
                selectByMouse: true
                color: Theme.textMain
                font.pixelSize: Theme.fontSizeSmall
                font.family: Theme.fontFamilyMono
                wrapMode: TextEdit.NoWrap
                text: ""
                onTextChanged: {
                    if (autoScroll && !collapsed) {
                        logFlickable.contentY = logFlickable.contentHeight - logFlickable.height
                    }
                }
            }
        }
    }
}
