// CollapsibleSection.qml - 可折叠区块（对标参考UI）
// 布局约定（重要）：
//   1. 本组件高度由 `implicitHeight`（头部 + 内容实测高）驱动，**不要**在外层给本组件设置
//      Layout.fillHeight。当宿主是 ScrollView / Flickable 的内容时，高度约束是无限大，
//      fillHeight 无解会被解成 0，整块塌陷不可见。需要固定高度时改设 Layout.preferredHeight。
//   2. 折叠动画依赖 contentArea.height，因此 contentArea 使用 clip 精确裁切。
import QtQuick
import QtQuick.Controls
import LabelTorch.Theme

Item {
    id: root
    width: parent ? parent.width : 200
    implicitHeight: column.height

    property string title: ""
    property bool expanded: true
    // 头部固定高度（供外层计算总高时复用，避免魔法数字漂移）
    readonly property int headerHeight: 36
    property alias content: contentArea.children
    default property alias contentData: contentArea.data

    Column {
        id: column
        width: parent.width
        spacing: 0

        Rectangle {
            width: parent.width
            height: root.headerHeight
            color: Qt.rgba(1, 1, 1, 0.01)

            Row {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 10
                anchors.bottomMargin: 10
                spacing: Theme.spacingSmall

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.expanded ? "▾" : "▸"
                    font.pixelSize: 12
                    color: Theme.textMuted
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    // 头部文字宽度收敛：Row 不会自动换行，标题过长会溢出到父项之外
                    width: Math.max(0, parent.width - 28)
                    text: root.title
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: Theme.textMuted
                }
            }

            // 底部分割线
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.borderColor
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.expanded = !root.expanded
            }
        }

        Item {
            id: contentArea
            width: parent.width
            // 内容隐式高度：优先用子项声明的 implicitHeight（如内部 ColumnLayout 的
            // implicitHeight），避免仅用 childrenRect.height 在布局尚未求解时把高度锁死为 0。
            readonly property real contentImplicitHeight: {
                if (children.length === 0)
                    return 0
                var h = childrenRect.height
                for (var i = 0; i < children.length; i++) {
                    var c = children[i]
                    if (c && c.visible !== false && isFinite(c.implicitHeight))
                        h = Math.max(h, c.implicitHeight + c.y)
                }
                return isFinite(h) && h > 0 ? h : 0
            }
            height: root.expanded ? contentImplicitHeight : 0
            visible: root.expanded
            clip: true
            Behavior on height { NumberAnimation { duration: Theme.animDuration; easing.type: Easing.InOutQuad } }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.dividerColor
            visible: !root.expanded
        }
    }
}
