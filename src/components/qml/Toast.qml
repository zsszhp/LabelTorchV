// Toast.qml - 轻提示视觉层（挂在 Main.qml 顶层，接收 ToastBus 通知）
// 成功提示 2s 自动消失；错误提示常驻可手动关闭；同时最多展示 Theme.toastMaxVisible 条
import QtQuick
import QtQuick.Controls
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

            delegate: Rectangle {
                id: toastCard
                required property int index
                width: toastColumn.width
                height: toastBody.implicitHeight + Theme.spacingLarge * 2
                radius: Theme.radiusLarge
                color: {
                    var k = root.toasts[index] ? root.toasts[index].kind : "info"
                    if (k === "success")
                        return Theme.toastSuccessBg
                    if (k === "error")
                        return Theme.toastErrorBg
                    return Theme.toastInfoBg
                }
                border.color: {
                    var k = root.toasts[index] ? root.toasts[index].kind : "info"
                    if (k === "success")
                        return Theme.success
                    if (k === "error")
                        return Theme.danger
                    return Theme.primaryGlow
                }
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
                        var d = root.toasts[index] ? root.toasts[index].duration : Theme.toastSuccessDuration
                        return d > 0 ? d : 0
                    }
                    running: interval > 0
                    repeat: false
                    onTriggered: root.dismissAt(toastCard.index)
                }

                RowLayout {
                    id: toastBody
                    anchors.fill: parent
                    anchors.margins: Theme.spacingLarge
                    spacing: Theme.spacingNormal

                    // 左侧状态色条
                    Rectangle {
                        Layout.preferredWidth: 3
                        Layout.fillHeight: true
                        radius: 2
                        color: {
                            var k = root.toasts[index] ? root.toasts[index].kind : "info"
                            if (k === "success")
                                return Theme.success
                            if (k === "error")
                                return Theme.danger
                            return Theme.primaryGlow
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.toasts[index] ? root.toasts[index].text : ""
                        color: Theme.textMain
                        font.pixelSize: Theme.fontSizeNormal
                        font.family: Theme.fontFamily
                        wrapMode: Text.WordWrap
                        maximumLineCount: 4
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }

                    // 关闭按钮：错误提示常驻，必须可关
                    Rectangle {
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 22
                        radius: Theme.radiusSmall
                        color: closeMouse.containsMouse ? Theme.bgHover : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            color: closeMouse.containsMouse ? Theme.textMain : Theme.textMuted
                            font.pixelSize: Theme.fontSizeNormal
                            font.family: Theme.fontFamily
                        }

                        MouseArea {
                            id: closeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.dismissAt(toastCard.index)
                        }
                    }
                }
            }
        }
    }
}
