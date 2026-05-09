// TaskTypeSwitcher.qml - Multi-task type switcher component
// Displays 4 toggle buttons for switching between task types:
// Detect (HBB), OBB, Classify, Anomaly
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    /// Current task type: "detect", "obb", "classify", "anomaly"
    property string taskType: "detect"

    /// Whether the switcher is enabled (e.g. only when a project is open)
    property bool switcherEnabled: true

    signal taskTypeSelected(string taskType)

    spacing: 2

    // Detect (HBB)
    Button {
        id: detectBtn
        text: "HBB"
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
            color: detectBtn.highlighted ? "#89b4fa" : (detectBtn.hovered ? "#45475a" : "#313244")
            radius: 3
        }

        contentItem: Label {
            text: detectBtn.text
            font.pixelSize: 11
            font.bold: detectBtn.highlighted
            color: detectBtn.highlighted ? "#1e1e2e" : (detectBtn.enabled ? "#cdd6f4" : "#585b70")
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // OBB
    Button {
        id: obbBtn
        text: "OBB"
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
            color: obbBtn.highlighted ? "#cba6f7" : (obbBtn.hovered ? "#45475a" : "#313244")
            radius: 3
        }

        contentItem: Label {
            text: obbBtn.text
            font.pixelSize: 11
            font.bold: obbBtn.highlighted
            color: obbBtn.highlighted ? "#1e1e2e" : (obbBtn.enabled ? "#cdd6f4" : "#585b70")
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // Classify
    Button {
        id: classifyBtn
        text: "CLS"
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
            color: classifyBtn.highlighted ? "#a6e3a1" : (classifyBtn.hovered ? "#45475a" : "#313244")
            radius: 3
        }

        contentItem: Label {
            text: classifyBtn.text
            font.pixelSize: 11
            font.bold: classifyBtn.highlighted
            color: classifyBtn.highlighted ? "#1e1e2e" : (classifyBtn.enabled ? "#cdd6f4" : "#585b70")
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // Anomaly
    Button {
        id: anomalyBtn
        text: "AD"
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
            color: anomalyBtn.highlighted ? "#f9e2af" : (anomalyBtn.hovered ? "#45475a" : "#313244")
            radius: 3
        }

        contentItem: Label {
            text: anomalyBtn.text
            font.pixelSize: 11
            font.bold: anomalyBtn.highlighted
            color: anomalyBtn.highlighted ? "#1e1e2e" : (anomalyBtn.enabled ? "#cdd6f4" : "#585b70")
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
