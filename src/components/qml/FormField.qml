// FormField.qml - 表单字段容器：标签 + 内容槽 + 内联错误文案
// 字段错误时红框 + 底部红字提示，供训练/导出等表单复用
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LabelTorch.Theme

Item {
    id: root

    // 字段标签
    property string label: ""
    // 错误文案；非空即进入错误态
    property string errorText: ""
    // 是否必填（标签旁红星）
    property bool required: false
    // 是否禁用
    property bool fieldEnabled: true
    // 错误态（可由外部强制）
    property bool hasError: errorText !== ""

    // 内容槽
    default property alias contentData: contentArea.data

    implicitWidth: 240
    implicitHeight: fieldColumn.implicitHeight

    ColumnLayout {
        id: fieldColumn
        anchors.fill: parent
        spacing: Theme.spacingSmall

        // 标签行
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingTiny

            Text {
                text: root.label
                color: root.fieldEnabled ? Theme.textSecondary : Theme.textDisabled
                font.pixelSize: Theme.fontSizeSmall
                font.family: Theme.fontFamily
            }

            Text {
                visible: root.required
                text: "*"
                color: Theme.danger
                font.pixelSize: Theme.fontSizeSmall
                font.family: Theme.fontFamily
            }

            Item { Layout.fillWidth: true }
        }

        // 内容槽：错误态时整块加红框
        Rectangle {
            id: errorFrame
            Layout.fillWidth: true
            Layout.preferredHeight: contentArea.implicitHeight > 0 ? contentArea.implicitHeight : 32
            radius: Theme.radiusSmall
            color: "transparent"
            border.color: root.hasError ? Theme.fieldErrorBorder : "transparent"
            border.width: root.hasError ? 1 : 0

            Item {
                id: contentArea
                anchors.fill: parent
                anchors.margins: root.hasError ? 1 : 0
            }
        }

        // 底部错误文案
        Text {
            Layout.fillWidth: true
            visible: root.hasError && root.errorText !== ""
            text: root.errorText
            color: Theme.fieldErrorText
            font.pixelSize: Theme.fontSizeCaption
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }
    }
}
