// FormField.qml - 表单字段容器：定宽标签 + 内容槽 + 内联错误文案
// 标签列宽 96，多字段纵排时标签右对齐成线；错误态统一红框 + 底部红字
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
    // 错误态（可由外部强制，与 errorText 非空等效）
    property bool hasError: errorText !== ""

    // 内容槽
    default property alias contentData: contentArea.data

    implicitWidth: 320
    implicitHeight: fieldRow.implicitHeight

    RowLayout {
        id: fieldRow
        anchors.fill: parent
        spacing: Theme.spacingMedium

        // 标签列：固定 96，保证表单纵向对齐
        RowLayout {
            Layout.preferredWidth: Theme.formLabelWidth
            Layout.maximumWidth: Theme.formLabelWidth
            Layout.alignment: Qt.AlignTop | Qt.AlignRight
            spacing: Theme.spacingTiny

            Text {
                Layout.fillWidth: true
                text: root.label
                color: root.fieldEnabled ? Theme.textSecondary : Theme.textDisabled
                font.pixelSize: Theme.fontSizeSmall
                font.family: Theme.fontFamily
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }

            Text {
                visible: root.required
                text: "*"
                color: Theme.danger
                font.pixelSize: Theme.fontSizeSmall
                font.family: Theme.fontFamily
            }
        }

        // 内容列：错误态统一红框 + 底部错误文案
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSmall

            Rectangle {
                id: errorFrame
                Layout.fillWidth: true
                Layout.preferredHeight: contentArea.implicitHeight > 0 ? contentArea.implicitHeight : 32
                radius: Theme.radiusSmall
                color: "transparent"
                // 错误态统一：1px 危险色描边，无态时完全透明不占位
                border.color: root.hasError ? Theme.fieldErrorBorder : "transparent"
                border.width: root.hasError ? 1 : 0

                Item {
                    id: contentArea
                    anchors.fill: parent
                    anchors.margins: root.hasError ? 1 : 0
                }
            }

            // 底部错误文案（统一字号/颜色，仅错误态可见）
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
}
