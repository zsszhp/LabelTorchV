// LogView.qml - Training log viewer with auto-scroll
// P1-25：环形缓冲保留最近 2000 行，批量刷新文本，避免万行日志刷爆 UI 线程
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.bgInput
    radius: 6

    property alias logText: logArea.text
    property bool autoScroll: true

    // 环形缓冲：最多保留 maxLogLines 行，超出后丢弃最旧行
    property int maxLogLines: 2000
    property var logLines: []
    property bool refreshPending: false

    // 追加一行日志（内部合并刷新，避免每行都触发 TextEdit 重排）
    function appendLog(line) {
        if (line === undefined || line === null) return
        var text = String(line)
        // 支持一次追加多行（批量日志）
        var parts = text.split("\n")
        for (var i = 0; i < parts.length; i++) {
            logLines.push(parts[i])
        }
        // 环形截断：超出容量时一次性丢弃最旧的
        if (logLines.length > maxLogLines) {
            logLines = logLines.slice(logLines.length - maxLogLines)
        }
        scheduleRefresh()
    }

    // 批量追加（后端合并推送的行数组）
    function appendLogBatch(lines) {
        if (!lines || lines.length === 0) return
        for (var i = 0; i < lines.length; i++) {
            logLines.push(String(lines[i]))
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

    // 合并刷新：一帧内多次 appendLog 只重建一次文本
    function scheduleRefresh() {
        if (refreshPending) return
        refreshPending = true
        Qt.callLater(function() {
            refreshPending = false
            logArea.text = logLines.join("\n")
            if (autoScroll) {
                logFlickable.contentY = logFlickable.contentHeight - logFlickable.height
            }
        })
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: Theme.bgCard
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                spacing: 8

                Label {
                    text: "训练日志"
                    color: Theme.textMuted
                    font.pixelSize: 12
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: autoScroll ? "自动滚动：开" : "自动滚动：关"
                    color: autoScroll ? Theme.success : Theme.textMuted
                    font.pixelSize: 11
                }

                Button {
                    text: "清除"
                    flat: true
                    font.pixelSize: 11
                    palette.buttonText: Theme.textMuted
                    onClicked: root.clear()
                }
            }
        }

        // Log content area
        Flickable {
            id: logFlickable
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: logArea.width
            contentHeight: logArea.height

            ScrollBar.vertical: ScrollBar {
                active: true
                policy: ScrollBar.AsNeeded
            }

            TextEdit {
                id: logArea
                readOnly: true
                selectByMouse: true
                color: Theme.textMain
                font.pixelSize: 12
                font.family: "Consolas, Courier New, monospace"
                wrapMode: TextEdit.NoWrap
                text: ""
                onTextChanged: {
                    if (autoScroll) {
                        logFlickable.contentY = logFlickable.contentHeight - logFlickable.height
                    }
                }
            }
        }
    }
}
