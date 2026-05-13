pragma Singleton
import QtQuick

QtObject {
    // 背景色系
    readonly property color bgPrimary: "#1a1a2e"
    readonly property color bgSecondary: "#16213e"
    readonly property color bgTertiary: "#0f3460"
    readonly property color bgCard: "#16213e"
    readonly property color bgHover: "#0f3460"
    readonly property color bgSelected: "#0f3460"
    readonly property color bgInput: "#0f3460"

    // 强调色
    readonly property color accentPrimary: "#e94560"
    readonly property color accentSecondary: "#533483"
    readonly property color accentSuccess: "#4caf50"
    readonly property color accentWarning: "#ff9800"
    readonly property color accentError: "#f44336"
    readonly property color accentPurple: "#533483"

    // 文字色
    readonly property color textPrimary: "#e0e0e0"
    readonly property color textSecondary: "#a0a0a0"
    readonly property color textDisabled: "#606060"
    readonly property color textMuted: "#606060"

    // 状态色
    readonly property color statusSuccess: "#4caf50"
    readonly property color statusWarning: "#ff9800"
    readonly property color statusError: "#f44336"
    readonly property color statusInfo: "#2196f3"

    // 边框与分割线
    readonly property color border: "#2a2a4a"
    readonly property color borderNormal: "#2a2a4a"
    readonly property color borderFocus: "#e94560"
    readonly property color divider: "#252545"

    readonly property color tagBaseline: "#e94560"
    readonly property color tagBest: "#4caf50"
    readonly property color tagProduction: "#ff9800"

    // 字体
    readonly property string fontFamily: "微软雅黑"
    readonly property string fontFamilyMono: "Consolas, Courier New, monospace"
    readonly property int fontSizeSmall: 11
    readonly property int fontSizeNormal: 13
    readonly property int fontSizeLarge: 16
    readonly property int fontSizeTitle: 20
    readonly property int fontSizeDisplay: 24

    // 间距
    readonly property int spacingTiny: 2
    readonly property int spacingSmall: 4
    readonly property int spacingNormal: 8
    readonly property int spacingLarge: 16
    readonly property int spacingXLarge: 24

    // 圆角
    readonly property int radiusSmall: 4
    readonly property int radiusNormal: 8
    readonly property int radiusLarge: 12

    // 动画
    readonly property int animDuration: 200

    readonly property int navWidth: 220
    readonly property int statusBarHeight: 40
    readonly property int logPanelHeight: 160
    readonly property int toolbarHeight: 36

    readonly property var classColors: [
        "#f38ba8", "#a6e3a1", "#89b4fa", "#f9e2af",
        "#fab387", "#94e2d5", "#cba6f7", "#f5c2e7",
        "#89dceb", "#b4befe"
    ]

    function classColor(index) {
        return classColors[index % classColors.length]
    }
}
