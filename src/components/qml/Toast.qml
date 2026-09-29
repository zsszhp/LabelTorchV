// Toast.qml - 轻提示视觉层（挂在 Main.qml 顶层，接收 ToastBus 通知）
// 实底卡片 + 单强调色条 + 双层阴影；成功提示 2s 自动消失，错误常驻可关
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import LabelTorch.Theme

Item {
    id: root
    // 允许鼠标穿透空白区域，只挡住提示卡片本身
    enabled: false

    property var toasts: []

    function pushToast(message, kind, durationMs) {
        var t = {
            text: message === undefined ? "" : String(message),
            kind: kind === undefined ? "info" : kind,
            duration: durationMs === undefined || durationMs === null ? Theme.toastSuccessDuration : durationMs
        }
        var list = root.toasts.slice()
        list.push(t)
        // 超出上限时丢弃最早的，避免刷屏
        while (list.length > Theme.toastMaxVisible) {
            list.shift()
        }
        root.toasts = list
        toastRepeater.model = root.toasts.length
    }

    function dismissAt(index) {
        var list = root.toasts.slice()
        if (index < 0 || index >= list.length)
            return
        list.splice(index, 1)
        root.toasts = list
        toastRepeater.model = root.toasts.length
    }

    // 取当前条目的语义色（强调条/图标用）
    function toneColor(kind) {
        if (kind === "success")
            return Theme.success
        if (kind === "error")
            return Theme.danger
        return Theme.primary
    }

    Connections {
        target: ToastBus
        function onNotify(message, kind, durationMs) {
            root.pushToast(message, kind, durationMs)
        }
    }

    // 提示卡片垂直堆叠，锚定在内容区右上方
    Column {
        id: toastColumn
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Theme.spacingXLarge + Theme.headerHeight
        anchors.rightMargin: Theme.spacingXLarge
        spacing: Theme.spacingNormal
        width: Theme.toastWidth

        Repeater {
            id: toastRepeater
            model: 0

            delegate: Item {
                id: toastWrap
                required property int index
                width: toastColumn.width
                height: toastCard.height

                readonly property var toastData: root.toasts[index] ? root.toasts[index] : null
                readonly property string kind: toastData ? toastData.kind : "info"
                readonly property color accent: root.toneColor(kind)

                // 双层阴影远影（外扩淡影，拉出悬浮感）
                Rectangle {
                    anchors.fill: toastCard
                    radius: toastCard.radius
                    color: Theme.elevationFarColor
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: Theme.elevationFarColor
                        shadowBlur: Theme.elevationFarBlur
                        shadowVerticalOffset: Theme.elevationFarOffsetY
                        shadowHorizontalOffset: 0
                    }
                }

                // 双层阴影近影（贴近深影，压住边界）
                Rectangle {
                    anchors.fill: toastCard
                    radius: toastCard.radius
                    color: Theme.elevationNearColor
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: Theme.elevationNearColor
                        shadowBlur: Theme.elevationNearBlur
                        shadowVerticalOffset: Theme.elevationNearOffsetY
                        shadowHorizontalOffset: 0
                    }
                }

                Rectangle {
                    id: toastCard
                    width: toastWrap.width
                    height: toastBody.implicitHeight + Theme.spacingLarge * 2
                    radius: Theme.radiusNormal
                    // 实底卡片底色，语义只靠左侧强调条表达
                    color: Theme.bgCard
                    border.color: Theme.borderColor
                    border.width: 1
                    // 卡片本身可交互（关闭按钮），需开启命中测试
                    enabled: true

                    // 进场淡入
                    opacity: 0
                    Component.onCompleted: fadeIn.start()

                    NumberAnimation {
                        id: fadeIn
                        target: toastCard
                        property: "opacity"
                        from: 0
                        to: 1
                        duration: Theme.animDuration
                        easing.type: Easing.OutCubic
                    }

                    // 成功/信息提示到时自动消失；错误提示 duration<=0 常驻
                    Timer {
                        id: autoClose
                        interval: {
                            var d = toastWrap.toastData ? toastWrap.toastData.duration : Theme.toastSuccessDuration
                            return d > 0 ? d : 0
                        }
                        running: interval > 0
                        repeat: false
                        onTriggered: root.dismissAt(toastWrap.index)
                    }

                    RowLayout {
                        id: toastBody
                        anchors.fill: parent
                        anchors.margins: Theme.spacingLarge
                        spacing: Theme.spacingMedium

                        // 单强调色条：整条高度、左置、按语义着色
                        Rectangle {
                            Layout.preferredWidth: 3
                            Layout.fillHeight: true
                            radius: 2
                            color: toastWrap.accent
                        }

                        Text {
                            Layout.fillWidth: true
                            text: toastWrap.toastData ? toastWrap.toastData.text : ""
                            color: Theme.textMain
                            font.pixelSize: Theme.fontSizeNormal
                            font.family: Theme.fontFamily
                            wrapMode: Text.WordWrap
                            maximumLineCount: 4
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }

                        // 关闭按钮：SvgIcon 关闭图标，悬停变色
                        Rectangle {
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22
                            radius: Theme.radiusSmall
                            color: closeMouse.containsMouse ? Theme.bgHover : "transparent"

                            Loader {
                                anchors.centerIn: parent
                                width: 12
                                height: 12
                                source: "qrc:/qt/qml/LabelTorch/Shell/qml/SvgIcon.qml"
                                onLoaded: {
                                    if (item) {
                                        item.icon = "close"
                                        item.color = Qt.binding(function () {
                                            return closeMouse.containsMouse ? Theme.textMain : Theme.textMuted
                                        })
                                        item.width = 12
                                        item.height = 12
                                    }
                                }
                            }

                            MouseArea {
                                id: closeMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.dismissAt(toastWrap.index)
                            }
                        }
                    }
                }
            }
        }
    }
}
