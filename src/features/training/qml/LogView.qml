// LogView.qml - Training log viewer with auto-scroll
import QtQuick
import QtQuick.Controls
import LabelTorch.Shell
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#11111b"
    radius: 6

    property alias logText: logArea.text
    property bool autoScroll: true

    function appendLog(line) {
        if (logArea.text.length > 0) {
            logArea.text += "\n"
        }
        logArea.text += line
        if (autoScroll) {
            logFlickable.contentY = logFlickable.contentHeight - logFlickable.height
        }
    }

    function clear() {
        logArea.text = ""
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
                    text: "Training Log"
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: autoScroll ? "Auto-scroll: ON" : "Auto-scroll: OFF"
                    color: autoScroll ? Theme.accentSuccess : Theme.textMuted
                    font.pixelSize: 11
                }

                Button {
                    text: "Clear"
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
                color: Theme.textPrimary
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
