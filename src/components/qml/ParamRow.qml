// ParamRow.qml - 参数行（标签+控件，对标参考UI）
import QtQuick
import QtQuick.Layouts
import LabelTorch.Theme

RowLayout {
    id: root
    spacing: Theme.spacingNormal
    Layout.fillWidth: true
    implicitHeight: 28

    property string label: ""
    property int labelWidth: 100
    property alias control: controlArea.children

    Text {
        Layout.preferredWidth: root.labelWidth
        Layout.fillWidth: false
        Layout.fillHeight: true
        verticalAlignment: Text.AlignVCenter
        text: root.label
        font.pixelSize: Theme.fontSizeSmall
        color: Theme.textMuted
        elide: Text.ElideRight
    }

    Item {
        id: controlArea
        Layout.fillWidth: true
        Layout.fillHeight: true

        onChildrenChanged: {
            for (var i = 0; i < children.length; i++) {
                var child = children[i];
                if (child && child.anchors) {
                    if (child.hasOwnProperty("model") || child.hasOwnProperty("placeholderText") || child.hasOwnProperty("textRole")) {
                        child.anchors.fill = controlArea;
                    } else if (!child.anchors.fill && !child.anchors.right && !child.anchors.left) {
                        child.anchors.right = controlArea.right;
                        child.anchors.verticalCenter = controlArea.verticalCenter;
                    }
                }
            }
        }
    }
}
