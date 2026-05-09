// ProjectPage.qml - 项目管理页面
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Label {
            text: "项目管理"
            font.pixelSize: 24
            font.bold: true
            color: "#cdd6f4"
        }

        Label {
            text: "创建或打开一个项目开始工作"
            font.pixelSize: 14
            color: "#a6adc8"
        }

        Button {
            text: "新建项目"
            onClicked: {
                // TODO: 打开新建项目对话框（Task 3）
            }
        }

        Button {
            text: "打开项目"
            onClicked: {
                // TODO: 打开项目选择对话框（Task 3）
            }
        }

        Item { Layout.fillHeight: true }
    }
}
