// Main.qml - 标炬主窗口
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    title: "标炬 LabelTorch"
    color: "#1e1e2e"

    // 主布局：左导航 + 中央内容 + 底部日志
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // 左侧导航栏
        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: "#181825"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 0
                spacing: 0

                // 应用标题
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    color: "#11111b"

                    Label {
                        anchors.centerIn: parent
                        text: "标炬 LabelTorch"
                        font.pixelSize: 16
                        font.bold: true
                        color: "#cdd6f4"
                    }
                }

                // 当前项目信息
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: appController.projectOpen ? 40 : 0
                    visible: appController.projectOpen
                    color: "#1e1e2e"

                    Label {
                        anchors.centerIn: parent
                        text: appController.currentProjectName
                        font.pixelSize: 12
                        color: "#89b4fa"
                        elide: Text.ElideRight
                        width: parent.width - 16
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                // 导航列表
                ListView {
                    id: navList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.topMargin: 8
                    clip: true
                    model: [
                        { name: "项目管理", icon: "\u25C6", page: "project" },
                        { name: "类别体系", icon: "\u25B6", page: "taxonomy" },
                        { name: "数据导入", icon: "\u25BC", page: "dataset" },
                        { name: "标注工作台", icon: "\u270E", page: "annotation" },
                        { name: "训练工作台", icon: "\u2699", page: "training" },
                        { name: "版本中心", icon: "\u25C8", page: "model" },
                        { name: "导出中心", icon: "\u25A0", page: "export" }
                    ]
                    delegate: ItemDelegate {
                        width: navList.width
                        height: 44
                        highlighted: appController.currentPage === modelData.page
                        enabled: modelData.page === "project" || modelData.page === "taxonomy" || appController.projectOpen

                        contentItem: Row {
                            spacing: 10
                            leftPadding: 16
                            Label { text: modelData.icon; font.pixelSize: 16; color: parent.parent.enabled ? "#cdd6f4" : "#585b70"; anchors.verticalCenter: parent.verticalCenter }
                            Label {
                                text: modelData.name
                                font.pixelSize: 14
                                color: highlighted ? "#89b4fa" : (parent.parent.enabled ? "#cdd6f4" : "#585b70")
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        background: Rectangle {
                            color: highlighted ? "#313244" : (parent.hovered ? "#1e1e2e" : "transparent")
                            radius: 4
                        }

                        onClicked: appController.currentPage = modelData.page
                    }
                }

                // 底部状态
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    color: "#11111b"

                    Label {
                        anchors.centerIn: parent
                        text: "Python 后端: 未连接"
                        font.pixelSize: 11
                        color: "#6c7086"
                    }
                }
            }
        }

        // 右侧内容区
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1e1e2e"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // 顶部状态栏
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    color: "#181825"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16

                        Label {
                            text: {
                                switch(appController.currentPage) {
                                    case "project": return "项目管理"
                                    case "taxonomy": return "类别体系"
                                    case "dataset": return "数据导入"
                                    case "annotation": return "标注工作台"
                                    case "training": return "训练工作台"
                                    case "model": return "版本中心"
                                    case "export": return "导出中心"
                                    default: return "标炬"
                                }
                            }
                            font.pixelSize: 14
                            font.bold: true
                            color: "#cdd6f4"
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            visible: appController.projectOpen
                            text: appController.currentProjectName
                            font.pixelSize: 12
                            color: "#a6adc8"
                        }
                    }
                }

                // 中央内容
                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: {
                        switch(appController.currentPage) {
                            case "project": return 0
                            case "taxonomy": return 1
                            case "dataset": return 2
                            case "annotation": return 3
                            case "training": return 4
                            case "model": return 5
                            case "export": return 6
                            default: return 0
                        }
                    }

                    // 项目管理页
                    Loader { source: "qrc:/LabelTorch/Project/qml/ProjectPage.qml" }
                    // 类别体系页
                    Loader { source: "qrc:/LabelTorch/Project/qml/TaxonomyPage.qml" }
                    // 数据导入页
                    Rectangle { color: "#1e1e2e"; Label { anchors.centerIn: parent; text: "数据导入 - 待实现"; color: "#6c7086"; font.pixelSize: 16 } }
                    // 标注工作台
                    Rectangle { color: "#1e1e2e"; Label { anchors.centerIn: parent; text: "标注工作台 - 待实现"; color: "#6c7086"; font.pixelSize: 16 } }
                    // 训练工作台
                    Rectangle { color: "#1e1e2e"; Label { anchors.centerIn: parent; text: "训练工作台 - 待实现"; color: "#6c7086"; font.pixelSize: 16 } }
                    // 版本中心
                    Rectangle { color: "#1e1e2e"; Label { anchors.centerIn: parent; text: "版本中心 - 待实现"; color: "#6c7086"; font.pixelSize: 16 } }
                    // 导出中心
                    Rectangle { color: "#1e1e2e"; Label { anchors.centerIn: parent; text: "导出中心 - 待实现"; color: "#6c7086"; font.pixelSize: 16 } }
                }

                // 底部任务面板
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    color: "#181825"

                    Label {
                        anchors.centerIn: parent
                        text: "任务与日志面板 - 待实现"
                        color: "#6c7086"
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
