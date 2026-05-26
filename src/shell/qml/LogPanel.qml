import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme

Rectangle {
    id: root
    color: Qt.alpha(Theme.bgPrimary, Theme.glassOpacity)
    border.color: Theme.border
    border.width: 1

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

        // 顶栏
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: Theme.bgSecondary
            border.color: Theme.border
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLarge
                anchors.rightMargin: Theme.spacingNormal
                spacing: Theme.spacingNormal

                Label {
                    text: "⌨ LOG PANEL"
                    color: Theme.accentPrimary
                    font.pixelSize: Theme.fontSizeSmall
                    font.bold: true
                    font.family: Theme.fontFamilyMono
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: autoScroll ? "SCROLL ACTIVE" : "SCROLL MANUAL"
                    color: autoScroll ? Theme.accentSuccess : Theme.textMuted
                    font.pixelSize: Theme.fontSizeCaption
                    font.bold: true
                    font.family: Theme.fontFamilyMono
                }

                Button {
                    text: "CLEAR"
                    flat: true
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: Theme.fontFamilyMono
                    palette.buttonText: Theme.textSecondary
                    onClicked: root.clear()
                }

                Button {
                    text: collapsed ? "EXPAND ▲" : "COLLAPSE ▼"
                    flat: true
                    font.pixelSize: Theme.fontSizeCaption
                    font.family: Theme.fontFamilyMono
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
