// ColorPicker.qml - 颜色选择器（对标参考UI添加标签弹窗）
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme

Item {
    id: root
    width: 200
    height: 28
    implicitWidth: 200
    implicitHeight: 28

    property color selectedColor: "#0077FF"
    signal colorSelected(color color)

    Row {
        anchors.fill: parent
        spacing: Theme.spacingSmall

        Rectangle {
            width: 24
            height: 24
            anchors.verticalCenter: parent.verticalCenter
            color: root.selectedColor
            radius: Theme.radiusSmall
            border.color: Theme.borderColor
            border.width: 1
        }

        Button {
            height: 28
            text: "调色板"
            font.pixelSize: Theme.fontSizeCaption
            flat: true
            palette.buttonText: Theme.textSecondary
            background: Rectangle {
                color: parent.hovered ? Theme.bgHover : Theme.bgCard
                border.color: Theme.borderColor
                border.width: 1
                radius: Theme.radiusSmall
            }
            onClicked: colorGrid.visible = !colorGrid.visible
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.selectedColor
            font.pixelSize: Theme.fontSizeCaption
            font.family: Theme.fontFamilyMono
            color: Theme.textMuted
        }
    }

    Rectangle {
        id: colorGrid
        visible: false
        width: 200
        height: 80
        anchors.top: parent.bottom
        anchors.topMargin: 4
        color: Theme.bgCard
        border.color: Theme.borderColor
        border.width: 1
        radius: Theme.radiusNormal
        z: 100

        Grid {
            anchors.fill: parent
            anchors.margins: Theme.spacingSmall
            columns: 8
            spacing: 2

            Repeater {
                model: [
                    "#FF1744", "#FF9100", "#FFD600", "#00E676",
                    "#00E5FF", "#0077FF", "#D500F9", "#E879F9",
                    "#F87171", "#FB923C", "#FBBF24", "#36D399",
                    "#38BDF8", "#4DA6FF", "#B794F6", "#F472B6",
                    "#FFFFFF", "#E2E8F0", "#94A3B8", "#64748B",
                    "#475569", "#262F3D", "#1E2530", "#0F131A"
                ]

                Rectangle {
                    width: 20
                    height: 20
                    color: modelData
                    radius: 2
                    border.color: root.selectedColor === modelData ? Theme.primaryGlow : "transparent"
                    border.width: 2

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.selectedColor = modelData
                            root.colorSelected(modelData)
                            colorGrid.visible = false
                        }
                    }
                }
            }
        }
    }
}
