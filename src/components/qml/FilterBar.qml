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
                font.pixelSize: Theme.fontSizeSmall
                palette.button: Theme.bgSide
                palette.text: Theme.textMain
                palette.buttonText: Theme.textSecondary
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
                model: ["全部Tag"]
                font.pixelSize: Theme.fontSizeSmall
                palette.button: Theme.bgSide
                palette.text: Theme.textMain
                palette.buttonText: Theme.textSecondary
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
                model: ["全部类别"]
                font.pixelSize: Theme.fontSizeSmall
                palette.button: Theme.bgSide
                palette.text: Theme.textMain
                palette.buttonText: Theme.textSecondary
            }
        }
    }
}
