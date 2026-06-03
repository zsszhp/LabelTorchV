// ParamRow.qml - 参数行（标签+控件，对标参考UI）
import QtQuick
import LabelTorch.Theme

Row {
    id: root
    spacing: Theme.spacingNormal
    height: 28

    property string label: ""
    property int labelWidth: 120
    property alias control: controlArea.children

    Text {
        width: root.labelWidth
        height: parent.height
        verticalAlignment: Text.AlignVCenter
        text: root.label
        font.pixelSize: Theme.fontSizeSmall
        color: Theme.textMuted
        elide: Text.ElideRight
    }

    Item {
        id: controlArea
        width: root.width - root.labelWidth - root.spacing
        height: parent.height
    }
}
