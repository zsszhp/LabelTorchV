// ProjectCard.qml - 项目卡片组件
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme

Item {
    property string projectName: ""
    property string projectPath: ""
    property string lastModified: ""

    Rectangle {
        anchors.fill: parent
        color: Theme.bgCard
        radius: Theme.radiusNormal
        border.color: Theme.border
        border.width: 1

        Column {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 4

            Label { text: projectName; font.bold: true; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeNormal; font.family: Theme.fontFamily }
            Label { text: projectPath; color: Theme.textMuted; font.pixelSize: Theme.fontSizeCaption; font.family: Theme.fontFamilyMono }
            Label { text: lastModified; color: Theme.textMuted; font.pixelSize: Theme.fontSizeCaption; font.family: Theme.fontFamilyMono }
        }
    }
}
