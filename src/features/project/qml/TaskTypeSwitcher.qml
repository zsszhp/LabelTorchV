// TaskTypeSwitcher.qml - Multi-task type switcher component
// Displays 4 toggle buttons for switching between task types:
// Detect (HBB), OBB, Classify, Anomaly
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme

RowLayout {
    id: root

    property string taskType: "detect"

    property bool switcherEnabled: true

    signal taskTypeSelected(string taskType)

    spacing: 2

    // Detect (HBB)
    Button {
        id: detectBtn
        text: "水平框"
        font.pixelSize: 11
        highlighted: root.taskType === "detect"
        flat: !highlighted
        enabled: root.switcherEnabled
        Layout.preferredWidth: 48
        Layout.preferredHeight: 26

        ToolTip.visible: hovered
        ToolTip.text: qsTr("Horizontal Bounding Box Detection")
        ToolTip.delay: 500

        onClicked: {
            root.taskType = "detect"
            root.taskTypeSelected("detect")
        }

        background: Rectangle {
            color: detectBtn.highlighted ? Theme.accentPrimary : (detectBtn.hovered ? Theme.bgHover : Theme.bgTertiary)
            radius: 3
        }

        contentItem: Label {
            text: detectBtn.text
            font.pixelSize: 11
            font.bold: detectBtn.highlighted
            color: detectBtn.highlighted ? Theme.textPrimary : (detectBtn.enabled ? Theme.textSecondary : Theme.textDisabled)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // OBB
    Button {
        id: obbBtn
        text: "旋转框"
        font.pixelSize: 11
        highlighted: root.taskType === "obb"
        flat: !highlighted
        enabled: root.switcherEnabled
        Layout.preferredWidth: 48
        Layout.preferredHeight: 26

        ToolTip.visible: hovered
        ToolTip.text: qsTr("Oriented Bounding Box Detection")
        ToolTip.delay: 500

        onClicked: {
            root.taskType = "obb"
            root.taskTypeSelected("obb")
        }

        background: Rectangle {
            color: obbBtn.highlighted ? Theme.accentSecondary : (obbBtn.hovered ? Theme.bgHover : Theme.bgTertiary)
            radius: 3
        }

        contentItem: Label {
            text: obbBtn.text
            font.pixelSize: 11
            font.bold: obbBtn.highlighted
            color: obbBtn.highlighted ? Theme.textPrimary : (obbBtn.enabled ? Theme.textSecondary : Theme.textDisabled)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // Classify
    Button {
        id: classifyBtn
        text: "分类"
        font.pixelSize: 11
        highlighted: root.taskType === "classify"
        flat: !highlighted
        enabled: root.switcherEnabled
        Layout.preferredWidth: 48
        Layout.preferredHeight: 26

        ToolTip.visible: hovered
        ToolTip.text: qsTr("Image Classification")
        ToolTip.delay: 500

        onClicked: {
            root.taskType = "classify"
            root.taskTypeSelected("classify")
        }

        background: Rectangle {
            color: classifyBtn.highlighted ? Theme.accentSuccess : (classifyBtn.hovered ? Theme.bgHover : Theme.bgTertiary)
            radius: 3
        }

        contentItem: Label {
            text: classifyBtn.text
            font.pixelSize: 11
            font.bold: classifyBtn.highlighted
            color: classifyBtn.highlighted ? Theme.textPrimary : (classifyBtn.enabled ? Theme.textSecondary : Theme.textDisabled)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // Anomaly
    Button {
        id: anomalyBtn
        text: "异常"
        font.pixelSize: 11
        highlighted: root.taskType === "anomaly"
        flat: !highlighted
        enabled: root.switcherEnabled
        Layout.preferredWidth: 48
        Layout.preferredHeight: 26

        ToolTip.visible: hovered
        ToolTip.text: qsTr("Anomaly Detection")
        ToolTip.delay: 500

        onClicked: {
            root.taskType = "anomaly"
            root.taskTypeSelected("anomaly")
        }

        background: Rectangle {
            color: anomalyBtn.highlighted ? Theme.accentWarning : (anomalyBtn.hovered ? Theme.bgHover : Theme.bgTertiary)
            radius: 3
        }

        contentItem: Label {
            text: anomalyBtn.text
            font.pixelSize: 11
            font.bold: anomalyBtn.highlighted
            color: anomalyBtn.highlighted ? Theme.bgPrimary : (anomalyBtn.enabled ? Theme.textSecondary : Theme.textDisabled)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
