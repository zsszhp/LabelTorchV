import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.bgTertiary

    property alias logText: logArea.text
    property bool autoScroll: true
    property bool collapsed: false

    function appendLog(line) {
        if (logArea.text.length > 0) {
            logArea.text += "\n"
        }
        logArea.text += line
        if (autoScroll && !collapsed) {
            logFlickable.contentY = logFlickable.contentHeight - logFlickable.height
        }
        if (logArea.text.length > 500000) {
            logArea.text = logArea.text.substring(logArea.text.length - 400000)
        }
    }

    function clear() {
        logArea.text = ""
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: Theme.bgSecondary
            radius: 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                spacing: 8

                Label {
                    text: "\u25B2 日志"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: autoScroll ? "自动滚动" : "手动"
                    color: autoScroll ? Theme.accentSuccess : Theme.textMuted
                    font.pixelSize: Theme.fontSizeSmall
                }

                Button {
                    text: "清除"
                    flat: true
                    font.pixelSize: Theme.fontSizeSmall
                    palette.buttonText: Theme.textMuted
                    onClicked: root.clear()
                }

                Button {
                    text: collapsed ? "展开" : "折叠"
                    flat: true
                    font.pixelSize: Theme.fontSizeSmall
                    palette.buttonText: Theme.accentPrimary
                    onClicked: {
                        collapsed = !collapsed
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                onDoubleClicked: collapsed = !collapsed
            }
        }

        Flickable {
            id: logFlickable
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !collapsed
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
                color: Theme.textPrimary
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
