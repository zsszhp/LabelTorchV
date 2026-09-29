// ToastBus.qml - 全局轻提示总线（单例）
// 页面通过 ToastBus.success/error/info 发提示，Main.qml 挂载的 Toast 视觉层负责渲染
// 成功提示 2s 自动消失；错误提示常驻，需手动关闭
pragma Singleton
import QtQuick
import LabelTorch.Theme

QtObject {
    id: bus

    // kind: "success" | "error" | "info"
    // durationMs <= 0 表示常驻不自动消失
    signal notify(string message, string kind, int durationMs)

    function success(message) {
        notify(message, "success", Theme.toastSuccessDuration)
    }

    function error(message) {
        // 错误常驻可关：durationMs 传 0
        notify(message, "error", Theme.toastErrorDuration)
    }

    function info(message) {
        notify(message, "info", Theme.toastSuccessDuration)
    }

    // 兜底入口：kind 可省略，durationMs 可覆盖
    function show(message, kind, durationMs) {
        var k = kind === undefined || kind === "" ? "info" : kind
        var d = durationMs
        if (d === undefined || d === null) {
            d = k === "error" ? Theme.toastErrorDuration : Theme.toastSuccessDuration
        }
        notify(message, k, d)
    }
}
