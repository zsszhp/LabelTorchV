// FilterBar.qml - 过滤栏（对标参考UI）
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme

Rectangle {
    id: root
    width: parent ? parent.width : 400
    height: Theme.filterBarHeight
    color: Theme.bgMain
    visible: false

    property alias datasetFilter: datasetCombo.currentText
    property alias tagFilter: tagCombo.currentText
    property alias labelFilter: labelCombo.currentText
    property alias datasetModel: datasetCombo.model
    property alias tagModel: tagCombo.model
    property alias labelModel: labelCombo.model
    // 样本 Tag 筛选（图像 Tag 体系：全部Tag/无Tag/各具体Tag）
    property alias sampleTagFilter: sampleTagCombo.currentText
    property alias sampleTagModel: sampleTagCombo.model
    property alias sampleTagIndex: sampleTagCombo.currentIndex
    // 文件名搜索（对标 DLTools 数据集页搜索框）
    property alias searchText: searchField.text
    // 搜索文本变化信号（页面据此过滤）
    signal searchTextChanged()

    Row {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingLarge
        anchors.rightMargin: Theme.spacingLarge
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.spacingNormal

        Rectangle {
            width: 160
            height: 28
            color: Theme.bgSide
            border.color: Theme.borderColor
            border.width: 1
            radius: Theme.radiusNormal

            ComboBox {
                id: datasetCombo
                anchors.fill: parent
                model: ["全部数据集"]
                currentIndex: 0
                font.pixelSize: Theme.fontSizeSmall
                palette.button: Theme.bgSide
                palette.text: Theme.textMain
                palette.buttonText: Theme.textMuted
            }
        }

        Rectangle {
            width: 120
            height: 28
            color: Theme.bgSide
            border.color: Theme.borderColor
            border.width: 1
            radius: Theme.radiusNormal

            ComboBox {
                id: tagCombo
                anchors.fill: parent
                model: ["全部状态"]
                currentIndex: 0
                font.pixelSize: Theme.fontSizeSmall
                palette.button: Theme.bgSide
                palette.text: Theme.textMain
                palette.buttonText: Theme.textMuted
            }
        }

        Rectangle {
            width: 120
            height: 28
            color: Theme.bgSide
            border.color: Theme.borderColor
            border.width: 1
            radius: Theme.radiusNormal

            ComboBox {
                id: sampleTagCombo
                anchors.fill: parent
                model: ["全部Tag"]
                currentIndex: 0
                font.pixelSize: Theme.fontSizeSmall
                palette.button: Theme.bgSide
                palette.text: Theme.textMain
                palette.buttonText: Theme.textMuted
            }
        }

        Rectangle {
            width: 120
            height: 28
            color: Theme.bgSide
            border.color: Theme.borderColor
            border.width: 1
            radius: Theme.radiusNormal

            ComboBox {
                id: labelCombo
                anchors.fill: parent
                model: ["全部划分"]
                currentIndex: 0
                font.pixelSize: Theme.fontSizeSmall
                palette.button: Theme.bgSide
                palette.text: Theme.textMain
                palette.buttonText: Theme.textMuted
            }
        }

        // 文件名搜索框（输入文件名子串筛选图像）
        Rectangle {
            width: 200
            height: 28
            color: Theme.bgInput
            border.color: searchField.activeFocus ? Theme.primaryGlow : Theme.borderColor
            border.width: 1
            radius: Theme.radiusNormal

            Row {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: Theme.spacingSmall

                // 经 qrc 加载 Shell 的 SvgIcon（避免 components→shell 模块循环依赖）
                Loader {
                    id: searchLoader
                    anchors.verticalCenter: parent.verticalCenter
                    width: 13
                    height: 13
                    source: "qrc:/qt/qml/LabelTorch/Shell/qml/SvgIcon.qml"
                    onLoaded: {
                        item.icon = "search"
                        item.color = Qt.binding(function() {
                            return searchField.text.length > 0 ? Theme.primary : "transparent"
                        })
                    }
                }

                TextField {
                    id: searchField
                    width: parent.width - 20
                    anchors.verticalCenter: parent.verticalCenter
                    placeholderText: "输入文本筛选图像"
                    placeholderTextColor: Theme.textDisabled
                    color: Theme.textMain
                    font.pixelSize: Theme.fontSizeSmall
                    font.family: Theme.fontFamily
                    selectByMouse: true
                    background: null
                    onTextChanged: root.searchTextChanged()
                }
            }
        }
    }
}
