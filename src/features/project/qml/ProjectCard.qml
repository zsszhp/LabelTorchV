// ProjectCard.qml - 项目卡片组件
import QtQuick
import QtQuick.Controls

Item {
    property string projectName: ""
    property string projectPath: ""
    property string lastModified: ""

    Rectangle {
        anchors.fill: parent
        color: "#313244"
        radius: 8

        Column {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 4

            Label { text: projectName; font.bold: true; color: "#cdd6f4"; font.pixelSize: 14 }
            Label { text: projectPath; color: "#6c7086"; font.pixelSize: 11 }
            Label { text: lastModified; color: "#6c7086"; font.pixelSize: 11 }
        }
    }
}
